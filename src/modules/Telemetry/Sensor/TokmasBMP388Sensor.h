#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && defined(HAS_TOKMAS_BMP388)

#pragma once

#include "TelemetrySensor.h"
#include <stddef.h>
#include <stdint.h>

class TokmasBMP388Sensor : public TelemetrySensor
{
  private:
    struct Calibration {
        int16_t c0;
        int16_t c1;
        int32_t c00;
        int32_t c10;
        int16_t c01;
        int16_t c11;
        int16_t c20;
        int16_t c21;
        int16_t c30;
        int16_t c31;
        int16_t c40;
    } calibration{};

    TwoWire *wire = nullptr;
    uint8_t address = 0;

    static int32_t signExtend(uint32_t value, uint8_t bits);
    bool readRegisters(uint8_t registerAddress, uint8_t *data, size_t length);
    bool readRegister(uint8_t registerAddress, uint8_t &value);
    bool writeRegister(uint8_t registerAddress, uint8_t value);
    bool waitForStatus(uint8_t mask, uint32_t timeoutMs);
    bool readCalibration();
    bool triggerAndRead(uint8_t command, uint8_t readyMask, uint8_t dataRegister, int32_t &rawValue);
    bool readMeasurement(float &temperatureC, float &pressurePa);

  public:
    TokmasBMP388Sensor();
    bool getMetrics(meshtastic_Telemetry *measurement) override;
    bool initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev) override;
};

#endif
