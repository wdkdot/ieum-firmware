#pragma once

#include "configuration.h"

#ifdef HAS_BQ25628E

#include <Wire.h>
#include <stdint.h>

#ifndef BQ25628E_ADDR
#define BQ25628E_ADDR 0x6A
#endif

#ifndef BQ25628E_WIRE
#define BQ25628E_WIRE Wire
#endif

#ifndef BQ25628E_INPUT_CURRENT_LIMIT_MA
#define BQ25628E_INPUT_CURRENT_LIMIT_MA 500
#endif

#ifndef BQ25628E_CHARGE_CURRENT_LIMIT_MA
#define BQ25628E_CHARGE_CURRENT_LIMIT_MA 320
#endif

#ifndef BQ25628E_CHARGE_VOLTAGE_LIMIT_MV
#define BQ25628E_CHARGE_VOLTAGE_LIMIT_MV 4200
#endif

#ifndef BQ25628E_INPUT_OVP_MV
#define BQ25628E_INPUT_OVP_MV 6300
#endif

#ifndef BQ25628E_WATCHDOG_SECONDS
#define BQ25628E_WATCHDOG_SECONDS 0
#endif

class BQ25628E
{
  public:
    enum class ChargeState : uint8_t {
        NotChargingOrTerminated = 0,
        ConstantCurrent = 1,
        ConstantVoltage = 2,
        TopOff = 3,
    };

    struct Configuration {
        uint16_t inputCurrentLimitMa = BQ25628E_INPUT_CURRENT_LIMIT_MA;
        uint16_t chargeCurrentLimitMa = BQ25628E_CHARGE_CURRENT_LIMIT_MA;
        uint16_t chargeVoltageLimitMv = BQ25628E_CHARGE_VOLTAGE_LIMIT_MV;
        uint16_t inputOvervoltageProtectionMv = BQ25628E_INPUT_OVP_MV;
        uint16_t watchdogSeconds = BQ25628E_WATCHDOG_SECONDS;
        bool chargeEnabled = true;
        bool externalInputCurrentLimitEnabled = true;
    };

    struct Status {
        bool adcConversionDone = false;
        bool thermalRegulation = false;
        bool systemMinimumRegulation = false;
        bool inputCurrentRegulation = false;
        bool inputVoltageRegulation = false;
        bool safetyTimerExpired = false;
        bool watchdogExpired = false;
        bool inputFault = false;
        bool batteryFault = false;
        bool systemFault = false;
        bool thermalShutdown = false;
        ChargeState chargeState = ChargeState::NotChargingOrTerminated;
        uint8_t vbusStatus = 0;
        uint8_t thermistorStatus = 0;
        uint8_t faultStatus = 0;
    };

    struct InterruptFlags {
        uint8_t status0 = 0;
        uint8_t status1 = 0;
        uint8_t fault = 0;

        bool hasNonAdcEvent() const { return (status0 & ~0x40U) != 0U || status1 != 0U || fault != 0U; }
    };

    struct Measurements {
        bool valid = false;
        bool batteryCurrentValid = false;
        int16_t inputCurrentMa = 0;
        int16_t batteryCurrentMa = 0;
        uint16_t inputVoltageMv = 0;
        uint16_t pmidVoltageMv = 0;
        uint16_t batteryVoltageMv = 0;
        uint16_t systemVoltageMv = 0;
        uint16_t thermistorPermille = 0;
        int16_t dieTemperatureDeciC = 0;
    };

    bool begin(TwoWire &wire, uint8_t address = BQ25628E_ADDR);
    bool begin(TwoWire &wire, uint8_t address, const Configuration &configuration);
    bool refreshStatus();
    bool updateMeasurements();
    bool applyConfiguration(const Configuration &configuration);
    bool verifyConfiguration();

    bool setInputCurrentLimit(uint16_t currentMa);
    bool setChargeCurrentLimit(uint16_t currentMa);
    bool setChargeVoltageLimit(uint16_t voltageMv);
    bool setChargeEnable(bool enable);
    bool setWatchdogTimeout(uint16_t seconds);
    bool resetWatchdog();
    bool enterShipMode(bool delayed = true);
    bool enterShutdownMode(bool delayed = true);

    void notifyInterrupt() { interruptPending_ = true; }

    bool isReady() const { return initialized_; }
    bool lastStatusReadSucceeded() const { return lastStatusReadSucceeded_; }
    bool interruptPending() const { return interruptPending_; }
    bool hasInput() const { return status_.vbusStatus != 0U; }
    bool isCharging() const { return status_.chargeState != ChargeState::NotChargingOrTerminated; }
    bool hasFault() const
    {
        return status_.inputFault || status_.batteryFault || status_.systemFault || status_.thermalShutdown ||
               status_.thermistorStatus != 0U;
    }
    uint8_t partNumber() const { return partNumber_; }
    uint8_t revision() const { return revision_; }
    const Configuration &configuration() const { return configuration_; }
    const Status &status() const { return status_; }
    const InterruptFlags &interruptFlags() const { return interruptFlags_; }
    const Measurements &measurements() const { return measurements_; }

  private:
    struct AdcRawValues {
        uint16_t ibus = 0;
        uint16_t ibat = 0;
        uint16_t vbus = 0;
        uint16_t vpmid = 0;
        uint16_t vbat = 0;
        uint16_t vsys = 0;
        uint16_t ts = 0;
        uint16_t tdie = 0;
    };

    TwoWire *wire_ = nullptr;
    uint8_t address_ = BQ25628E_ADDR;
    uint8_t partNumber_ = 0;
    uint8_t revision_ = 0;
    bool initialized_ = false;
    bool lastStatusReadSucceeded_ = false;
    volatile bool interruptPending_ = false;
    Configuration configuration_;
    Status status_;
    InterruptFlags interruptFlags_;
    Measurements measurements_;
    uint8_t consecutiveMeasurementFailures_ = 0;

    bool validateConfiguration(const Configuration &configuration) const;
    bool writeConfiguration(const Configuration &configuration);
    bool verifyConfiguration(const Configuration &configuration);
    bool readRegister8(uint8_t reg, uint8_t &value);
    bool readRegister16(uint8_t reg, uint16_t &value);
    bool writeRegister8(uint8_t reg, uint8_t value);
    bool writeRegister16(uint8_t reg, uint16_t value);
    bool updateRegister8(uint8_t reg, uint8_t mask, uint8_t value);
    bool updateRegister16(uint8_t reg, uint16_t mask, uint16_t value);
    bool performAdcConversion(AdcRawValues &raw);
    bool readAdcRawValues(AdcRawValues &raw);
    bool applyAdcValues(const AdcRawValues &raw);
    bool recordMeasurementFailure();
};

extern BQ25628E *bq25628e;

bool initBQ25628E(TwoWire &wire);
bool initBQ25628E(TwoWire &wire, const BQ25628E::Configuration &configuration);

#endif // HAS_BQ25628E
