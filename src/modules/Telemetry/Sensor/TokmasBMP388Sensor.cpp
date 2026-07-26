#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && defined(HAS_TOKMAS_BMP388)

#include "TokmasBMP388Sensor.h"
#include "mesh/Throttle.h"
#include <Arduino.h>
#include <Wire.h>
#include <cmath>

namespace
{
constexpr uint8_t PRESSURE_DATA_REGISTER = 0x00;
constexpr uint8_t TEMPERATURE_DATA_REGISTER = 0x03;
constexpr uint8_t PRESSURE_CONFIG_REGISTER = 0x06;
constexpr uint8_t TEMPERATURE_CONFIG_REGISTER = 0x07;
constexpr uint8_t MEASUREMENT_CONFIG_REGISTER = 0x08;
constexpr uint8_t INTERRUPT_FIFO_CONFIG_REGISTER = 0x09;
constexpr uint8_t RESET_REGISTER = 0x0C;
constexpr uint8_t ID_REGISTER = 0x0D;
constexpr uint8_t CALIBRATION_REGISTER = 0x10;

constexpr uint8_t EXPECTED_ID = 0x11;
constexpr uint8_t SOFT_RESET_COMMAND = 0x09;
constexpr uint8_t PRESSURE_COMMAND = 0x01;
constexpr uint8_t TEMPERATURE_COMMAND = 0x02;
constexpr uint8_t PRESSURE_READY = 1U << 4;
constexpr uint8_t TEMPERATURE_READY = 1U << 5;
constexpr uint8_t SENSOR_READY = 1U << 6;
constexpr uint8_t COEFFICIENTS_READY = 1U << 7;

constexpr uint8_t OVERSAMPLING_8X = 0x03;
constexpr float RAW_SCALE_8X = 7864320.0f;
constexpr uint32_t INITIALIZATION_TIMEOUT_MS = 60;
constexpr uint32_t MEASUREMENT_TIMEOUT_MS = 50;
constexpr size_t CALIBRATION_LENGTH = 21;
} // namespace

TokmasBMP388Sensor::TokmasBMP388Sensor()
    : TelemetrySensor(meshtastic_TelemetrySensorType_BMP3XX, "BMP388_TOKMAS")
{
}

int32_t TokmasBMP388Sensor::signExtend(uint32_t value, uint8_t bits)
{
    const uint32_t signBit = 1UL << (bits - 1);
    const uint32_t mask = (1UL << bits) - 1;
    value &= mask;
    return (value & signBit) == 0 ? static_cast<int32_t>(value)
                                  : static_cast<int32_t>(value) - static_cast<int32_t>(1UL << bits);
}

bool TokmasBMP388Sensor::readRegisters(uint8_t registerAddress, uint8_t *data, size_t length)
{
    if (wire == nullptr || data == nullptr || length == 0 || length > UINT8_MAX) {
        return false;
    }

    wire->beginTransmission(address);
    wire->write(registerAddress);
    if (wire->endTransmission(false) != 0U) {
        return false;
    }

    const uint8_t expected = static_cast<uint8_t>(length);
    const uint8_t received = wire->requestFrom(address, expected);
    if (received != expected) {
        while (wire->available()) {
            wire->read();
        }
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        if (!wire->available()) {
            return false;
        }
        data[i] = static_cast<uint8_t>(wire->read());
    }
    return true;
}

bool TokmasBMP388Sensor::readRegister(uint8_t registerAddress, uint8_t &value)
{
    return readRegisters(registerAddress, &value, 1);
}

bool TokmasBMP388Sensor::writeRegister(uint8_t registerAddress, uint8_t value)
{
    if (wire == nullptr) {
        return false;
    }

    wire->beginTransmission(address);
    wire->write(registerAddress);
    wire->write(value);
    return wire->endTransmission() == 0U;
}

bool TokmasBMP388Sensor::waitForStatus(uint8_t mask, uint32_t timeoutMs)
{
    const uint32_t startedAt = millis();
    do {
        uint8_t value = 0;
        if (readRegister(MEASUREMENT_CONFIG_REGISTER, value) && (value & mask) == mask) {
            return true;
        }
        delay(1);
    } while (Throttle::isWithinTimespanMs(startedAt, timeoutMs));
    return false;
}

bool TokmasBMP388Sensor::readCalibration()
{
    uint8_t raw[CALIBRATION_LENGTH] = {};
    if (!readRegisters(CALIBRATION_REGISTER, raw, sizeof(raw))) {
        return false;
    }

    calibration.c0 = static_cast<int16_t>(signExtend((static_cast<uint32_t>(raw[0]) << 4) | (raw[1] >> 4), 12));
    calibration.c1 =
        static_cast<int16_t>(signExtend(((static_cast<uint32_t>(raw[1]) & 0x0F) << 8) | raw[2], 12));
    calibration.c00 = signExtend((static_cast<uint32_t>(raw[3]) << 12) | (static_cast<uint32_t>(raw[4]) << 4) |
                                     (raw[5] >> 4),
                                 20);
    calibration.c10 = signExtend(((static_cast<uint32_t>(raw[5]) & 0x0F) << 16) |
                                     (static_cast<uint32_t>(raw[6]) << 8) | raw[7],
                                 20);
    calibration.c01 =
        static_cast<int16_t>(signExtend((static_cast<uint32_t>(raw[8]) << 8) | raw[9], 16));
    calibration.c11 =
        static_cast<int16_t>(signExtend((static_cast<uint32_t>(raw[10]) << 8) | raw[11], 16));
    calibration.c20 =
        static_cast<int16_t>(signExtend((static_cast<uint32_t>(raw[12]) << 8) | raw[13], 16));
    calibration.c21 =
        static_cast<int16_t>(signExtend((static_cast<uint32_t>(raw[14]) << 8) | raw[15], 16));
    calibration.c30 =
        static_cast<int16_t>(signExtend((static_cast<uint32_t>(raw[16]) << 8) | raw[17], 16));
    calibration.c31 =
        static_cast<int16_t>(signExtend((static_cast<uint32_t>(raw[18]) << 4) | (raw[19] >> 4), 12));
    calibration.c40 =
        static_cast<int16_t>(signExtend(((static_cast<uint32_t>(raw[19]) & 0x0F) << 8) | raw[20], 12));
    return true;
}

bool TokmasBMP388Sensor::triggerAndRead(uint8_t command, uint8_t readyMask, uint8_t dataRegister, int32_t &rawValue)
{
    if (!writeRegister(MEASUREMENT_CONFIG_REGISTER, command) || !waitForStatus(readyMask, MEASUREMENT_TIMEOUT_MS)) {
        return false;
    }

    uint8_t raw[3] = {};
    if (!readRegisters(dataRegister, raw, sizeof(raw))) {
        return false;
    }
    rawValue = signExtend((static_cast<uint32_t>(raw[0]) << 16) | (static_cast<uint32_t>(raw[1]) << 8) | raw[2], 24);
    return true;
}

bool TokmasBMP388Sensor::readMeasurement(float &temperatureC, float &pressurePa)
{
    int32_t rawTemperature = 0;
    int32_t rawPressure = 0;
    if (!triggerAndRead(TEMPERATURE_COMMAND, TEMPERATURE_READY, TEMPERATURE_DATA_REGISTER, rawTemperature) ||
        !triggerAndRead(PRESSURE_COMMAND, PRESSURE_READY, PRESSURE_DATA_REGISTER, rawPressure)) {
        return false;
    }

    const float scaledTemperature = static_cast<float>(rawTemperature) / RAW_SCALE_8X;
    const float scaledPressure = static_cast<float>(rawPressure) / RAW_SCALE_8X;

    temperatureC = static_cast<float>(calibration.c0) * 0.5f + static_cast<float>(calibration.c1) * scaledTemperature;
    pressurePa = static_cast<float>(calibration.c00) +
                 scaledPressure *
                     (static_cast<float>(calibration.c10) +
                      scaledPressure *
                          (static_cast<float>(calibration.c20) +
                           scaledPressure *
                               (static_cast<float>(calibration.c30) +
                                scaledPressure * static_cast<float>(calibration.c40)))) +
                 scaledTemperature *
                     (static_cast<float>(calibration.c01) +
                      scaledPressure *
                          (static_cast<float>(calibration.c11) +
                           scaledPressure *
                               (static_cast<float>(calibration.c21) +
                                scaledPressure * static_cast<float>(calibration.c31))));

    return isfinite(temperatureC) && isfinite(pressurePa) && temperatureC >= -40.0f && temperatureC <= 85.0f &&
           pressurePa >= 30000.0f && pressurePa <= 110000.0f;
}

bool TokmasBMP388Sensor::initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev)
{
    LOG_INFO("Init sensor: %s", sensorName);
    wire = bus;
    address = dev->address.address;

    uint8_t chipId = 0;
    if (!readRegister(ID_REGISTER, chipId) || chipId != EXPECTED_ID) {
        LOG_WARN("%s ID mismatch at 0x%02x: 0x%02x", sensorName, address, chipId);
        return false;
    }

    if (!writeRegister(RESET_REGISTER, SOFT_RESET_COMMAND) ||
        !waitForStatus(SENSOR_READY | COEFFICIENTS_READY, INITIALIZATION_TIMEOUT_MS)) {
        LOG_WARN("%s initialization timed out", sensorName);
        return false;
    }

    if (!readCalibration() || !writeRegister(MEASUREMENT_CONFIG_REGISTER, 0x00) ||
        !writeRegister(PRESSURE_CONFIG_REGISTER, OVERSAMPLING_8X) ||
        !writeRegister(TEMPERATURE_CONFIG_REGISTER, OVERSAMPLING_8X) ||
        !writeRegister(INTERRUPT_FIFO_CONFIG_REGISTER, 0x00)) {
        LOG_WARN("%s configuration failed", sensorName);
        return false;
    }

    float temperatureC = 0.0f;
    float pressurePa = 0.0f;
    if (!readMeasurement(temperatureC, pressurePa)) {
        LOG_WARN("%s first measurement failed", sensorName);
        return false;
    }

    status = true;
    initI2CSensor();
    LOG_INFO("%s ready: %.1fC %.1fhPa", sensorName, temperatureC, pressurePa / 100.0f);
    return true;
}

bool TokmasBMP388Sensor::getMetrics(meshtastic_Telemetry *measurement)
{
    float temperatureC = 0.0f;
    float pressurePa = 0.0f;
    if (!readMeasurement(temperatureC, pressurePa)) {
        LOG_WARN("%s measurement failed", sensorName);
        return false;
    }

    measurement->variant.environment_metrics.has_temperature = true;
    measurement->variant.environment_metrics.has_barometric_pressure = true;
    measurement->variant.environment_metrics.temperature = temperatureC;
    measurement->variant.environment_metrics.barometric_pressure = pressurePa / 100.0f;
    LOG_DEBUG("%s temperature: %.1fC pressure: %.1fhPa", sensorName, temperatureC, pressurePa / 100.0f);
    return true;
}

#endif
