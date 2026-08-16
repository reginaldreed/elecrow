# CrowPanel ESP32 E-Paper 4.2" — Device Spec

Reference for starting new projects on this board. Everything here was verified
against real hardware in this repo, not copied from Elecrow's docs — where those
docs are wrong, that's called out explicitly.

Board: [Elecrow CrowPanel ESP32 E-Paper 4.2"](https://www.elecrow.com/wiki/CrowPanel_ESP32_E-paper_4.2-inch_HMI_Display.html),
**green-sticker revision** specifically (check the back of the board — a green
circular sticker marks this controller variant; other revisions use a different
panel and pin-for-pin-different init, see [Board revisions](#board-revisions)).

## Hardware

| | |
|---|---|
| MCU | ESP32-S3-WROOM-1-N8R8 — 240 MHz, dual-core |
| Flash | 8 MB, quad SPI |
| PSRAM | 8 MB, octal SPI |
| Display | 4.2" AM EPD, **UC8276C controller** (DIE07300S panel), 400×300, 1bpp B/W |
| Refresh | Full ~3.1 s, partial ~0.6 s (measured) |
| Input | Rotary switch (up/down/press), Menu button, Exit button |
| Storage | microSD (TF) slot, independent SPI bus |
| Power | SH1.0 2-pin JST, 3.7 V LiPo, onboard charge circuit |
| USB | CH340 USB-UART bridge (not native USB CDC) |
| Operating temp | 0–50°C |

## Pin map (verified)

```cpp
// E-paper panel
constexpr int EPD_PWR_MAIN = 41;  // rail 1 — see Power-up sequence
constexpr int EPD_PWR      = 7;   // rail 2
constexpr int EPD_CS       = 45;
constexpr int EPD_DC       = 46;
constexpr int EPD_RST      = 47;
constexpr int EPD_BUSY     = 48;  // active LOW on this controller
constexpr int EPD_SCK      = 12;
constexpr int EPD_MOSI     = 11;
// no MISO — e-paper is write-only

// Rotary switch + buttons (active LOW, need INPUT_PULLUP)
constexpr int ROT_UP    = 6;
constexpr int ROT_DOWN  = 4;
constexpr int ROT_CONF  = 5;
constexpr int BTN_MENU  = 2;
constexpr int BTN_EXIT  = 1;

// microSD (separate SPI bus from the panel)
constexpr int SD_CS   = 10;
constexpr int SD_MISO = 13;
constexpr int SD_SCK  = 39;
constexpr int SD_MOSI = 40;
```

Free GPIO for your own use: IO3, IO8, IO9, IO14–21, IO38.

## The one thing that will burn you: power-up sequence

This board gates the panel through **two separate power rails**, and Elecrow's
own factory firmware, wiki, and most third-party writeups only mention one
(`EPD_PWR` / IO7). The second rail, `EPD_PWR_MAIN` (IO41), is undocumented
outside a single forum thread.

```cpp
pinMode(EPD_PWR_MAIN, OUTPUT);
digitalWrite(EPD_PWR_MAIN, HIGH);
pinMode(EPD_PWR, OUTPUT);
digitalWrite(EPD_PWR, HIGH);
delay(100);
// only now touch SPI / call display.init()
```

**Get the order or presence of this wrong and the failure is silent and
misleading**: the panel still resets correctly, still acknowledges every SPI
command, BUSY still transitions correctly — but no refresh ever actually
completes, and the screen never changes. This is indistinguishable from a dead
panel without an oscilloscope or exactly this knowledge. It happens even when
running Elecrow's own factory firmware, because those images target other
board revisions that don't have this second rail.

If you ever see "commands accepted, BUSY behaves, nothing draws" on this
board, this is the first thing to check — before assuming hardware fault, wrong
pin map, or wrong panel class.

## Panel driver

Use **GxEPD2**, panel class `GxEPD2_420_SE0420NQ04` (lives in GxEPD2's `other/`
folder — not the more commonly referenced `GxEPD2_420` or
`GxEPD2_420_GDEY042T81`, both of which are for different controllers and will
silently fail the same way as the missing power rail, above).

```cpp
#include <SPI.h>
#include <GxEPD2_BW.h>

GxEPD2_BW<GxEPD2_420_SE0420NQ04, GxEPD2_420_SE0420NQ04::HEIGHT> display(
    GxEPD2_420_SE0420NQ04(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

void setup() {
  // ... power-up sequence above ...
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(115200, true, 2, false);
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.setRotation(0);
}
```

- Full refresh (`display.setFullWindow()`): ~3.1 s, clears ghosting, use for
  first draw and any large content change.
- Partial refresh (`display.setPartialWindow(x, y, w, h)`): ~0.6 s, some
  ghosting accumulates over repeated partials — do a periodic full refresh
  (e.g. every N partials, or once per session) to clear it.
- `display.hibernate()` after drawing cuts panel power draw to near zero;
  the MCU can still run normally, or go to deep sleep separately.

Startup will log `IO 45/47/46 is not set as GPIO` warnings from the ESP32
Arduino core — this is a harmless GxEPD2 ordering quirk (it probes
`digitalWrite` before `pinMode` once during `init()`) and self-resolves within
the same call. Ignore it.

## Input handling

Buttons and the rotary switch are all active-LOW with `INPUT_PULLUP`, polled
(no interrupts wired). ~20 ms poll interval is enough debounce in practice:

```cpp
struct Button { const char *name; int pin; bool was_down; };
Button buttons[] = {
  {"UP", ROT_UP, false}, {"DOWN", ROT_DOWN, false}, {"CONF", ROT_CONF, false},
  {"MENU", BTN_MENU, false}, {"EXIT", BTN_EXIT, false},
};

void loop() {
  for (auto &b : buttons) {
    bool down = digitalRead(b.pin) == LOW;
    if (down && !b.was_down) { /* edge: b.name pressed */ }
    b.was_down = down;
  }
  delay(20);
}
```

## microSD

Separate SPI bus from the panel (SD_SCK=39, SD_MOSI=40, SD_MISO=13, SD_CS=10)
— can be active concurrently with the panel without bus contention. Standard
Arduino `SD` library works; no board-specific quirks found yet.

## Power / battery

- SH1.0 2-pin JST, 3.7 V LiPo, onboard charge management — no charge-status
  GPIO identified yet.
- For battery-powered projects: `display.hibernate()` after every draw, and
  put the ESP32-S3 into deep sleep between updates. Not yet measured on this
  board — budget for it before committing to a battery-life target, since the
  two-rail panel power gate may itself draw standby current worth accounting
  for (drive both rails LOW when sleeping, not just skip touching them).

## Toolchain

PlatformIO in a local venv — Homebrew's Python is 3.14, which PlatformIO 6.1
rejects (needs 3.10–3.13):

```bash
python3.12 -m venv .venv
.venv/bin/pip install platformio pyyaml   # pyyaml: espressif32's build script needs it, not auto-installed
```

`platformio.ini` baseline for this board:

```ini
[env:crowpanel_42e]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

board_build.arduino.memory_type = qio_opi   ; N8R8: 8MB flash + 8MB PSRAM
board_build.flash_mode = qio
board_build.psram_type = opi
board_upload.flash_size = 8MB
board_build.partitions = default_8MB.csv

upload_port = /dev/cu.usbserial-XXXX   ; CH340 bridge, not /dev/cu.usbmodem*
monitor_port = /dev/cu.usbserial-XXXX
monitor_speed = 115200

build_flags =
  -D ARDUINO_USB_MODE=1
  -D ARDUINO_USB_CDC_ON_BOOT=0   ; must be 0 — no native USB wired to Serial
  -D BOARD_HAS_PSRAM

lib_deps =
  zinggjm/GxEPD2@^1.6.2
  adafruit/Adafruit GFX Library@^1.11.11
```

Build/flash/monitor:

```bash
.venv/bin/pio run -t upload -t monitor
```

`pio device monitor` needs a real TTY and fails under a non-interactive shell
(e.g. driven by an agent) with `termios.error: Operation not supported`. For
scripted log capture, talk to the port with `pyserial` directly, pulsing
DTR/RTS to reset the board and catch output from boot:

```python
import serial, time
s = serial.Serial('/dev/cu.usbserial-XXXX', 115200, timeout=1)
s.setDTR(False); s.setRTS(True); time.sleep(0.1)
s.setRTS(False); time.sleep(0.1)
s.reset_input_buffer()
# then read lines for however long you need
```

If the board doesn't enumerate a `/dev/cu.usbserial-*` device: hold **BOOT**,
tap **RESET**, release **BOOT** to force the ROM bootloader.

## Board revisions

Elecrow ships multiple controllers under this exact SKU, distinguishable only
by physical markings (a green circular sticker, or silkscreen revision text —
inconsistent across units). Confirmed distinct so far:

| Marking | Controller | GxEPD2 class | BUSY polarity |
|---|---|---|---|
| Green circular sticker | UC8276C (DIE07300S) | `GxEPD2_420_SE0420NQ04` | active LOW |
| (other / unmarked units reported) | SSD1683 | `GxEPD2_420_GDEY042T81` | active HIGH |

Symptoms of picking the wrong class/pins for your revision: the panel resets,
BUSY toggles, commands are acknowledged — but the screen never changes, or
changes and then reverts. This looks exactly like the missing-power-rail
failure above and like a dead panel; check board markings and confirm the
controller family before assuming hardware fault.

## Recommended project structure

```
project/
  platformio.ini
  include/
    board_pins.h       # the pin map above, unmodified
  src/
    main.cpp
  lib/                  # any project-specific modules
```

Start every new project from the `board_pins.h` + power-up sequence above
verbatim — that's the part that's expensive to rediscover, not the
application logic on top.
