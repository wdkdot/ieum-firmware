#pragma once
#ifndef _MMA8652FC_SENSOR_H_
#define _MMA8652FC_SENSOR_H_

#include "MotionSensor.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && defined(HAS_MMA8652FC)

class MMA8652FCSensor : public MotionSensor
{
  public:
    explicit MMA8652FCSensor(ScanI2C::FoundDevice foundDevice);
    ~MMA8652FCSensor() override;
    bool init() override;
    int32_t runOnce() override;
    bool readAcceleration(int16_t &x, int16_t &y, int16_t &z);

  private:
    static constexpr uint8_t MAX_CONSECUTIVE_ERRORS = 3;
    static constexpr int32_t SAMPLE_INTERVAL_MS = 160;
    static constexpr int32_t ERROR_BACKOFF_MS = 5000;

    TwoWire *wire = &Wire;
    bool configured = false;
    uint8_t consecutiveErrors = 0;
    int16_t lastX = 0;
    int16_t lastY = 0;
    int16_t lastZ = 0;

    bool configure();
    bool readRegister(uint8_t reg, uint8_t &value);
    bool readRegisters(uint8_t reg, uint8_t *data, size_t length);
    bool writeRegister(uint8_t reg, uint8_t value);
    int32_t handleError();
};

#endif

#endif
