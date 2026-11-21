# Meshtastic for the BSides Lisboa 2026 badge

Turn the BSides Lisboa 2026 badge (ESP32-S3 + Ra-01SC or Ra-01SH LoRa) into a working [Meshtastic](https://meshtastic.org) node.
This is a fork of [meshtastic/firmware](https://github.com/meshtastic/firmware) with a `bsides2026_badge` variant. It is not an official Meshtastic release.

> **Warning:** unlocking erases everything on the badge (contacts, Stegomatrix messages, high scores, the game firmware). It cannot be undone, and secure download mode and disabled JTAG are permanent: you can write the flash but never read it back. Final scores stay on the BSides website.

## What you get

| Part | Function |
|---|---|
| LoRa Ra-01SC (LLCC68, 433 MHz) or Ra-01SH (SX1262, 868 MHz) | Meshtastic radio, auto-detected. Region EU_433 (slot 4) or EU_868 (slot 1) for Meshtastic PT |
| OLED + 4 touch pads | Menu: TP1 up (long = left), TP2 back, TP3 down (long = right), TP4 select |
| WS2812B LED 1 | Air quality from the AGS10 (blue = starting, green good, yellow moderate, red poor) |
| WS2812B LED 2 | Noise level from the PDM microphone (off when quiet, green / yellow / red as it gets louder) |
| Both LEDs | Blink cyan for 6 s when a text message arrives |
| SPA06, VEML6040, AGS10 | Temperature, pressure, light and air quality in the Meshtastic telemetry |
| LSM6DS3 | Touch or shake the badge to wake the screen |

A received text message opens directly on the screen instead of showing a "New Message" banner.

## 1. Unlock the badge (once)

You need a computer, a USB-C **data** cable (charge-only cables do not work) and a serial terminal at 115200 baud (Arduino IDE Serial Monitor, PuTTY, `screen` or `python -m serial.tools.miniterm`).

1. Plug in the badge and open its serial port (Windows: the COM port; Linux: `/dev/ttyACM0`; macOS: `/dev/cu.usbmodem*`). Press Enter and type `help`.
2. Open the official page <https://bsl.arialives.com/unlock/> to get the unlock code for your badge.
3. Type these commands in order, each confirmation within 60 seconds. The code is case-insensitive, dashes are optional, and the badge accepts one attempt every 3 seconds:
   ```
   unlock <your-code>
   unlock confirm
   unlock ERASE
   ```
   `ERASE` must be in capitals. The badge erases its data, disables flash encryption and resets. It stays silent until you flash new firmware.

## 2. Flash Meshtastic

1. Install Python and esptool: `pip install esptool`
2. Download both files from the [latest release](../../releases/latest) into one folder: `bsides2026-badge-meshtastic.merged.bin` and `flash_badge.py`.
3. Put the badge in download mode: **hold the PIN 0 (IO0) button while plugging in the USB-C cable**.
4. Run the script with your port:
   ```
   python flash_badge.py COM3
   ```
   (Linux: `/dev/ttyACM0`.) It takes about two minutes.
5. Unplug the cable and plug it in again **without pressing any button**. On the first boot, pick the LoRa region that matches your module (EU_433 for the Ra-01SC, EU_868 for the Ra-01SH).

### Why a script and not the plain esptool command

The official page flashes with
`esptool --chip esp32s3 -p PORT --no-stub write-flash --no-compress 0x0 firmware.merged.bin`.
That works for small images, but in secure download mode esptool cannot read the flash size, and writes beyond about 2.3 MB fail with "Failed to enter flash download mode". `flash_badge.py` adds `--flash-size 16MB` and writes the image in 256 KB blocks. If a flash is interrupted, start again from step 3 and rewrite the whole image: partial flashes leave "Checksum failed" boot errors.

### Troubleshooting

- **No port appears:** hold IO0 while plugging in the cable.
- **Nothing on the screen after flashing:** unplug and replug the cable without pressing buttons.
- **Flash fails halfway:** re-enter download mode and run the script again.

## Build from source

```
pio run -e bsides2026-badge
```

The output is `.pio/build/bsides2026-badge/firmware-bsides2026-badge-*.factory.bin`. Merge it with `esptool --chip esp32s3 merge-bin -o badge.merged.bin --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 <factory.bin>`.
The variant lives in `variants/esp32s3/bsides2026_badge/`. PlatformIO's `pioarduino` platform needs PlatformIO 6.2.0 or newer.

## Hardware notes

- I2C bus A (SDA 1, SCL 2): VEML6040 0x10, AGS10 0x1A, LSM6DS3 0x6A, SPA06 0x77. Bus B (SDA 47, SCL 48): OLED 0x3C.
- Radio (same pinout for Ra-01SC and Ra-01SH): NSS 10, SCK 12, MOSI 11, MISO 13, BUSY 14, DIO1 9, no reset pin, no TCXO.
- WS2812B x2 on GPIO15, touch pads on GPIO5 to GPIO8, PDM microphone CLK 17 / DATA 18, IR receiver GPIO16.
- The SPA06 temperature is calibrated with a fixed offset of -37.6 C against a 20 C room; the cause of the raw error was not found.
