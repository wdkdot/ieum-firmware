#pragma once

#include "BQ25628E.h"

#ifdef HAS_BQ25628E

template <typename BatteryLevelBase> class BQ25628EBatteryLevel final : public BatteryLevelBase
{
  public:
    int getBatteryPercent() override { return -1; }

    uint16_t getBattVoltage() override
    {
        return hasValidMeasurement() ? bq25628e->measurements().batteryVoltageMv : 0;
    }

    bool isBatteryConnect() override { return hasValidMeasurement() && !bq25628e->hasInput(); }

    bool isVbusIn() override { return hasValidStatus() && bq25628e->hasInput(); }

    bool isCharging() override { return hasValidStatus() && bq25628e->isCharging(); }

  private:
    bool hasValidStatus() const { return bq25628e != nullptr && bq25628e->lastStatusReadSucceeded(); }

    bool hasValidMeasurement() const { return hasValidStatus() && bq25628e->measurements().valid; }
};

#endif // HAS_BQ25628E
