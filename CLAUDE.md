# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

ESP-IDF v6.0.2 project for an **ESP32-S3-DevKitC-1** clone (Beetroot Academy homework "Superloop and Independent Tasks"). The requirements are in `TASK.md`: three LEDs blink independently at 200 ms, 500 ms and 1000 ms from one non-blocking loop. Check changes against it.

## Build / flash

`idf.py` is not on PATH. Activate the IDF env in the same PowerShell call as every `idf.py` command (shell state doesn't persist between calls):

```powershell
. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1 *> $null; idf.py build
```

- Activating the env also puts Espressif's `cmake`/`ninja` ahead of the STM32CubeCLT ones on PATH, so never build without it.
- `sdkconfig` is gitignored and may not exist. If it's missing, run `idf.py set-target esp32s3` before the first build.
- Flash with `idf.py -p COM5 flash` (or use `/build-flash`). Never run `idf.py monitor`: it's interactive and will hang, so ask the user to run it.
- clangd reads `compile_commands.json` from a hardcoded `build/` path in `.vscode/settings.json`, so IDE diagnostics are only valid after a first build.

## Task rules (graded)

`TASK.md` is written in Arduino terms. In ESP-IDF they mean:
- No blocking delays for timing (`delay()` → no `vTaskDelay`-based blinking).
- No `while` loop that busy-waits for time inside the loop body.
- `millis()` is allowed → use `esp_timer_get_time()` (µs) and compare against each LED's last-toggle time.
- The main loop may still need a short yield (e.g. `vTaskDelay(1)`) so the idle task's watchdog doesn't fire. That yield is not used for timing.

## Hardware

- External LEDs, each wired GPIO → 220 Ω → LED anode, cathode → GND (active HIGH): **LED1 = GPIO4** (200 ms), **LED2 = GPIO5** (500 ms), **LED3 = GPIO6** (1000 ms).
- The onboard RGB LED is a WS2812 on GPIO48. It is addressable, so `gpio_set_level` won't drive it. It's not one of the three plain LEDs.
- The wiring is drawn in `docs/wiring.svg` (README picture) and `diagram.json` (Wokwi). If a pin changes, update both.
- Serial port is COM5 (CH343 USB-UART). The small green LED that flickers with log output is the UART TX light, not an app LED.

## Repo

The project goes in GitHub repo `eoffers51/Lesson_2_3_Diods` (not created yet; follow the academy rules in the parent CLAUDE.md).

## Code style

Formatting follows `.clang-format` (ESP-IDF style: 4 spaces, function braces on their own line). A hook applies it after edits.
