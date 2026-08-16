# CrowPanel ESP32 E-Paper 4.2"

Bring-up project for the [Elecrow CrowPanel ESP32 E-Paper 4.2-inch HMI Display](https://www.elecrow.com/wiki/CrowPanel_ESP32_E-paper_4.2-inch_HMI_Display.html)
— specifically the **green-sticker revision** (UC8276C / DIE07300S controller).

## Hardware

| | |
|---|---|
| MCU | ESP32-S3-WROOM-1-N8R8, 240 MHz, 8 MB flash, 8 MB PSRAM |
| Display | 4.2" AM EPD, UC8276C (DIE07300S), 400×300, black/white, partial refresh |
| Input | Rotary switch (up/down/conf), Menu + Exit buttons |
| Storage | TF card slot on its own SPI bus |
| Power | SH1.0 2-pin JST for 3.7 V LiPo, onboard charger |

Pin map lives in [board_pins.h](include/board_pins.h). Free GPIO for your own use:
IO3, IO8, IO9, IO14–21, IO38.

## Build and flash

PlatformIO lives in a local venv — Homebrew's Python is 3.14, which PlatformIO 6.1
rejects (it supports 3.10–3.13), so this project pins 3.12:

```bash
python3.12 -m venv .venv && .venv/bin/pip install platformio pyyaml
```

`pyyaml` is not pulled in automatically but the espressif32 platform's build script
imports it. Then:

```bash
.venv/bin/pio run -t upload -t monitor
```

The board enumerates as a USB-UART bridge (CH340), not native USB CDC — expect
something like `/dev/cu.usbserial-*`, set as `upload_port`/`monitor_port` in
[platformio.ini](platformio.ini).

## What the bring-up sketch does

Full-refresh splash on boot, then each button press rewrites the bottom status line
via partial refresh and logs to serial at 115200.

## Gotcha: two power rails, and order matters

This board gates the panel through **two** rails: `EPD_PWR_MAIN` (IO41) and
`EPD_PWR` (IO7). `EPD_PWR_MAIN` must go HIGH first, before `EPD_PWR` and before
any SPI traffic.

Skip it and the failure is silent and misleading: the panel still resets
correctly, still acknowledges SPI commands, and BUSY still reports success —
but no refresh ever actually completes, and the screen never changes. This
looks exactly like a dead panel (which is what we initially concluded) and
happens even with Elecrow's own factory firmware for other board revisions,
since those don't drive IO41 either.

Fix credit: [forum.elecrow.com/discussion/29480](https://forum.elecrow.com/discussion/29480/crowpanel-4-2-e-paper-die07300s-v1-0-panel-frozen-wont-update-even-with-official-factory-fw/p2)
(user [stevemur](https://forum.elecrow.com/profile/stevemur)).

## Panel class

Uses GxEPD2's `GxEPD2_420_SE0420NQ04` (UC8276C, 400×300, BUSY active LOW).
Elecrow has shipped several different controllers under this SKU across board
revisions — if you have a different one, expect to swap this class and possibly
the pin map.
