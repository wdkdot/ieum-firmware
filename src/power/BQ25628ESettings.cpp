#include "BQ25628ESettings.h"

#ifdef HAS_BQ25628E

#include "FSCommon.h"
#include "SPILock.h"
#include "SafeFile.h"
#include "concurrency/LockGuard.h"

#include <stddef.h>

namespace
{
constexpr char SETTINGS_FILE[] = "/prefs/bq25628e.dat";
constexpr uint32_t SETTINGS_MAGIC = 0x42513238U; // "BQ28"
constexpr uint16_t SETTINGS_VERSION = 1;

struct SettingsRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t chargeVoltageLimitMv;
    uint32_t checksum;
};

static_assert(offsetof(SettingsRecord, checksum) == 8U);
static_assert(sizeof(SettingsRecord) == 12U);

uint32_t calculateChecksum(const SettingsRecord &record)
{
    uint32_t checksum = 2166136261U;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    for (size_t i = 0; i < offsetof(SettingsRecord, checksum); i++) {
        checksum ^= bytes[i];
        checksum *= 16777619U;
    }
    return checksum;
}
} // namespace

bool BQ25628ESettings::isSupportedVoltage(uint16_t chargeVoltageLimitMv)
{
    return chargeVoltageLimitMv == BATTERY_CARE_VOLTAGE_MV || chargeVoltageLimitMv == FULL_CHARGE_VOLTAGE_MV;
}

bool BQ25628ESettings::load(uint16_t &chargeVoltageLimitMv) const
{
#ifdef FSCom
    SettingsRecord record = {};

    concurrency::LockGuard guard(spiLock);
    auto file = FSCom.open(SETTINGS_FILE, FILE_O_READ);
    if (!file) {
        LOG_INFO("No BQ25628E settings found; using %umV", chargeVoltageLimitMv);
        return false;
    }

    const size_t bytesRead = file.read(reinterpret_cast<uint8_t *>(&record), sizeof(record));
    const size_t fileSize = file.size();
    file.close();

    const bool valid = bytesRead == sizeof(record) && fileSize == sizeof(record) && record.magic == SETTINGS_MAGIC &&
                       record.version == SETTINGS_VERSION && isSupportedVoltage(record.chargeVoltageLimitMv) &&
                       record.checksum == calculateChecksum(record);
    if (!valid) {
        LOG_WARN("Invalid BQ25628E settings; using %umV", chargeVoltageLimitMv);
        return false;
    }

    chargeVoltageLimitMv = record.chargeVoltageLimitMv;
    LOG_INFO("Loaded BQ25628E charge voltage limit: %umV", chargeVoltageLimitMv);
    return true;
#else
    LOG_WARN("BQ25628E settings require a filesystem; using %umV", chargeVoltageLimitMv);
    return false;
#endif
}

bool BQ25628ESettings::save(uint16_t chargeVoltageLimitMv) const
{
    if (!isSupportedVoltage(chargeVoltageLimitMv)) {
        LOG_WARN("Refusing unsupported BQ25628E charge voltage: %umV", chargeVoltageLimitMv);
        return false;
    }

#ifdef FSCom
    SettingsRecord record = {SETTINGS_MAGIC, SETTINGS_VERSION, chargeVoltageLimitMv, 0};
    record.checksum = calculateChecksum(record);

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
    if (!writeSucceeded || !load(verifiedVoltageMv) || verifiedVoltageMv != chargeVoltageLimitMv) {
        LOG_WARN("Failed to save BQ25628E charge voltage limit");
        return false;
    }

    LOG_INFO("Saved BQ25628E charge voltage limit: %umV", chargeVoltageLimitMv);
    return true;
#else
    LOG_WARN("BQ25628E settings require a filesystem");
    return false;
#endif
}

#endif // HAS_BQ25628E
