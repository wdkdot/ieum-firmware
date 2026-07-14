#include "MMA8652FCSensor.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && defined(HAS_MMA8652FC)

namespace
{
constexpr uint8_t REG_OUT_X_MSB = 0x01;
constexpr uint8_t REG_WHO_AM_I = MMA8652FC_WHO_AM_I_REG;
constexpr uint8_t REG_XYZ_DATA_CFG = 0x0E;
constexpr uint8_t REG_FF_MT_CFG = 0x15;
constexpr uint8_t REG_FF_MT_SRC = 0x16;
constexpr uint8_t REG_FF_MT_THS = 0x17;
constexpr uint8_t REG_FF_MT_COUNT = 0x18;
constexpr uint8_t REG_CTRL1 = 0x2A;
constexpr uint8_t REG_CTRL2 = 0x2B;
constexpr uint8_t REG_CTRL3 = 0x2C;
constexpr uint8_t REG_CTRL4 = 0x2D;
constexpr uint8_t REG_CTRL5 = 0x2E;
constexpr uint8_t REG_INT_SOURCE = 0x0C;

constexpr uint8_t RANGE_2G = 0x00;
constexpr uint8_t FF_MT_LATCHED_MOTION_XYZ = 0xF8;
constexpr uint8_t FF_MT_THRESHOLD_1_25G = 20;
constexpr uint8_t FF_MT_DEBOUNCE_320MS = 2;
constexpr uint8_t FF_MT_EVENT_ACTIVE = 0x80;
constexpr uint8_t INT_SOURCE_FF_MT = 0x04;
constexpr uint8_t CTRL1_ODR_6_25_HZ = 0x30;
constexpr uint8_t CTRL1_ACTIVE = 0x01;
constexpr uint8_t CTRL2_LOW_POWER = 0x03;

#ifdef MMA8652FC_INT_PIN
constexpr uint8_t CTRL3_PUSH_PULL_ACTIVE = MMA8652FC_INT_ACTIVE == HIGH ? 0x02 : 0x00;
constexpr uint8_t CTRL4_ENABLE_FF_MT = 0x04;
#ifdef MMA8652FC_INT1
constexpr uint8_t CTRL5_ROUTE_FF_MT = 0x04;
#else
constexpr uint8_t CTRL5_ROUTE_FF_MT = 0x00;
#endif

volatile bool mma8652fcInterrupt = false;

void handleMMA8652FCInterrupt()
{
    mma8652fcInterrupt = true;
}
#endif

constexpr int16_t decode12Bit(uint8_t msb, uint8_t lsb)
{
    uint16_t value = (static_cast<uint16_t>(msb) << 4) | (lsb >> 4);
    if ((value & 0x0800U) != 0U) {
        value |= 0xF000U;
    }
    return static_cast<int16_t>(value);
}

static_assert(decode12Bit(0x7F, 0xF0) == 2047);
static_assert(decode12Bit(0x80, 0x00) == -2048);
static_assert(decode12Bit(0xFF, 0xF0) == -1);
static_assert((CTRL1_ODR_6_25_HZ | CTRL1_ACTIVE) == 0x31);
} // namespace

MMA8652FCSensor::MMA8652FCSensor(ScanI2C::FoundDevice foundDevice) : MotionSensor(foundDevice)
{
#if WIRE_INTERFACES_COUNT == 2
    wire = devicePort() == ScanI2C::I2CPort::WIRE1 ? &Wire1 : &Wire;
#endif
}

MMA8652FCSensor::~MMA8652FCSensor()
{
#ifdef MMA8652FC_INT_PIN
    detachInterrupt(MMA8652FC_INT_PIN);
#endif
}

bool MMA8652FCSensor::readRegister(uint8_t reg, uint8_t &value)
{
    return readRegisters(reg, &value, 1);
}

bool MMA8652FCSensor::readRegisters(uint8_t reg, uint8_t *data, size_t length)
{
    wire->beginTransmission(deviceAddress());
    wire->write(reg);
    if (wire->endTransmission(false) != 0) {
        return false;
    }

    const size_t received = wire->requestFrom(deviceAddress(), static_cast<uint8_t>(length));
    if (received != length) {
        while (wire->available()) {
            wire->read();
        }
        return false;
    }

    for (size_t i = 0; i < length; i++) {
        if (!wire->available()) {
            return false;
        }
        data[i] = wire->read();
    }
    return true;
}

bool MMA8652FCSensor::writeRegister(uint8_t reg, uint8_t value)
{
    wire->beginTransmission(deviceAddress());
    wire->write(reg);
    wire->write(value);
    return wire->endTransmission() == 0;
}

bool MMA8652FCSensor::configure()
{
    uint8_t whoAmI = 0;
    if (!readRegister(REG_WHO_AM_I, whoAmI) || whoAmI != MMA8652FC_WHO_AM_I_VALUE) {
        LOG_WARN("MMA8652FC unexpected WHO_AM_I 0x%02x", whoAmI);
        return false;
    }

    uint8_t ctrl1 = 0;
    if (!readRegister(REG_CTRL1, ctrl1) || !writeRegister(REG_CTRL1, ctrl1 & ~CTRL1_ACTIVE)) {
        return false;
    }

    if (!writeRegister(REG_XYZ_DATA_CFG, RANGE_2G) || !writeRegister(REG_CTRL2, CTRL2_LOW_POWER) ||
        !writeRegister(REG_FF_MT_CFG, FF_MT_LATCHED_MOTION_XYZ) ||
        !writeRegister(REG_FF_MT_THS, FF_MT_THRESHOLD_1_25G) ||
        !writeRegister(REG_FF_MT_COUNT, FF_MT_DEBOUNCE_320MS)) {
        return false;
    }

#ifdef MMA8652FC_INT_PIN
    if (!writeRegister(REG_CTRL3, CTRL3_PUSH_PULL_ACTIVE) || !writeRegister(REG_CTRL4, CTRL4_ENABLE_FF_MT) ||
        !writeRegister(REG_CTRL5, CTRL5_ROUTE_FF_MT)) {
        return false;
    }
#endif

    if (!writeRegister(REG_CTRL1, CTRL1_ODR_6_25_HZ | CTRL1_ACTIVE))
        return false;

    delay(2);
    uint8_t verifyCtrl1 = 0;
    return readRegister(REG_CTRL1, verifyCtrl1) && verifyCtrl1 == (CTRL1_ODR_6_25_HZ | CTRL1_ACTIVE);
}

bool MMA8652FCSensor::init()
{
#ifdef MMA8652FC_INT_PIN
    const char *detectionMode = "latched IRQ";
    detachInterrupt(MMA8652FC_INT_PIN);
    pinMode(MMA8652FC_INT_PIN, INPUT);
    mma8652fcInterrupt = false;
#else
    const char *detectionMode = "source polling";
#endif

    configured = configure();
    consecutiveErrors = 0;
#ifdef MMA8652FC_INT_PIN
    if (configured) {
        uint8_t interruptSource = 0;
        uint8_t motionSource = 0;
        configured = readRegister(REG_INT_SOURCE, interruptSource) && readRegister(REG_FF_MT_SRC, motionSource);
        if (configured) {
            attachInterrupt(MMA8652FC_INT_PIN, handleMMA8652FCInterrupt,
                            MMA8652FC_INT_ACTIVE == HIGH ? RISING : FALLING);
        }
    }
#endif
    LOG_INFO("MMA8652FC init %s (12-bit, +/-2g, 6.25 Hz low power, %s)", configured ? "ok" : "failed", detectionMode);
    return configured;
}

bool MMA8652FCSensor::readAcceleration(int16_t &x, int16_t &y, int16_t &z)
{
    uint8_t data[6] = {};
    if (!readRegisters(REG_OUT_X_MSB, data, sizeof(data))) {
        return false;
    }

    x = decode12Bit(data[0], data[1]);
    y = decode12Bit(data[2], data[3]);
    z = decode12Bit(data[4], data[5]);
    return true;
}

int32_t MMA8652FCSensor::handleError()
{
    consecutiveErrors++;
    if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
        configured = false;
        LOG_WARN("MMA8652FC I2C errors, backing off before reinitialization");
        return ERROR_BACKOFF_MS;
    }
    return SAMPLE_INTERVAL_MS;
}

int32_t MMA8652FCSensor::runOnce()
{
    if (!configured) {
        return init() ? SAMPLE_INTERVAL_MS : ERROR_BACKOFF_MS;
    }

    bool motionDetected = false;
#ifdef MMA8652FC_INT_PIN
    if (mma8652fcInterrupt || digitalRead(MMA8652FC_INT_PIN) == MMA8652FC_INT_ACTIVE) {
        mma8652fcInterrupt = false;
        uint8_t interruptSource = 0;
        uint8_t motionSource = 0;
        if (!readRegister(REG_INT_SOURCE, interruptSource) ||
            ((interruptSource & INT_SOURCE_FF_MT) != 0U && !readRegister(REG_FF_MT_SRC, motionSource))) {
            return handleError();
        }
        motionDetected = (motionSource & FF_MT_EVENT_ACTIVE) != 0U;
    }
#else
    uint8_t motionSource = 0;
    if (!readRegister(REG_FF_MT_SRC, motionSource)) {
        return handleError();
    }
    motionDetected = (motionSource & FF_MT_EVENT_ACTIVE) != 0U;
#endif

    if (!readAcceleration(lastX, lastY, lastZ)) {
        return handleError();
    }

    consecutiveErrors = 0;
    if (motionDetected) {
        wakeScreen();
    }
    return SAMPLE_INTERVAL_MS;
}

#endif
