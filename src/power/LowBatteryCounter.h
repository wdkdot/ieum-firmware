#pragma once

#include <stdint.h>

// Return true only on the reading that completes a consecutive low-voltage sequence.
inline bool updateLowBatteryCounter(uint8_t &counter, bool eligible, uint16_t voltageMv, uint16_t thresholdMv,
                                    uint8_t requiredReadings)
{
    if (!eligible || voltageMv == 0 || voltageMv >= thresholdMv || requiredReadings == 0) {
        counter = 0;
        return false;
    }
    if (counter >= requiredReadings) {
        return false;
    }
    return ++counter == requiredReadings;
}
