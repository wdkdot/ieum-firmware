/*
  Copyright (c) 2014-2015 Arduino LLC.  All rights reserved.
  Copyright (c) 2016 Sandeep Mistry All rights reserved.
  Copyright (c) 2018 Adafruit Industries (adafruit.com)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
*/

#ifndef _VARIANT_IEUM_
#define _VARIANT_IEUM_

#define RAK4630

#define VARIANT_MCK (64000000ul)
#define USE_LFXO

#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PINS_COUNT (48)
#define NUM_DIGITAL_PINS (48)
#define NUM_ANALOG_INPUTS (0)
#define NUM_ANALOG_OUTPUTS (0)

// LEDs
#define PIN_LED1 (21) // P0.21, LED 1
#define PIN_LED2 (35) // P1.03, LED 2
#define LED_STATE_ON 1

// Buttons use board-level pull-ups and are active low when pressed.
#define PIN_BUTTON1 (33) // P1.01
#define PIN_BUTTON2 (34) // P1.02
#define BUTTON_ACTIVE_LOW true
#define BUTTON_ACTIVE_PULLUP false
#define ALT_BUTTON_ACTIVE_LOW true
#define ALT_BUTTON_ACTIVE_PULLUP false

// Sensor I2C bus
#define WIRE_INTERFACES_COUNT 1
#define PIN_WIRE_SDA (13) // P0.13, I2C1_SDA
#define PIN_WIRE_SCL (14) // P0.14, I2C1_SCL

// UARTs. Serial1 is wired to GNSS; Serial2 is the auxiliary UART.
#define PIN_SERIAL1_RX (15) // P0.15, GNSS_TX
#define PIN_SERIAL1_TX (16) // P0.16, GNSS_RX
#define PIN_SERIAL2_RX (19) // P0.19, UART1_RX
#define PIN_SERIAL2_TX (20) // P0.20, UART1_TX

#define GPS_RX_PIN PIN_SERIAL1_RX
#define GPS_TX_PIN PIN_SERIAL1_TX
#define PIN_GPS_EN (17) // P0.17
#define GPS_EN_ACTIVE HIGH

// SPI0 is internal to the RAK4630 SX1262. SPI1 is wired to the E-ink panel.
#define SPI_INTERFACES_COUNT 2
#define PIN_SPI_MISO (45) // P1.13
#define PIN_SPI_MOSI (44) // P1.12
#define PIN_SPI_SCK (43)  // P1.11
#define PIN_SPI1_MISO (-1)
#define PIN_SPI1_MOSI (30) // P0.30, SPI_MOSI
#define PIN_SPI1_SCK (3)   // P0.03, SPI_SCK

static const uint8_t SS = 42;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK = PIN_SPI_SCK;

// RAK4630 internal SX1262
#define USE_SX1262
#define SX126X_CS (42)    // P1.10
#define SX126X_DIO1 (47)  // P1.15
#define SX126X_BUSY (46)  // P1.14
#define SX126X_RESET (38) // P1.06
#define SX126X_POWER_EN (37)
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8

// Confirmed Ieum PCB connections
#define IEUM_GNSS_RX_PIN GPS_RX_PIN
#define IEUM_GNSS_TX_PIN GPS_TX_PIN
#define IEUM_GNSS_EN_PIN PIN_GPS_EN

#define PIN_EINK_SCLK PIN_SPI1_SCK
#define PIN_EINK_MOSI PIN_SPI1_MOSI
#define PIN_EINK_CS (26)  // P0.26
#define PIN_EINK_DC (29)  // P0.29
#define PIN_EINK_RES (28) // P0.28
#define PIN_EINK_BUSY (2) // P0.02
#define PIN_EINK_EN (10)  // P0.10, active high
#define IEUM_EINK_EN_PIN PIN_EINK_EN

#define HAS_MMA8652FC
#define IEUM_MOTION_INT_PIN (9) // P0.09, direct from MMA8652FC INT1
#define MMA8652FC_INT_PIN IEUM_MOTION_INT_PIN
#define MMA8652FC_INT_ACTIVE HIGH
#define MMA8652FC_INT1
#define HAS_BQ25628E
#define IEUM_PMIC_INT_PIN (5) // P0.05, open-drain active-low pulse
#define IEUM_PMIC_INT_ACTIVE LOW
#define IEUM_PMIC_INT_PULSE_US 256
#define BQ25628E_WIRE Wire
#define BQ25628E_INT_PIN IEUM_PMIC_INT_PIN
#define BQ25628E_INT_ACTIVE IEUM_PMIC_INT_ACTIVE
#define BQ25628E_INPUT_CURRENT_LIMIT_MA 500
#define BQ25628E_CHARGE_CURRENT_LIMIT_MA 320
#define BQ25628E_CHARGE_VOLTAGE_LIMIT_MV 4200
#define BQ25628E_INPUT_OVP_MV 6300
#define BQ25628E_WATCHDOG_SECONDS 0
#define IEUM_BUTTON1_PIN PIN_BUTTON1
#define IEUM_BUTTON2_PIN PIN_BUTTON2
#define IEUM_LED1_PIN PIN_LED1
#define IEUM_LED2_PIN PIN_LED2

// Use native USB VBUS detection for the initial board-support build.
#define NRF_APM

#ifdef __cplusplus
}
#endif

#endif
