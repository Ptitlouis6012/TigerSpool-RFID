#pragma once
#include <Arduino.h>

// ---- PN532 over HSU / UART1 (free pins; GPIO6/7 is I2C with pull-ups) -----
#define PN532_UART_NUM   1
#define PN532_UART_RX    44      // ESP32 RX  <- PN532 TXD
#define PN532_UART_TX    43      // ESP32 TX  -> PN532 RXD
#define PN532_UART_BAUD  115200

// ---- Panel -------------------------------------------------------------
#define SCR_W  240
#define SCR_H  320
#define SCR_ROTATION  2          // portrait, rotated 180
