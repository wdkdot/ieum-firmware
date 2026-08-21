#include "BQ25628E.h"

#ifdef HAS_BQ25628E

#include "FSCommon.h"
#include "SPILock.h"
#include "SafeFile.h"
#include "concurrency/LockGuard.h"
#include "mesh/Throttle.h"
#include <Arduino.h>

#include <atomic>
#include <stddef.h>

namespace
{
constexpr uint8_t REG_CHARGE_CURRENT_LIMIT = 0x02;
constexpr uint8_t REG_CHARGE_VOLTAGE_LIMIT = 0x04;
constexpr uint8_t REG_INPUT_CURRENT_LIMIT = 0x06;
constexpr uint8_t REG_CHARGER_CONTROL0 = 0x16;
constexpr uint8_t REG_CHARGER_CONTROL1 = 0x17;
constexpr uint8_t REG_CHARGER_CONTROL2 = 0x18;
constexpr uint8_t REG_CHARGER_CONTROL3 = 0x19;
constexpr uint8_t REG_CHARGER_STATUS0 = 0x1D;
constexpr uint8_t REG_CHARGER_STATUS1 = 0x1E;
constexpr uint8_t REG_FAULT_STATUS = 0x1F;
constexpr uint8_t REG_CHARGER_FLAG0 = 0x20;
constexpr uint8_t REG_CHARGER_FLAG1 = 0x21;
constexpr uint8_t REG_FAULT_FLAG = 0x22;
constexpr uint8_t REG_CHARGER_MASK0 = 0x23;
constexpr uint8_t REG_ADC_CONTROL = 0x26;
constexpr uint8_t REG_ADC_FUNCTION_DISABLE = 0x27;
constexpr uint8_t REG_IBUS_ADC = 0x28;
constexpr uint8_t REG_IBAT_ADC = 0x2A;
constexpr uint8_t REG_VBUS_ADC = 0x2C;
constexpr uint8_t REG_VPMID_ADC = 0x2E;
constexpr uint8_t REG_VBAT_ADC = 0x30;
constexpr uint8_t REG_VSYS_ADC = 0x32;
constexpr uint8_t REG_TS_ADC = 0x34;
constexpr uint8_t REG_TDIE_ADC = 0x36;
constexpr uint8_t REG_PART_INFORMATION = 0x38;

constexpr uint16_t ICHG_MASK = 0x07E0;
constexpr uint16_t VREG_MASK = 0x0FF8;
constexpr uint16_t IINDPM_MASK = 0x0FF0;
constexpr uint8_t CHARGE_ENABLE_MASK = 0x20;
constexpr uint8_t WATCHDOG_RESET_MASK = 0x04;
constexpr uint8_t WATCHDOG_MASK = 0x03;
constexpr uint8_t INPUT_OVP_MASK = 0x01;
constexpr uint8_t BATFET_DELAY_MASK = 0x04;
constexpr uint8_t BATFET_CONTROL_MASK = 0x03;
constexpr uint8_t BATFET_SHUTDOWN = 0x01;
constexpr uint8_t BATFET_SHIP = 0x02;
constexpr uint8_t EXTERNAL_ILIM_ENABLE_MASK = 0x04;
constexpr uint8_t ADC_DONE_MASK = 0x40;
constexpr uint8_t ADC_ONE_SHOT_9_BIT = 0xF0;
constexpr uint8_t ADC_ENABLE_MASK = 0x80;
constexpr uint8_t ADC_CONTROL_MASK = 0xFC;
constexpr uint8_t TS_ADC_DISABLE_MASK = 0x04;
constexpr uint8_t EXPECTED_PART_NUMBER = 4;
constexpr uint8_t PART_NUMBER_MASK = 0x38;
constexpr uint8_t PART_NUMBER_SHIFT = 3;
constexpr uint8_t REVISION_MASK = 0x07;
constexpr uint8_t I2C_START_GAP_US = 100;
constexpr uint16_t ADC_TIMEOUT_MS = 150;
constexpr uint8_t MAX_CONSECUTIVE_MEASUREMENT_FAILURES = 3;
constexpr char SETTINGS_FILE[] = "/prefs/bq25628e.dat";
constexpr uint32_t SETTINGS_MAGIC = 0x42513238U; // "BQ28"
constexpr uint16_t SETTINGS_VERSION = 1;
constexpr uint16_t BATTERY_CARE_VOLTAGE_MV = 4000;
constexpr uint16_t FULL_CHARGE_VOLTAGE_MV = 4200;

struct SettingsRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t chargeVoltageLimitMv;
    uint32_t checksum;
};

static_assert(offsetof(SettingsRecord, checksum) == 8U);
static_assert(sizeof(SettingsRecord) == 12U);

std::atomic<uint16_t> chargeVoltageLimitMv{BQ25628E_CHARGE_VOLTAGE_LIMIT_MV};
std::atomic<uint16_t> pendingChargeVoltageLimitMv{BQ25628E_CHARGE_VOLTAGE_LIMIT_MV};
std::atomic<bool> chargeVoltageRequestPending{false};
BQ25628EWakeCallback wakeCallback = nullptr;
bool initializationFailed = false;
bool communicationFailed = false;
bool measurementFailed = false;
bool vbusKnown = false;
bool previousVbus = false;
uint8_t previousFaultStatus = 0;

constexpr uint16_t encodeChargeCurrent(uint16_t currentMa)
{
    return static_cast<uint16_t>((currentMa / 40U) << 5);
}

constexpr uint16_t encodeChargeVoltage(uint16_t voltageMv)
{
    return static_cast<uint16_t>((voltageMv / 10U) << 3);
}

constexpr uint16_t encodeInputCurrent(uint16_t currentMa)
{
    return static_cast<uint16_t>((currentMa / 20U) << 4);
}

constexpr uint8_t encodeWatchdog(uint16_t seconds)
{
    return seconds == 0U ? 0U : seconds == 50U ? 1U : seconds == 100U ? 2U : 3U;
}

constexpr uint8_t encodeInputOvp(uint16_t voltageMv)
{
    return voltageMv == 18500U ? INPUT_OVP_MASK : 0U;
}

constexpr int16_t signExtend(uint16_t value, uint8_t bits)
{
    const uint16_t signBit = static_cast<uint16_t>(1U << (bits - 1U));
    return (value & signBit) != 0U ? static_cast<int16_t>(static_cast<int32_t>(value) - (1L << bits))
                                   : static_cast<int16_t>(value);
}

constexpr uint16_t scaleRounded(uint16_t value, uint16_t numerator)
{
    return static_cast<uint16_t>((static_cast<uint32_t>(value) * numerator + 50U) / 100U);
}

static_assert(encodeChargeCurrent(320) == 0x0100);
static_assert(encodeChargeVoltage(4000) == 0x0C80);
static_assert(encodeChargeVoltage(4200) == 0x0D20);
static_assert(encodeInputCurrent(500) == 0x0190);
static_assert(encodeWatchdog(0) == 0x00);
static_assert(signExtend(0x3FFF, 14) == -1);
static_assert(signExtend(0x1FFF, 14) == 8191);
static_assert(scaleRounded(0x04EB, 397) == 4998);
static_assert(scaleRounded(0x07CF, 199) == 3978);

bool isSupportedChargeVoltage(uint16_t voltageMv)
{
    return voltageMv == BATTERY_CARE_VOLTAGE_MV || voltageMv == FULL_CHARGE_VOLTAGE_MV;
}

uint32_t calculateSettingsChecksum(const SettingsRecord &record)
{
    uint32_t checksum = 2166136261U;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    for (size_t i = 0; i < offsetof(SettingsRecord, checksum); i++) {
        checksum ^= bytes[i];
        checksum *= 16777619U;
    }
    return checksum;
}

bool loadChargeVoltageLimit(uint16_t &voltageMv)
{
#ifdef FSCom
    SettingsRecord record = {};

    concurrency::LockGuard guard(spiLock);
    auto file = FSCom.open(SETTINGS_FILE, FILE_O_READ);
    if (!file) {
        LOG_INFO("No BQ25628E settings found; using %umV", voltageMv);
        return false;
    }

    const size_t bytesRead = file.read(reinterpret_cast<uint8_t *>(&record), sizeof(record));
    const size_t fileSize = file.size();
    file.close();

    const bool valid = bytesRead == sizeof(record) && fileSize == sizeof(record) && record.magic == SETTINGS_MAGIC &&
                       record.version == SETTINGS_VERSION && isSupportedChargeVoltage(record.chargeVoltageLimitMv) &&
                       record.checksum == calculateSettingsChecksum(record);
    if (!valid) {
        LOG_WARN("Invalid BQ25628E settings; using %umV", voltageMv);
        return false;
    }

    voltageMv = record.chargeVoltageLimitMv;
    LOG_INFO("Loaded BQ25628E charge voltage limit: %umV", voltageMv);
    return true;
#else
    LOG_WARN("BQ25628E settings require a filesystem; using %umV", voltageMv);
    return false;
#endif
}

bool saveChargeVoltageLimit(uint16_t voltageMv)
{
    if (!isSupportedChargeVoltage(voltageMv)) {
        LOG_WARN("Refusing unsupported BQ25628E charge voltage: %umV", voltageMv);
        return false;
    }

#ifdef FSCom
    SettingsRecord record = {SETTINGS_MAGIC, SETTINGS_VERSION, voltageMv, 0};
    record.checksum = calculateSettingsChecksum(record);

    {
        concurrency::LockGuard guard(spiLock);
        FSCom.mkdir("/prefs");
    }

    auto file = SafeFile(SETTINGS_FILE, true);
    size_t bytesWritten = 0;
    {
        concurrency::LockGuard guard(spiLock);
        bytesWritten = file.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record));
    }

    const bool closeSucceeded = file.close();
    const bool writeSucceeded = bytesWritten == sizeof(record) && closeSucceeded;
    uint16_t verifiedVoltageMv = 0;
    if (!writeSucceeded || !loadChargeVoltageLimit(verifiedVoltageMv) || verifiedVoltageMv != voltageMv) {
        LOG_WARN("Failed to save BQ25628E charge voltage limit");
        return false;
    }

    LOG_INFO("Saved BQ25628E charge voltage limit: %umV", voltageMv);
    return true;
#else
    LOG_WARN("BQ25628E settings require a filesystem");
    return false;
#endif
}

BQ25628E::Configuration makeConfiguration()
{
    BQ25628E::Configuration configuration;
    configuration.chargeVoltageLimitMv = chargeVoltageLimitMv.load();
    return configuration;
}

void handleInterrupt()
{
    if (bq25628e != nullptr) {
        bq25628e->notifyInterrupt();
    }
    if (wakeCallback != nullptr) {
        wakeCallback();
    }
}
} // namespace

BQ25628E *bq25628e = nullptr;

bool initBQ25628E(TwoWire &wire)
{
    return initBQ25628E(wire, BQ25628E::Configuration{});
}

bool initBQ25628E(TwoWire &wire, const BQ25628E::Configuration &configuration)
{
    if (bq25628e != nullptr) {
        return true;
    }

    bq25628e = new BQ25628E();
    if (!bq25628e->begin(wire, BQ25628E_ADDR, configuration)) {
        delete bq25628e;
        bq25628e = nullptr;
        return false;
    }
    return true;
}

bool setupBQ25628E(BQ25628EWakeCallback callback)
{
    wakeCallback = callback;

    uint16_t configuredVoltageMv = BQ25628E_CHARGE_VOLTAGE_LIMIT_MV;
    loadChargeVoltageLimit(configuredVoltageMv);
    chargeVoltageLimitMv.store(configuredVoltageMv);
    pendingChargeVoltageLimitMv.store(configuredVoltageMv);

    const bool found = initBQ25628E(BQ25628E_WIRE, makeConfiguration());
    initializationFailed = !found;
    if (initializationFailed) {
        LOG_WARN("BQ25628E initialization failed; retrying from Power thread");
    }
    return found;
}

BQ25628EServiceResult serviceBQ25628E()
{
    BQ25628EServiceResult result;
    if (bq25628e == nullptr) {
        if (initBQ25628E(BQ25628E_WIRE, makeConfiguration())) {
            LOG_INFO("BQ25628E initialization recovered");
            initializationFailed = false;
            result.initializedNow = true;
            attachBQ25628EInterrupt();
        } else if (!initializationFailed) {
            LOG_WARN("BQ25628E initialization failed; retrying periodically");
            initializationFailed = true;
        }
    }

    if (bq25628e == nullptr) {
        return result;
    }

    if (chargeVoltageRequestPending.exchange(false)) {
        const uint16_t requestedVoltageMv = pendingChargeVoltageLimitMv.load();
        if (!bq25628e->setChargeVoltageLimit(requestedVoltageMv)) {
            chargeVoltageLimitMv.store(bq25628e->configuration().chargeVoltageLimitMv);
            LOG_WARN("BQ25628E failed to apply %umV charge voltage limit", requestedVoltageMv);
        } else if (!saveChargeVoltageLimit(requestedVoltageMv)) {
            LOG_WARN("BQ25628E charge voltage changed to %umV but was not persisted", requestedVoltageMv);
        } else {
            LOG_INFO("BQ25628E charge voltage limit changed to %umV", requestedVoltageMv);
        }
    }

    if (!bq25628e->refreshStatus()) {
        if (!communicationFailed) {
            LOG_WARN("BQ25628E status read failed; retaining autonomous charger operation");
        }
        communicationFailed = true;
        return result;
    }

    if (communicationFailed) {
        LOG_INFO("BQ25628E communication restored");
    }
    communicationFailed = false;

    const bool currentVbus = bq25628e->hasInput();
    if (vbusKnown && currentVbus != previousVbus) {
        LOG_INFO("BQ25628E USB %s", currentVbus ? "connected" : "disconnected");
        result.inputChanged = true;
        result.inputPresent = currentVbus;
    }
    previousVbus = currentVbus;
    vbusKnown = true;

    const auto &flags = bq25628e->interruptFlags();
    if (flags.hasNonAdcEvent()) {
        LOG_DEBUG("BQ25628E flags: status0=0x%02x status1=0x%02x fault=0x%02x", flags.status0, flags.status1, flags.fault);
    }

    const uint8_t currentFaultStatus = bq25628e->status().faultStatus;
    if (currentFaultStatus != previousFaultStatus) {
        if (currentFaultStatus != 0U) {
            LOG_WARN("BQ25628E fault status changed to 0x%02x", currentFaultStatus);
        } else {
            LOG_INFO("BQ25628E fault status cleared");
        }
        previousFaultStatus = currentFaultStatus;
    }

    if (!bq25628e->updateMeasurements()) {
        if (!measurementFailed) {
            LOG_WARN("BQ25628E ADC conversion failed; charger status remains available");
        }
        measurementFailed = true;
    } else {
        if (measurementFailed) {
            LOG_INFO("BQ25628E ADC measurements restored");
        }
        measurementFailed = false;
    }
    return result;
}

void attachBQ25628EInterrupt()
{
#ifdef BQ25628E_INT_PIN
    if (bq25628e != nullptr) {
        pinMode(BQ25628E_INT_PIN, INPUT);
        attachInterrupt(BQ25628E_INT_PIN, handleInterrupt, BQ25628E_INT_ACTIVE == LOW ? FALLING : RISING);
    }
#endif
}

void detachBQ25628EInterrupt()
{
#ifdef BQ25628E_INT_PIN
    if (bq25628e != nullptr) {
        detachInterrupt(BQ25628E_INT_PIN);
    }
#endif
}

bool requestBQ25628EChargeVoltageLimit(uint16_t voltageMv)
{
    if (!isSupportedChargeVoltage(voltageMv) || bq25628e == nullptr || !bq25628e->isReady()) {
        return false;
    }

    pendingChargeVoltageLimitMv.store(voltageMv);
    chargeVoltageLimitMv.store(voltageMv);
    chargeVoltageRequestPending.store(true);
    if (wakeCallback != nullptr) {
        wakeCallback();
    }
    return true;
}

uint16_t getBQ25628EChargeVoltageLimit()
{
    return chargeVoltageLimitMv.load();
}

bool BQ25628E::readRegister8(uint8_t reg, uint8_t &value)
{
    wire_->beginTransmission(address_);
    wire_->write(reg);
    const uint8_t transmissionResult = wire_->endTransmission(false);
    delayMicroseconds(I2C_START_GAP_US);
    if (transmissionResult != 0U) {
        return false;
    }

    const size_t received = wire_->requestFrom(address_, static_cast<uint8_t>(1));
    delayMicroseconds(I2C_START_GAP_US);
    if (received != 1U || !wire_->available()) {
        while (wire_->available()) {
            wire_->read();
        }
        return false;
    }
    value = static_cast<uint8_t>(wire_->read());
    return true;
}

bool BQ25628E::readRegister16(uint8_t reg, uint16_t &value)
{
    wire_->beginTransmission(address_);
    wire_->write(reg);
    const uint8_t transmissionResult = wire_->endTransmission(false);
    delayMicroseconds(I2C_START_GAP_US);
    if (transmissionResult != 0U) {
        return false;
    }

    const size_t received = wire_->requestFrom(address_, static_cast<uint8_t>(2));
    delayMicroseconds(I2C_START_GAP_US);
    if (received != 2U || wire_->available() < 2) {
        while (wire_->available()) {
            wire_->read();
        }
        return false;
    }

    const uint8_t low = static_cast<uint8_t>(wire_->read());
    const uint8_t high = static_cast<uint8_t>(wire_->read());
    value = static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
    return true;
}

bool BQ25628E::writeRegister8(uint8_t reg, uint8_t value)
{
    wire_->beginTransmission(address_);
    wire_->write(reg);
    wire_->write(value);
    const uint8_t transmissionResult = wire_->endTransmission();
    delayMicroseconds(I2C_START_GAP_US);
    return transmissionResult == 0U;
}

bool BQ25628E::writeRegister16(uint8_t reg, uint16_t value)
{
    wire_->beginTransmission(address_);
    wire_->write(reg);
    wire_->write(static_cast<uint8_t>(value & 0xFFU));
    wire_->write(static_cast<uint8_t>(value >> 8));
    const uint8_t transmissionResult = wire_->endTransmission();
    delayMicroseconds(I2C_START_GAP_US);
    return transmissionResult == 0U;
}

bool BQ25628E::updateRegister8(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current = 0;
    if (!readRegister8(reg, current)) {
        return false;
    }
    const uint8_t updated = static_cast<uint8_t>((current & ~mask) | (value & mask));
    return updated == current || writeRegister8(reg, updated);
}

bool BQ25628E::updateRegister16(uint8_t reg, uint16_t mask, uint16_t value)
{
    uint16_t current = 0;
    if (!readRegister16(reg, current)) {
        return false;
    }
    const uint16_t updated = static_cast<uint16_t>((current & ~mask) | (value & mask));
    return updated == current || writeRegister16(reg, updated);
}

bool BQ25628E::validateConfiguration(const Configuration &configuration) const
{
    const bool inputCurrentValid = configuration.inputCurrentLimitMa >= 100U && configuration.inputCurrentLimitMa <= 3200U &&
                                   configuration.inputCurrentLimitMa % 20U == 0U;
    const bool chargeCurrentValid = configuration.chargeCurrentLimitMa >= 40U && configuration.chargeCurrentLimitMa <= 2000U &&
                                    configuration.chargeCurrentLimitMa % 40U == 0U;
    const bool chargeVoltageValid = configuration.chargeVoltageLimitMv >= 3500U && configuration.chargeVoltageLimitMv <= 4800U &&
                                    configuration.chargeVoltageLimitMv % 10U == 0U;
    const bool inputOvpValid =
        configuration.inputOvervoltageProtectionMv == 6300U || configuration.inputOvervoltageProtectionMv == 18500U;
    const bool watchdogValid = configuration.watchdogSeconds == 0U || configuration.watchdogSeconds == 50U ||
                               configuration.watchdogSeconds == 100U || configuration.watchdogSeconds == 200U;

    if (!inputCurrentValid || !chargeCurrentValid || !chargeVoltageValid || !inputOvpValid || !watchdogValid) {
        LOG_WARN("BQ25628E invalid configuration: input=%umA charge=%umA/%umV OVP=%umV watchdog=%us",
                 configuration.inputCurrentLimitMa, configuration.chargeCurrentLimitMa, configuration.chargeVoltageLimitMv,
                 configuration.inputOvervoltageProtectionMv, configuration.watchdogSeconds);
        return false;
    }
    return true;
}

bool BQ25628E::writeConfiguration(const Configuration &configuration)
{
    const uint8_t chargerControl0 = static_cast<uint8_t>((configuration.chargeEnabled ? CHARGE_ENABLE_MASK : 0U) |
                                                         encodeWatchdog(configuration.watchdogSeconds));
    const uint8_t externalIlim = configuration.externalInputCurrentLimitEnabled ? EXTERNAL_ILIM_ENABLE_MASK : 0U;

    // Any I2C write enters host mode and starts the POR watchdog, so configure it first.
    return updateRegister8(REG_CHARGER_CONTROL0, static_cast<uint8_t>(CHARGE_ENABLE_MASK | WATCHDOG_RESET_MASK | WATCHDOG_MASK),
                           chargerControl0) &&
           updateRegister16(REG_INPUT_CURRENT_LIMIT, IINDPM_MASK, encodeInputCurrent(configuration.inputCurrentLimitMa)) &&
           updateRegister16(REG_CHARGE_CURRENT_LIMIT, ICHG_MASK, encodeChargeCurrent(configuration.chargeCurrentLimitMa)) &&
           updateRegister16(REG_CHARGE_VOLTAGE_LIMIT, VREG_MASK, encodeChargeVoltage(configuration.chargeVoltageLimitMv)) &&
           updateRegister8(REG_CHARGER_CONTROL1, INPUT_OVP_MASK, encodeInputOvp(configuration.inputOvervoltageProtectionMv)) &&
           updateRegister8(REG_CHARGER_CONTROL3, EXTERNAL_ILIM_ENABLE_MASK, externalIlim) &&
           updateRegister8(REG_CHARGER_MASK0, ADC_DONE_MASK, ADC_DONE_MASK);
}

bool BQ25628E::verifyConfiguration(const Configuration &configuration)
{
    uint16_t inputCurrent = 0;
    uint16_t chargeCurrent = 0;
    uint16_t chargeVoltage = 0;
    uint8_t chargerControl0 = 0;
    uint8_t chargerControl1 = 0;
    uint8_t chargerControl3 = 0;
    uint8_t chargerMask0 = 0;

    if (!readRegister16(REG_INPUT_CURRENT_LIMIT, inputCurrent) || !readRegister16(REG_CHARGE_CURRENT_LIMIT, chargeCurrent) ||
        !readRegister16(REG_CHARGE_VOLTAGE_LIMIT, chargeVoltage) || !readRegister8(REG_CHARGER_CONTROL0, chargerControl0) ||
        !readRegister8(REG_CHARGER_CONTROL1, chargerControl1) || !readRegister8(REG_CHARGER_CONTROL3, chargerControl3) ||
        !readRegister8(REG_CHARGER_MASK0, chargerMask0)) {
        return false;
    }

    const uint8_t expectedControl0 = static_cast<uint8_t>((configuration.chargeEnabled ? CHARGE_ENABLE_MASK : 0U) |
                                                          encodeWatchdog(configuration.watchdogSeconds));
    const uint8_t expectedExternalIlim = configuration.externalInputCurrentLimitEnabled ? EXTERNAL_ILIM_ENABLE_MASK : 0U;

    return (inputCurrent & IINDPM_MASK) == encodeInputCurrent(configuration.inputCurrentLimitMa) &&
           (chargeCurrent & ICHG_MASK) == encodeChargeCurrent(configuration.chargeCurrentLimitMa) &&
           (chargeVoltage & VREG_MASK) == encodeChargeVoltage(configuration.chargeVoltageLimitMv) &&
           (chargerControl0 & static_cast<uint8_t>(CHARGE_ENABLE_MASK | WATCHDOG_MASK)) == expectedControl0 &&
           (chargerControl1 & INPUT_OVP_MASK) == encodeInputOvp(configuration.inputOvervoltageProtectionMv) &&
           (chargerControl3 & EXTERNAL_ILIM_ENABLE_MASK) == expectedExternalIlim &&
           (chargerMask0 & ADC_DONE_MASK) == ADC_DONE_MASK;
}

bool BQ25628E::applyConfiguration(const Configuration &configuration)
{
    if (wire_ == nullptr || partNumber_ != EXPECTED_PART_NUMBER || !validateConfiguration(configuration) ||
        !writeConfiguration(configuration) || !verifyConfiguration(configuration)) {
        return false;
    }
    configuration_ = configuration;
    return true;
}

bool BQ25628E::verifyConfiguration()
{
    return initialized_ && verifyConfiguration(configuration_);
}

bool BQ25628E::begin(TwoWire &wire, uint8_t address)
{
    return begin(wire, address, Configuration{});
}

bool BQ25628E::begin(TwoWire &wire, uint8_t address, const Configuration &configuration)
{
    wire_ = &wire;
    address_ = address;
    partNumber_ = 0;
    revision_ = 0;
    initialized_ = false;
    lastStatusReadSucceeded_ = false;
    interruptPending_ = false;
    status_ = {};
    interruptFlags_ = {};
    measurements_ = {};
    consecutiveMeasurementFailures_ = 0;

    uint8_t partInformation = 0;
    if (!readRegister8(REG_PART_INFORMATION, partInformation)) {
        LOG_WARN("BQ25628E I2C read failed at 0x%02x", address_);
        return false;
    }

    partNumber_ = static_cast<uint8_t>((partInformation & PART_NUMBER_MASK) >> PART_NUMBER_SHIFT);
    revision_ = static_cast<uint8_t>(partInformation & REVISION_MASK);
    if (partNumber_ != EXPECTED_PART_NUMBER) {
        LOG_WARN("BQ25628E unexpected part information 0x%02x (PN=%u, expected %u)", partInformation, partNumber_,
                 EXPECTED_PART_NUMBER);
        return false;
    }

    if (!applyConfiguration(configuration) || !refreshStatus()) {
        LOG_WARN("BQ25628E configuration or initial status read failed");
        return false;
    }

    initialized_ = true;
    if (!updateMeasurements()) {
        LOG_WARN("BQ25628E initial ADC conversion failed; status reporting remains available");
    }

    LOG_INFO("BQ25628E detected at 0x%02x (PN=%u rev=%u), input=%umA charge=%umA/%umV watchdog=%us", address_, partNumber_,
             revision_, configuration_.inputCurrentLimitMa, configuration_.chargeCurrentLimitMa,
             configuration_.chargeVoltageLimitMv, configuration_.watchdogSeconds);
    return true;
}

bool BQ25628E::refreshStatus()
{
    uint8_t flag0 = 0;
    uint8_t flag1 = 0;
    uint8_t faultFlag = 0;
    uint8_t status0 = 0;
    uint8_t status1 = 0;
    uint8_t faultStatus = 0;

    lastStatusReadSucceeded_ = false;
    if (wire_ == nullptr || partNumber_ != EXPECTED_PART_NUMBER) {
        return false;
    }
    if (!readRegister8(REG_CHARGER_FLAG0, flag0) || !readRegister8(REG_CHARGER_FLAG1, flag1) ||
        !readRegister8(REG_FAULT_FLAG, faultFlag) || !readRegister8(REG_CHARGER_STATUS0, status0) ||
        !readRegister8(REG_CHARGER_STATUS1, status1) || !readRegister8(REG_FAULT_STATUS, faultStatus)) {
        return false;
    }

    interruptFlags_ = {flag0, flag1, faultFlag};
    status_.adcConversionDone = (status0 & 0x40U) != 0U;
    status_.thermalRegulation = (status0 & 0x20U) != 0U;
    status_.systemMinimumRegulation = (status0 & 0x10U) != 0U;
    status_.inputCurrentRegulation = (status0 & 0x08U) != 0U;
    status_.inputVoltageRegulation = (status0 & 0x04U) != 0U;
    status_.safetyTimerExpired = (status0 & 0x02U) != 0U;
    status_.watchdogExpired = (status0 & 0x01U) != 0U;
    status_.inputFault = (faultStatus & 0x80U) != 0U;
    status_.batteryFault = (faultStatus & 0x40U) != 0U;
    status_.systemFault = (faultStatus & 0x20U) != 0U;
    status_.thermalShutdown = (faultStatus & 0x08U) != 0U;
    status_.chargeState = static_cast<ChargeState>((status1 >> 3) & 0x03U);
    status_.vbusStatus = static_cast<uint8_t>(status1 & 0x07U);
    status_.thermistorStatus = static_cast<uint8_t>(faultStatus & 0x07U);
    status_.faultStatus = faultStatus;
    interruptPending_ = false;

    if (initialized_ && !verifyConfiguration(configuration_)) {
        LOG_DEBUG("BQ25628E restoring configured limits after reset or adapter change");
        if (!writeConfiguration(configuration_) || !verifyConfiguration(configuration_)) {
            return false;
        }
    }

    lastStatusReadSucceeded_ = true;
    return true;
}

bool BQ25628E::updateMeasurements()
{
    AdcRawValues raw;
    if (!initialized_ || !performAdcConversion(raw) || !applyAdcValues(raw)) {
        return recordMeasurementFailure();
    }
    return true;
}

bool BQ25628E::performAdcConversion(AdcRawValues &raw)
{
    // TS ADC needs REGN, which is unavailable below 3.2 V in battery-only mode.
    const bool thermistorEnabled = hasInput();
    const uint8_t disabledChannels = thermistorEnabled ? 0U : TS_ADC_DISABLE_MASK;
    if (!updateRegister8(REG_ADC_CONTROL, ADC_ENABLE_MASK, 0U) ||
        !updateRegister8(REG_ADC_FUNCTION_DISABLE, 0xFFU, disabledChannels)) {
        return false;
    }

    const uint32_t conversionStartedMs = millis();
    if (!updateRegister8(REG_ADC_CONTROL, ADC_CONTROL_MASK, ADC_ONE_SHOT_9_BIT)) {
        return false;
    }

    bool conversionDone = false;
    while (Throttle::isWithinTimespanMs(conversionStartedMs, ADC_TIMEOUT_MS)) {
        uint8_t chargerStatus0 = 0;
        if (!readRegister8(REG_CHARGER_STATUS0, chargerStatus0)) {
            return false;
        }
        if ((chargerStatus0 & ADC_DONE_MASK) != 0U) {
            conversionDone = true;
            break;
        }
        delay(1);
    }

    if (!conversionDone) {
        updateRegister8(REG_ADC_CONTROL, ADC_ENABLE_MASK, 0U);
        return false;
    }
    return readAdcRawValues(raw, thermistorEnabled);
}

bool BQ25628E::readAdcRawValues(AdcRawValues &raw, bool thermistorEnabled)
{
    raw = {};
    if (!readRegister16(REG_IBUS_ADC, raw.ibus) || !readRegister16(REG_IBAT_ADC, raw.ibat) ||
        !readRegister16(REG_VBUS_ADC, raw.vbus) || !readRegister16(REG_VPMID_ADC, raw.vpmid) ||
        !readRegister16(REG_VBAT_ADC, raw.vbat) || !readRegister16(REG_VSYS_ADC, raw.vsys) ||
        (thermistorEnabled && !readRegister16(REG_TS_ADC, raw.ts)) || !readRegister16(REG_TDIE_ADC, raw.tdie)) {
        return false;
    }
    raw.thermistorValid = thermistorEnabled;
    return true;
}

bool BQ25628E::applyAdcValues(const AdcRawValues &raw)
{
    const uint16_t rawBatteryVoltage = static_cast<uint16_t>((raw.vbat >> 1) & 0x0FFFU);
    if (rawBatteryVoltage == 0U) {
        return false;
    }

    measurements_ = {};
    measurements_.inputCurrentMa = static_cast<int16_t>(signExtend(static_cast<uint16_t>(raw.ibus >> 1), 15) * 2);
    measurements_.batteryCurrentValid = raw.ibat != 0x8000U;
    measurements_.thermistorValid = raw.thermistorValid;
    if (measurements_.batteryCurrentValid) {
        measurements_.batteryCurrentMa = static_cast<int16_t>(signExtend(static_cast<uint16_t>(raw.ibat >> 2), 14) * 4);
    }
    measurements_.inputVoltageMv = scaleRounded(static_cast<uint16_t>((raw.vbus >> 2) & 0x1FFFU), 397);
    measurements_.pmidVoltageMv = scaleRounded(static_cast<uint16_t>((raw.vpmid >> 2) & 0x1FFFU), 397);
    measurements_.batteryVoltageMv = scaleRounded(rawBatteryVoltage, 199);
    measurements_.systemVoltageMv = scaleRounded(static_cast<uint16_t>((raw.vsys >> 1) & 0x0FFFU), 199);
    if (measurements_.thermistorValid) {
        measurements_.thermistorPermille = static_cast<uint16_t>((static_cast<uint32_t>(raw.ts & 0x0FFFU) * 961U + 500U) / 1000U);
    }
    measurements_.dieTemperatureDeciC = static_cast<int16_t>(signExtend(raw.tdie & 0x0FFFU, 12) * 5);

    measurements_.valid = true;
    consecutiveMeasurementFailures_ = 0;
    return true;
}

bool BQ25628E::recordMeasurementFailure()
{
    if (consecutiveMeasurementFailures_ < UINT8_MAX) {
        consecutiveMeasurementFailures_++;
    }
    if (consecutiveMeasurementFailures_ >= MAX_CONSECUTIVE_MEASUREMENT_FAILURES) {
        measurements_.valid = false;
        measurements_.batteryCurrentValid = false;
        measurements_.thermistorValid = false;
    }
    return false;
}

bool BQ25628E::setInputCurrentLimit(uint16_t currentMa)
{
    Configuration updated = configuration_;
    updated.inputCurrentLimitMa = currentMa;
    return applyConfiguration(updated);
}

bool BQ25628E::setChargeCurrentLimit(uint16_t currentMa)
{
    Configuration updated = configuration_;
    updated.chargeCurrentLimitMa = currentMa;
    return applyConfiguration(updated);
}

bool BQ25628E::setChargeVoltageLimit(uint16_t voltageMv)
{
    Configuration updated = configuration_;
    updated.chargeVoltageLimitMv = voltageMv;
    return applyConfiguration(updated);
}

bool BQ25628E::setChargeEnable(bool enable)
{
    Configuration updated = configuration_;
    updated.chargeEnabled = enable;
    return applyConfiguration(updated);
}

bool BQ25628E::setWatchdogTimeout(uint16_t seconds)
{
    Configuration updated = configuration_;
    updated.watchdogSeconds = seconds;
    return applyConfiguration(updated);
}

bool BQ25628E::resetWatchdog()
{
    return initialized_ && updateRegister8(REG_CHARGER_CONTROL0, WATCHDOG_RESET_MASK, WATCHDOG_RESET_MASK);
}

bool BQ25628E::enterShipMode(bool delayed)
{
    if (!initialized_) {
        return false;
    }
    const uint8_t value = static_cast<uint8_t>((delayed ? BATFET_DELAY_MASK : 0U) | BATFET_SHIP);
    LOG_INFO("BQ25628E ship mode requested (%s)", delayed ? "12.5 s delay" : "25 ms delay");
    return updateRegister8(REG_CHARGER_CONTROL2, static_cast<uint8_t>(BATFET_DELAY_MASK | BATFET_CONTROL_MASK), value);
}

bool BQ25628E::enterShutdownMode(bool delayed)
{
    if (!initialized_ || (lastStatusReadSucceeded_ && hasInput())) {
        LOG_WARN("BQ25628E shutdown mode requires a valid device with VBUS absent");
        return false;
    }
    const uint8_t value = static_cast<uint8_t>((delayed ? BATFET_DELAY_MASK : 0U) | BATFET_SHUTDOWN);
    LOG_INFO("BQ25628E shutdown mode requested (%s)", delayed ? "12.5 s delay" : "25 ms delay");
    return updateRegister8(REG_CHARGER_CONTROL2, static_cast<uint8_t>(BATFET_DELAY_MASK | BATFET_CONTROL_MASK), value);
}

#endif // HAS_BQ25628E
