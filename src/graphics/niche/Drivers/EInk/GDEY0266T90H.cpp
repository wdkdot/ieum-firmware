#include "./GDEY0266T90H.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include <cstring>

#include "mesh/Throttle.h"

using namespace NicheGraphics::Drivers;

GDEY0266T90H::GDEY0266T90H(uint8_t pinPower, bool powerActiveHigh, uint8_t pinSclk, uint8_t pinMosi)
    : SSD16XX(PANEL_WIDTH, PANEL_HEIGHT, SUPPORTED_UPDATES), pinPower(pinPower), powerActiveHigh(powerActiveHigh),
      pinSclk(pinSclk), pinMosi(pinMosi)
{
    // Good Display's sample uses 10 MHz; 8 MHz remains below the panel's 20 MHz limit.
    spiSettings = SPISettings(8000000, MSBFIRST, SPI_MODE0);
}

void GDEY0266T90H::begin(SPIClass *spi, uint8_t pinDc, uint8_t pinCs, uint8_t pinBusy, uint8_t pinRst)
{
    this->spi = spi;
    pin_dc = pinDc;
    pin_cs = pinCs;
    pin_busy = pinBusy;
    pin_rst = pinRst;

    releasePins();
    setPower(false);
}

void GDEY0266T90H::setQuickUpdateMode(QuickUpdateMode mode)
{
    if (mode == QuickUpdateMode::PARTIAL && !previousBuffer) {
        previousBuffer = new uint8_t[PANEL_BUFFER_SIZE];
        if (!previousBuffer) {
            LOG_WARN("GDEY0266T90H could not allocate previous framebuffer; using fast refresh");
            quickUpdateMode = QuickUpdateMode::FAST;
            hasPreviousBuffer = false;
            return;
        }

        memset(previousBuffer, 0xFF, PANEL_BUFFER_SIZE);
        hasPreviousBuffer = false;
    }

    quickUpdateMode = mode;
}

void GDEY0266T90H::update(uint8_t *imageData, UpdateTypes type)
{
    if (!imageData) {
        LOG_ERROR("GDEY0266T90H update received an empty framebuffer");
        return;
    }

    failed = false;
    buffer = imageData;
    updateType = (type == FAST) ? FAST : FULL;

    if (updateType == FAST && quickUpdateMode == QuickUpdateMode::PARTIAL && (!previousBuffer || !hasPreviousBuffer)) {
        LOG_INFO("GDEY0266T90H partial refresh needs a base frame; using full refresh");
        updateType = FULL;
    }

    startSession();
    if (failed) {
        abortUpdate();
        failed = false;
        return;
    }

    reset();
    configScanning();
    configFullscreen();
    configWaveform();

    if (updateType == FAST && quickUpdateMode == QuickUpdateMode::FAST)
        configureFastRefresh();

    wait(COMMAND_BUSY_TIMEOUT_MS);
    if (failed) {
        abortUpdate();
        failed = false;
        return;
    }

    writeNewImage();
    writeOldImage();
    configUpdateSequence();

    refreshStartedAt = millis();
    sendCommand(0x20); // Activate the display update sequence.

    if (failed) {
        abortUpdate();
        failed = false;
        return;
    }

    detachFromUpdate();
}

void GDEY0266T90H::wait(uint32_t timeoutMs)
{
    if (failed)
        return;

    const uint32_t startedAt = millis();
    while (digitalRead(pin_busy) == HIGH) {
        if (!Throttle::isWithinTimespanMs(startedAt, timeoutMs)) {
            LOG_ERROR("GDEY0266T90H BUSY timeout during controller setup");
            failed = true;
            return;
        }
        yield();
    }
}

void GDEY0266T90H::reset()
{
    if (failed)
        return;

    if (pin_rst != 0xFF) {
        digitalWrite(pin_rst, LOW);
        delay(10);
        digitalWrite(pin_rst, HIGH);
        delay(10);
        wait(COMMAND_BUSY_TIMEOUT_MS);
    }

    sendCommand(0x12); // Software reset.
    wait(COMMAND_BUSY_TIMEOUT_MS);
}

void GDEY0266T90H::configScanning()
{
    sendCommand(0x01); // Driver output control: 360 gates, G0 first.
    sendData(0x67);
    sendData(0x01);
    sendData(0x00);
}

void GDEY0266T90H::configFullscreen()
{
    sendCommand(0x11); // X increments while Y decrements.
    sendData(0x01);

    sendCommand(0x44); // RAM X: 23 bytes per 184-pixel row.
    sendData(0x00);
    sendData(0x16);

    sendCommand(0x45); // RAM Y: gate 359 down to gate 0.
    sendData(0x67);
    sendData(0x01);
    sendData(0x00);
    sendData(0x00);
}

void GDEY0266T90H::configWaveform()
{
    sendCommand(0x3C); // Border follows LUT1.
    sendData(0x05);

    sendCommand(0x18); // Select the internal temperature sensor.
    sendData(0x80);

    sendCommand(0x4E); // RAM cursor X = 0.
    sendData(0x00);
    sendCommand(0x4F); // RAM cursor Y = 359.
    sendData(0x67);
    sendData(0x01);
}

void GDEY0266T90H::configureFastRefresh()
{
    sendCommand(0x18); // Read the internal temperature sensor.
    sendData(0x80);

    sendCommand(0x22); // Load the measured temperature value.
    sendData(0xB1);
    sendCommand(0x20);
    wait(COMMAND_BUSY_TIMEOUT_MS);

    sendCommand(0x1A); // Good Display's 1.5-second fast-refresh temperature setting.
    sendData(0x6E);
    sendData(0x00);

    sendCommand(0x22);
    sendData(0x91);
    sendCommand(0x20);
    wait(COMMAND_BUSY_TIMEOUT_MS);
}

void GDEY0266T90H::configUpdateSequence()
{
    sendCommand(0x22);

    if (updateType == FULL)
        sendData(0xF4);
    else if (quickUpdateMode == QuickUpdateMode::PARTIAL)
        sendData(0x1C);
    else
        sendData(0xC7);
}

void GDEY0266T90H::sendImageBottomToTop(const uint8_t *image)
{
    constexpr uint16_t ROW_BYTES = PANEL_WIDTH / 8;
    constexpr uint8_t ROWS_PER_CHUNK = 8;
    uint8_t chunk[ROW_BYTES * ROWS_PER_CHUNK];
    uint16_t sourceRow = PANEL_HEIGHT;

    while (sourceRow > 0) {
        const uint8_t rows = sourceRow > ROWS_PER_CHUNK ? ROWS_PER_CHUNK : static_cast<uint8_t>(sourceRow);

        for (uint8_t row = 0; row < rows; row++) {
            memcpy(chunk + (row * ROW_BYTES), image + ((sourceRow - 1 - row) * ROW_BYTES), ROW_BYTES);
        }

        sendData(chunk, rows * ROW_BYTES);
        sourceRow -= rows;
    }
}

void GDEY0266T90H::writeNewImage()
{
    sendCommand(0x24);
    sendImageBottomToTop(buffer);
}

void GDEY0266T90H::writeOldImage()
{
    if (updateType == FAST && quickUpdateMode == QuickUpdateMode::PARTIAL && previousBuffer && hasPreviousBuffer) {
        sendCommand(0x26);
        sendImageBottomToTop(previousBuffer);
        return;
    }

    writeZeroPlane();
}

void GDEY0266T90H::writeZeroPlane()
{
    constexpr uint16_t CHUNK_SIZE = 64;
    uint8_t zeroes[CHUNK_SIZE] = {};
    uint32_t remaining = bufferSize;

    sendCommand(0x26);
    while (remaining > 0) {
        const uint16_t bytes = remaining > CHUNK_SIZE ? CHUNK_SIZE : static_cast<uint16_t>(remaining);
        sendData(zeroes, bytes);
        remaining -= bytes;
    }
}

void GDEY0266T90H::detachFromUpdate()
{
    beginPolling(50, 0);
}

bool GDEY0266T90H::isUpdateDone()
{
    if (digitalRead(pin_busy) == LOW)
        return true;

    if (!Throttle::isWithinTimespanMs(refreshStartedAt, REFRESH_BUSY_TIMEOUT_MS)) {
        LOG_ERROR("GDEY0266T90H BUSY timeout during display refresh");
        failed = true;
    }

    return false;
}

void GDEY0266T90H::finalizeUpdate()
{
    if (previousBuffer) {
        memcpy(previousBuffer, buffer, bufferSize);
        hasPreviousBuffer = true;
    }

    finishSession(true);
}

void GDEY0266T90H::abortUpdate()
{
    hasPreviousBuffer = false;
    finishSession(false);
}

void GDEY0266T90H::deepSleep()
{
    sendCommand(0x10);
    sendData(0x01);

    // BUSY remains asserted in deep sleep; the official sample waits instead of polling it.
    delay(100);
}

void GDEY0266T90H::startSession()
{
    if (!spi || pin_dc == 0xFF || pin_cs == 0xFF || pin_busy == 0xFF) {
        LOG_ERROR("GDEY0266T90H is missing required SPI or control pins");
        failed = true;
        return;
    }

    setPower(true);
    sessionActive = true;

    pinMode(pin_cs, OUTPUT);
    digitalWrite(pin_cs, HIGH);
    pinMode(pin_dc, OUTPUT);
    digitalWrite(pin_dc, HIGH);
    pinMode(pin_busy, INPUT);

    if (pin_rst != 0xFF) {
        pinMode(pin_rst, OUTPUT);
        digitalWrite(pin_rst, HIGH);
    }

    spi->begin();
}

void GDEY0266T90H::finishSession(bool enterDeepSleep)
{
    if (sessionActive && enterDeepSleep)
        deepSleep();

    if (sessionActive && spi)
        spi->end();

    releasePins();
    setPower(false);
    sessionActive = false;
}

void GDEY0266T90H::setPower(bool enabled)
{
    if (pinPower == 0xFF)
        return;

    const uint8_t level = (enabled == powerActiveHigh) ? HIGH : LOW;
    digitalWrite(pinPower, level);
    pinMode(pinPower, OUTPUT);
}

void GDEY0266T90H::releasePins()
{
    const uint8_t pins[] = {pinSclk, pinMosi, pin_cs, pin_dc, pin_rst, pin_busy};

    for (const uint8_t pin : pins) {
        if (pin != 0xFF)
            pinMode(pin, INPUT);
    }
}

#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS
