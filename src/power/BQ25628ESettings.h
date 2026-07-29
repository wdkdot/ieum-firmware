#pragma once

#include "configuration.h"

#ifdef HAS_BQ25628E

#include <stdint.h>

class BQ25628ESettings
{
  public:
    static constexpr uint16_t BATTERY_CARE_VOLTAGE_MV = 4000;
    static constexpr uint16_t FULL_CHARGE_VOLTAGE_MV = 4200;

    bool load(uint16_t &chargeVoltageLimitMv) const;
    bool save(uint16_t chargeVoltageLimitMv) const;

    static bool isSupportedVoltage(uint16_t chargeVoltageLimitMv);
};

#endif // HAS_BQ25628E
