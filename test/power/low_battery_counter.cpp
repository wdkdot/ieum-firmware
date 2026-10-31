#include "power/LowBatteryCounter.h"
#include <assert.h>
#include <stdio.h>

static void testConsecutiveReadings()
{
    uint8_t count = 0;
    assert(!updateLowBatteryCounter(count, true, 3299, 3300, 3));
    assert(!updateLowBatteryCounter(count, true, 3250, 3300, 3));
    assert(updateLowBatteryCounter(count, true, 3200, 3300, 3));
    for (unsigned i = 0; i < 300; ++i) {
        assert(!updateLowBatteryCounter(count, true, 3200, 3300, 3));
        assert(count == 3);
    }
}

static void testRecoveryAndInvalidReadings()
{
    uint8_t count = 2;
    assert(!updateLowBatteryCounter(count, true, 3300, 3300, 3));
    assert(count == 0);
    count = 2;
    assert(!updateLowBatteryCounter(count, true, 3400, 3300, 3));
    assert(count == 0);
    count = 2;
    assert(!updateLowBatteryCounter(count, true, 0, 3300, 3));
    assert(count == 0);
    // USB input, an absent battery, unknown status, or a failed ADC invalidate
    // the sequence.
    count = 2;
    assert(!updateLowBatteryCounter(count, false, 3200, 3300, 3));
    assert(count == 0);
    assert(!updateLowBatteryCounter(count, true, 3200, 3300, 3));
    assert(!updateLowBatteryCounter(count, true, 3200, 3300, 3));
    assert(updateLowBatteryCounter(count, true, 3200, 3300, 3));
}

static void testUpstreamDefaults()
{
    uint8_t count = 0;
    for (unsigned i = 0; i < 10; ++i) {
        assert(!updateLowBatteryCounter(count, true, 3099, 3100, 11));
    }
    assert(updateLowBatteryCounter(count, true, 3099, 3100, 11));
    assert(!updateLowBatteryCounter(count, true, 3100, 3100, 11));
    assert(count == 0);
}

int main()
{
    testConsecutiveReadings();
    testRecoveryAndInvalidReadings();
    testUpstreamDefaults();
    puts("Low battery counter: 3 scenarios passed");
}
