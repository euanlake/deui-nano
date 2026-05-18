#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"

/*
 * Waveshare ESP32-S3-Knob-Touch-LCD-1.8 (ESP32-S3R8, ST77916 QSPI + CST816).
 * LCD + touch lines match Waveshare wiki / reference board wiring.
 *
 * ESP32-S3 rotary encoder: VolosR KnobRGBControl + muness/roon-knob use ECA=8, ECB=7
 * (not GPIO1/2 — GPIO1 is battery ADC on this PCB).
 *
 * References: https://www.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8
 */

/** Display geometry and pixel format for the round controller panel. */
#define LM_CTRL_LCD_H_RES 360
#define LM_CTRL_LCD_V_RES 360
#define LM_CTRL_LCD_BPP 16
#define LM_CTRL_LCD_HOST SPI2_HOST
#define LM_CTRL_LCD_DRAW_BUF_ROWS 20
#define LM_CTRL_LCD_SPI_MAX_TRANSFER_SZ (LM_CTRL_LCD_H_RES * LM_CTRL_LCD_DRAW_BUF_ROWS * LM_CTRL_LCD_BPP / 8)

/** QSPI display bus (legacy: PCLK=13, CS=14, DATA0-3=15-18, RST=21, BL=47). */
#define LM_CTRL_LCD_SCL GPIO_NUM_13
#define LM_CTRL_LCD_CS GPIO_NUM_14
#define LM_CTRL_LCD_D0 GPIO_NUM_15
#define LM_CTRL_LCD_D1 GPIO_NUM_16
#define LM_CTRL_LCD_D2 GPIO_NUM_17
#define LM_CTRL_LCD_D3 GPIO_NUM_18
#define LM_CTRL_LCD_RST GPIO_NUM_21
#define LM_CTRL_LCD_TE GPIO_NUM_NC
#define LM_CTRL_LCD_BL GPIO_NUM_47

/** CST816S touch — Waveshare Knob 1.8 demo uses SDA=11, SCL=12, addr 0x15. */
#define LM_CTRL_TOUCH_HOST I2C_NUM_0
#define LM_CTRL_TOUCH_SDA GPIO_NUM_11
#define LM_CTRL_TOUCH_SCL GPIO_NUM_12
/*
 * Keep INT/RST unbound: Waveshare ESP-IDF `08_LVGL_Test` talks to touch over I2C only and
 * does not toggle these pins. Some board revisions may route them differently.
 */
#define LM_CTRL_TOUCH_INT GPIO_NUM_NC
#define LM_CTRL_TOUCH_RST GPIO_NUM_NC

/** Physical outer ring encoder pins and direction correction. */
#define LM_CTRL_KNOB_A GPIO_NUM_8
#define LM_CTRL_KNOB_B GPIO_NUM_7
#define LM_CTRL_KNOB_DIRECTION (-1)
#define LM_CTRL_ENCODER_ENABLED 1

/** Optional local battery telemetry (GPIO1 = ADC1_CH0,  ~2:1 divider to ESP32 on Knob 1.8). */
#define LM_CTRL_BATTERY_ADC_GPIO GPIO_NUM_1
/* Waveshare: charge status may be LED-only unless wired to a GPIO — verify schematic. */
#define LM_CTRL_BATTERY_CHARGE_GPIO GPIO_NUM_NC
#define LM_CTRL_BATTERY_CHARGE_ACTIVE_LEVEL 0
#define LM_CTRL_BATTERY_SAMPLE_INTERVAL_MS 30000UL
/* Cross-check: Waveshare `01_ADC_Test` uses the same 2:1 scaling on ADC1_CH0. */
#define LM_CTRL_BATTERY_VOLTAGE_DIVIDER 2.0f
#define LM_CTRL_BATTERY_LOW_PERCENT 20

/** Power policy defaults (single timeout for dimming/sleep entry). */
#define DEUI_POWER_IDLE_TIMEOUT_MS 300000UL
#define DEUI_POWER_ACTIVE_BRIGHTNESS_PERCENT 100
#define DEUI_POWER_DIM_BRIGHTNESS_PERCENT 1

/*
 * RGB ring — verify against your exact SKU / factory demo if LEDs do not light.
 * GPIO0 is a strapping pin; many ESP32-S3 boards still use it for one-wire LEDs.
 */
#define LM_CTRL_LED_RING_GPIO GPIO_NUM_0
#define LM_CTRL_LED_RING_COUNT 13
