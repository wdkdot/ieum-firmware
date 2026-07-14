/*
  Copyright (c) 2014-2015 Arduino LLC.  All rights reserved.
  Copyright (c) 2016 Sandeep Mistry All rights reserved.
  Copyright (c) 2018 Adafruit Industries (adafruit.com)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
*/

#include "variant.h"
#include "nrf_gpio.h"

const uint32_t g_ADigitalPinMap[] = {
    // P0
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,

    // P1
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};

static void setSwitchedPeripheralsOff()
{
    const uint32_t pins[] = {IEUM_GNSS_RX_PIN, IEUM_GNSS_TX_PIN, PIN_EINK_SCLK, PIN_EINK_MOSI,
                             PIN_EINK_CS,      PIN_EINK_DC,      PIN_EINK_RES,  PIN_EINK_BUSY};

    for (const uint32_t pin : pins) {
        nrf_gpio_cfg_default(pin);
    }

    nrf_gpio_cfg_output(IEUM_GNSS_EN_PIN);
    nrf_gpio_pin_clear(IEUM_GNSS_EN_PIN);
    nrf_gpio_cfg_output(IEUM_EINK_EN_PIN);
    nrf_gpio_pin_clear(IEUM_EINK_EN_PIN);
}

static void setLedsOff()
{
    nrf_gpio_cfg_output(PIN_LED1);
    nrf_gpio_pin_clear(PIN_LED1);
    nrf_gpio_cfg_output(PIN_LED2);
    nrf_gpio_pin_clear(PIN_LED2);
}

void earlyInitVariant()
{
    setSwitchedPeripheralsOff();
    setLedsOff();
}

void initVariant()
{
    nrf_gpio_cfg_input(PIN_BUTTON1, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_input(PIN_BUTTON2, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_input(IEUM_PMIC_INT_PIN, NRF_GPIO_PIN_NOPULL);
}

void variant_shutdown()
{
    setSwitchedPeripheralsOff();
    setLedsOff();
}
