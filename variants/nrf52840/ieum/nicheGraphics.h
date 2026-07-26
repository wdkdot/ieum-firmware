#pragma once

#include "configuration.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "graphics/niche/Drivers/EInk/GDEY0266T90H.h"
#include "graphics/niche/InkHUD/Applets/User/AllMessage/AllMessageApplet.h"
#include "graphics/niche/InkHUD/Applets/User/DM/DMApplet.h"
#include "graphics/niche/InkHUD/Applets/User/FavoritesMap/FavoritesMapApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Heard/HeardApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Positions/PositionsApplet.h"
#include "graphics/niche/InkHUD/Applets/User/RecentsList/RecentsListApplet.h"
#include "graphics/niche/InkHUD/Applets/User/ThreadedMessage/ThreadedMessageApplet.h"
#include "graphics/niche/InkHUD/InkHUD.h"
#include "graphics/niche/Inputs/TwoButton.h"

static void prepareIeumButtons(NicheGraphics::InkHUD::InkHUD *inkhud)
{
    using namespace NicheGraphics;

    Inputs::TwoButton *buttons = Inputs::TwoButton::getInstance();
    buttons->setWiring(0, IEUM_BUTTON1_PIN, false);
    buttons->setTiming(0, 50, 500);
    buttons->setHandlerShortPress(0, [inkhud]() { inkhud->shortpress(); });
    buttons->setHandlerLongPress(0, [inkhud]() { inkhud->longpress(); });

    buttons->setWiring(1, IEUM_BUTTON2_PIN, false);
    buttons->setTiming(1, 50, 500);
    buttons->setHandlerShortPress(1, [inkhud]() { inkhud->shortpress(); });
    buttons->setHandlerLongPress(1, [inkhud]() { inkhud->longpress(); });

    buttons->start();
}

void setupNicheGraphics()
{
    using namespace NicheGraphics;

    auto *driver = new Drivers::GDEY0266T90H(IEUM_EINK_EN_PIN, IEUM_EINK_EN_ACTIVE == HIGH, PIN_EINK_SCLK, PIN_EINK_MOSI);

#if IEUM_EINK_USE_PARTIAL_REFRESH
    driver->setQuickUpdateMode(Drivers::GDEY0266T90H::QuickUpdateMode::PARTIAL);
#endif

    driver->begin(&SPI1, PIN_EINK_DC, PIN_EINK_CS, PIN_EINK_BUSY, PIN_EINK_RES);

    InkHUD::InkHUD *inkhud = InkHUD::InkHUD::getInstance();
    inkhud->setDriver(driver);
    inkhud->setDisplayResilience(IEUM_INKHUD_FAST_PER_FULL);

    InkHUD::Applet::fontLarge = FREESANS_12PT_WIN1252;
    InkHUD::Applet::fontMedium = FREESANS_9PT_WIN1252;
    InkHUD::Applet::fontSmall = FREESANS_6PT_WIN1252;

    inkhud->persistence->settings.rotation = IEUM_INKHUD_ROTATION;
    inkhud->persistence->settings.userTiles.maxCount = 1;
    inkhud->persistence->settings.optionalFeatures.batteryIcon = true;

    inkhud->addApplet("All Messages", new InkHUD::AllMessageApplet, true, true, 0);
    inkhud->addApplet("DMs", new InkHUD::DMApplet);
    inkhud->addApplet("Channel 0", new InkHUD::ThreadedMessageApplet(0));
    inkhud->addApplet("Channel 1", new InkHUD::ThreadedMessageApplet(1));
    inkhud->addApplet("Positions", new InkHUD::PositionsApplet, true);
    inkhud->addApplet("Favorites Map", new InkHUD::FavoritesMapApplet);
    inkhud->addApplet("Recents List", new InkHUD::RecentsListApplet);
    inkhud->addApplet("Heard", new InkHUD::HeardApplet, true);

    inkhud->begin();
    prepareIeumButtons(inkhud);
}

#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS
