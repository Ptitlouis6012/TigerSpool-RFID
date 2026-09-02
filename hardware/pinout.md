# Pinout

Quick reference. The reasoning, the failure modes and the bring-up checklist are
in **[../docs/WIRING.md](../docs/WIRING.md)** — read that before wiring a board
for the first time.

**Board:** Waveshare ESP32-S3-Touch-LCD-2 (ESP32-S3R8, 16 MB flash, 8 MB octal PSRAM)

## What you wire

| Function | ESP32-S3 GPIO | Connects to | Direction |
|---|---|---|---|
| PN532 power | `3V3` | PN532 `VCC` | — |
| Ground | `GND` | PN532 `GND` | — |
| UART1 RX | **44** | PN532 `TXD` | in |
| UART1 TX | **43** | PN532 `RXD` | out |
| Reader reset | **4** | PN532 `RSTO` | out |

UART1, 115200 baud, 8N1. PN532 DIP switches both `0` / OFF.

> ⚠️ **Not GPIO6/7.** They carry an I²C bus with pull-ups on this board and
> corrupt every read while appearing to work.

## Already on the board

Listed so you know which pins are taken. Nothing to wire here.

| Function | GPIO |
|---|---|
| LCD `SCLK` (SPI3) | 39 |
| LCD `MOSI` | 38 |
| LCD `MISO` | 40 |
| LCD `DC` | 42 |
| LCD `CS` | 45 |
| LCD backlight | 1 |
| Touch `SDA` (I²C0) | 48 |
| Touch `SCL` | 47 |

Touch controller: CST816S at address `0x15`. Display rotation: `2` (portrait,
180°).

## Unavailable pins

| GPIO | Reason |
|---|---|
| 0 | Strapping / BOOT |
| 6, 7 | On-board I²C with pull-ups |
| 19, 20 | Native USB |
| 26–32 | SPI flash |
| 33–37 | Octal PSRAM |
| 45, 46 | Strapping |
| 47, 48 | Touch I²C |

GPIO43/44 are free only because the console runs over USB-CDC. The build must
keep `-DARDUINO_USB_MODE=1` and `-DARDUINO_USB_CDC_ON_BOOT=1`.
