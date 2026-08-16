// Pin map for the CrowPanel ESP32 E-Paper 4.2" HMI Display (ESP32-S3-WROOM-1-N8R8).
#pragma once

// --- E-paper panel (UC8276C / DIE07300S, 400x300, SPI, green-sticker revision) ---
// Two power rails gate the panel, and order matters: EPD_PWR_MAIN must go HIGH
// before EPD_PWR (and before any SPI traffic) or the panel accepts commands and
// even reports success on BUSY, but never completes a real refresh.
constexpr int EPD_PWR_MAIN = 41;
constexpr int EPD_PWR = 7;

constexpr int EPD_W = 400;
constexpr int EPD_H = 300;
constexpr int EPD_BUSY = 48;
constexpr int EPD_RST = 47;
constexpr int EPD_DC = 46;
constexpr int EPD_CS = 45;
constexpr int EPD_SCK = 12;
constexpr int EPD_MOSI = 11;

// --- Rotary switch + buttons (active low, need INPUT_PULLUP) ---
constexpr int BTN_EXIT = 1;
constexpr int BTN_MENU = 2;
constexpr int ROT_DOWN = 4;
constexpr int ROT_CONF = 5;
constexpr int ROT_UP = 6;

// --- TF card slot (separate SPI bus from the panel) ---
constexpr int SD_CS = 10;
constexpr int SD_MISO = 13;
constexpr int SD_SCK = 39;
constexpr int SD_MOSI = 40;
