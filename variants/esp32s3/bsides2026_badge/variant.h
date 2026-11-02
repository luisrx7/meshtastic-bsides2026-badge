// BSides Lisboa 2026 badge ("StegoMatrix Access Terminal", PCB 0275)
// ESP32-S3-WROOM-1 N16R8 + Ra-01SC (SX1268, 433 MHz) + SSD1306 OLED. Pinout reverse-engineered, see HARDWARE.md.

// Screen: SSD1306 128x64 on its own I2C bus
#define USE_SSD1306
#define I2C_SDA 47
#define I2C_SCL 48

// Sensor bus (VEML6040 0x10, AGS10 0x1A, LSM6DS3 0x6A, SPA06 0x77)
#define HAS_SPA06 1
// One-point calibration: the compensation formula of the Adafruit driver reads 57.6 C where the room is 20.0 C (coefficients and
// raw value verified in the logs); the offset is a linear correction of that, not a model of the error
#define SPA06_TEMP_OFFSET_C -37.6f
#define HAS_VEML6040
#define HAS_AGS10
#define I2C_SDA1 1
#define I2C_SCL1 2

// PIN 0 button (BOOT button is wired to EN)
#define BUTTON_PIN 0
#define BUTTON_NEED_PULLUP

// 2x WS2812B addressable LEDs, daisy-chained on GPIO15. They are driven by variant.cpp (not by Meshtastic's ambient lighting):
// LED1 shows the air quality (IAQ from the AGS10), LED2 shows the microphone noise level. BADGE_LED_LEVEL is the channel value, 1 = dimmest.
#define BADGE_LED_PIN 15
// Wake-up threshold of the LSM6DS3 (2 g range: 1 step = 31 mg), the default 20 needs a hard shake
#define LSM6DS3_WAKE_THRESH 6
#define BADGE_LED_LEVEL 12

// PDM microphone (MSM261): clock and data only
#define BADGE_MIC_CLK 17
#define BADGE_MIC_DATA 18

// Capacitive touch pads TP1-TP4 = GPIO5-8: idle ~27000, touched ~226000 with touchRead().
// TP1 up (long: left), TP2 cancel, TP3 down (long: right), TP4 select; see src/input/TouchPads.cpp
#define INPUTBROKER_TOUCHPADS
#define TOUCHPAD_PINS 5, 6, 7, 8
// IR receiver on GPIO16 (idles HIGH, pulses LOW). Not used by Meshtastic yet.

// LoRa: Ra-01SC (SX1268). Crystal only, no TCXO. Internal RF switch driven by DIO2.
// NRESET was not found on any free GPIO; it is probably tied to EN or a pull-up.
// The module is marked LLCC68 (SX126x family); enable every SX126x variant so the radio probe picks the one RadioLib accepts.
#define USE_LLCC68
#define USE_SX1268
#define USE_SX1262

#define LORA_SCK 12
#define LORA_MISO 13
#define LORA_MOSI 11
#define LORA_CS 10
#define LORA_RESET RADIOLIB_NC

#define SX126X_CS LORA_CS
#define SX126X_DIO1 9
#define SX126X_BUSY 14
#define SX126X_RESET LORA_RESET
#define SX126X_DIO2_AS_RF_SWITCH
