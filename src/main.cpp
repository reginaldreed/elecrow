// Bring-up sketch for the CrowPanel ESP32 E-Paper 4.2" (green-sticker revision,
// UC8276C / DIE07300S controller).
//
// This board has TWO power rails for the panel: EPD_PWR_MAIN (IO41) and EPD_PWR
// (IO7). EPD_PWR_MAIN must go HIGH first, before any SPI traffic -- skip it and
// the panel still acknowledges commands and reports BUSY correctly, but no
// refresh ever actually completes. That's the fix from
// forum.elecrow.com/discussion/29480 (user stevemur), confirmed working here.
//
// Boot draws a full-refresh splash. Every button press then rewrites just the
// status line via partial refresh.

#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

#include "board_pins.h"

GxEPD2_BW<GxEPD2_420_SE0420NQ04, GxEPD2_420_SE0420NQ04::HEIGHT> display(
    GxEPD2_420_SE0420NQ04(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

struct Button {
  const char *name;
  int pin;
  bool was_down;
};

Button buttons[] = {
    {"UP", ROT_UP, false},   {"DOWN", ROT_DOWN, false}, {"CONF", ROT_CONF, false},
    {"MENU", BTN_MENU, false}, {"EXIT", BTN_EXIT, false},
};

constexpr int STATUS_Y = 240;
constexpr int STATUS_H = 60;

static uint32_t press_count = 0;

static void drawSplash() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(20, 60);
    display.print("CrowPanel 4.2");

    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(20, 110);
    display.print("ESP32-S3 / UC8276C");
    display.setCursor(20, 140);
    display.printf("%dx%d", display.width(), display.height());
    display.setCursor(20, 170);
    display.printf("PSRAM %u KB", (unsigned)(ESP.getPsramSize() / 1024));

    display.drawRect(10, 10, display.width() - 20, display.height() - 20, GxEPD_BLACK);
    display.drawFastHLine(10, STATUS_Y, display.width() - 20, GxEPD_BLACK);

    display.setCursor(20, STATUS_Y + 40);
    display.print("press any button");
  } while (display.nextPage());
}

static void drawStatus(const char *msg) {
  display.setPartialWindow(0, STATUS_Y + 1, display.width(), STATUS_H);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(20, STATUS_Y + 40);
    display.printf("%s  #%lu", msg, (unsigned long)press_count);
  } while (display.nextPage());
}

void setup() {
  Serial.begin(115200);
  delay(200);

  for (auto &b : buttons) {
    pinMode(b.pin, INPUT_PULLUP);
  }

  // Power rail order matters: MAIN before EPD, both before any SPI traffic.
  pinMode(EPD_PWR_MAIN, OUTPUT);
  digitalWrite(EPD_PWR_MAIN, HIGH);
  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, HIGH);
  delay(100);

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(115200, true, 2, false);
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.setRotation(0);

  Serial.printf("flash %u MB, psram %u KB\n", (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)),
                (unsigned)(ESP.getPsramSize() / 1024));

  drawSplash();
  Serial.println("splash drawn");
}

void loop() {
  for (auto &b : buttons) {
    bool down = digitalRead(b.pin) == LOW;
    if (down && !b.was_down) {
      press_count++;
      Serial.printf("%s pressed (#%lu)\n", b.name, (unsigned long)press_count);
      drawStatus(b.name);
    }
    b.was_down = down;
  }
  delay(20);  // cheap debounce
}
