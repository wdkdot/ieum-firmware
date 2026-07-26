/*

E-Ink display driver
    - GDEY0266T90H
    - Manufacturer: Good Display
    - Controller: SSD1685
    - Size: 2.66 inch
    - Resolution: 184px x 360px

*/

#pragma once

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "configuration.h"

#include "./SSD16XX.h"

namespace NicheGraphics::Drivers
{

class GDEY0266T90H : public SSD16XX
{
  public:
    enum class QuickUpdateMode : uint8_t {
        FAST,
        PARTIAL,
    };

    GDEY0266T90H(uint8_t pinPower = 0xFF, bool powerActiveHigh = true, uint8_t pinSclk = 0xFF, uint8_t pinMosi = 0xFF);

    void begin(SPIClass *spi, uint8_t pinDc, uint8_t pinCs, uint8_t pinBusy, uint8_t pinRst = 0xFF) override;
    void update(uint8_t *imageData, UpdateTypes type) override;
    void setQuickUpdateMode(QuickUpdateMode mode);

  protected:
    void wait(uint32_t timeoutMs = COMMAND_BUSY_TIMEOUT_MS) override;
    void reset() override;
    void configFullscreen() override;
    void configScanning() override;
    void configWaveform() override;
    void configUpdateSequence() override;
    void writeNewImage() override;
    void writeOldImage() override;
    void detachFromUpdate() override;
    bool isUpdateDone() override;
    void finalizeUpdate() override;
    void abortUpdate() override;
    void deepSleep() override;

  private:
    static constexpr uint16_t PANEL_WIDTH = 184;
    static constexpr uint16_t PANEL_HEIGHT = 360;
    static constexpr uint32_t PANEL_BUFFER_SIZE = PANEL_WIDTH * PANEL_HEIGHT / 8;
    static constexpr UpdateTypes SUPPORTED_UPDATES = static_cast<UpdateTypes>(FULL | FAST);
    static constexpr uint32_t COMMAND_BUSY_TIMEOUT_MS = 5000;
    static constexpr uint32_t REFRESH_BUSY_TIMEOUT_MS = 9000;

    void configureFastRefresh();
    void startSession();
    void finishSession(bool enterDeepSleep);
    void setPower(bool enabled);
    void releasePins();
    void sendImageBottomToTop(const uint8_t *image);
    void writeZeroPlane();

    QuickUpdateMode quickUpdateMode = QuickUpdateMode::FAST;
    uint8_t *previousBuffer = nullptr;
    bool hasPreviousBuffer = false;
    bool sessionActive = false;
    uint32_t refreshStartedAt = 0;

    uint8_t pinPower = 0xFF;
    bool powerActiveHigh = true;
    uint8_t pinSclk = 0xFF;
    uint8_t pinMosi = 0xFF;
};

static_assert((184 * 360 / 8) == 8280, "GDEY0266T90H framebuffer size must be 8,280 bytes");

} // namespace NicheGraphics::Drivers

#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS
