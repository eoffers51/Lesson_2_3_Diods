# Lesson 2.3: Superloop and independent tasks

[![Simulate on Wokwi](https://img.shields.io/badge/simulate-Wokwi-6c4cbb)](https://wokwi.com/projects/479851584497659905)

Beetroot Academy homework ([`TASK.md`](TASK.md)), ESP-IDF v6.0.2 on an ESP32-S3-DevKitC-1.

Three external LEDs blink at different rates, each on its own schedule, from **one non-blocking loop** in `app_main`. No `vTaskDelay`-based blinking and no `while` that waits for time. Every LED checks the clock (`esp_timer_get_time()`, the ESP-IDF version of Arduino's `millis()`) and toggles when its interval has passed.

## Requirements check

| `TASK.md` rule | How the code meets it |
|---|---|
| `delay()` forbidden | No `delay`/`vTaskDelay`-based blinking. Each LED toggles only when `esp_timer_get_time()` says its interval has passed ([`main.c:78`](main/main.c#L78)). |
| No `while()` waiting for time inside `loop()` | The only loop is `while (1)` in `app_main` ([`main.c:70`](main/main.c#L70)). It plays the role of the code that calls Arduino's `loop()` over and over, and its body never waits: each pass checks the clock and moves on with an `if`. |
| `millis()` allowed | `esp_timer_get_time()` is used (µs since boot, `int64_t`). |
| `vTaskDelay(1)` at the end of a pass | It's a one-tick **yield** so the idle task can feed the task watchdog. The schedule doesn't depend on it ([`main.c:102`](main/main.c#L102)). |

## How the intervals are read

"LED1 blinks every 200 ms" is read as **the LED toggles every 200 ms**: 200 ms on, 200 ms off. One full blink (on + off) therefore takes **2 × the interval**.

| LED | GPIO | Toggles every | Full period (on + off) |
|---|---|---|---|
| LED1 | GPIO4 | 200 ms | 400 ms |
| LED2 | GPIO5 | 500 ms | 1000 ms |
| LED3 | GPIO6 | 1000 ms | 2000 ms |

## How the code works

All three LEDs live in one array of structs in [`main/main.c`](main/main.c). Each struct holds the LED's pin, its interval, the time it last toggled and its current level. The superloop runs the same check for every LED:

```c
if (now_us - led->last_toggle_us >= led->interval_us) {
    // toggle the LED, then move its clock forward by exactly one interval
    led->last_toggle_us += led->interval_us;
}
```

- `last_toggle_us += interval_us` (not `= now_us`) keeps each LED on its original schedule, so small delays don't add up over time.
- The level is kept in the struct, because `gpio_get_level()` reads 0 on a pin set to `GPIO_MODE_OUTPUT` (its input buffer is off).
- `vTaskDelay(1)` at the end of each pass is **not** used for timing. It only gives the idle task a tick so the task watchdog doesn't fire. With the 100 Hz tick, a toggle can therefore be up to about 10 ms late, but it never drifts.

## Solution 2: one FreeRTOS task per LED

[`main/main_rtos.c`](main/main_rtos.c) solves the same problem a different way. The superloop handles the scheduling itself by checking every LED's clock on every pass. Here the **FreeRTOS scheduler** does that job instead: `app_main` calls `xTaskCreate` three times, and each task runs the same `blink_task` function with a pointer to its own LED.

```c
TickType_t last_wake = s_start_tick;
for (;;) {
    xTaskDelayUntil(&last_wake, period); // Blocked here, uses no CPU
    level = !level;
    gpio_set_level(led->gpio, level);
}
```

- **Independent:** while one task waits in `xTaskDelayUntil`, it is *Blocked*, so the scheduler runs the other tasks. A slow LED never holds up a fast one.
- **No drift:** `xTaskDelayUntil` wakes at `last_wake + period` and moves `last_wake` forward by exactly one period. That's the RTOS version of `last_toggle_us += interval_us`. A plain `vTaskDelay(period)` would add the time spent toggling and logging to every period, so the LED would slowly drift.
- **Common start:** all tasks start counting from one shared `s_start_tick`, so the three LEDs toggle together at 1000 ms, 2000 ms, …
- **Resolution:** the timing is in FreeRTOS ticks (10 ms at `CONFIG_FREERTOS_HZ=100`), so 200/500/1000 ms are exactly 20/50/100 ticks.
- Each task costs its own stack (3 KB here), and `app_main` returns after creating the tasks, which is allowed in ESP-IDF.

| | Superloop (`main.c`) | FreeRTOS tasks (`main_rtos.c`) |
|---|---|---|
| Who decides when an LED toggles | The loop, by comparing timestamps | The scheduler, by waking a Blocked task |
| Waits for time | Never | Yes, but only inside each LED's own task |
| Memory | One stack | One stack per LED |
| Adding an LED | One line in the array | One line in the array (one more task) |

> **Which one is the homework?** `TASK.md` forbids waiting for time (`delay()`), so the graded answer is the superloop in `main.c`, and it's built by default. The RTOS version waits (`xTaskDelayUntil`) but only in each LED's own task, so it shows the other common way to run independent jobs on an ESP32.

### Building solution 2

Pick the solution in `idf.py menuconfig` → *Lesson 2.3 LEDs* → *Blink solution* (the choice lives in [`main/Kconfig.projbuild`](main/Kconfig.projbuild)), or build it in its own folder without touching the default build:

```powershell
idf.py -B build_rtos -D SDKCONFIG=build_rtos/sdkconfig -D SDKCONFIG_DEFAULTS=sdkconfig.rtos build
idf.py -B build_rtos -D SDKCONFIG=build_rtos/sdkconfig -D SDKCONFIG_DEFAULTS=sdkconfig.rtos -p COM5 flash monitor
```

Repeat the `-D` options on **every** `idf.py` command for `build_rtos`. Without `-D SDKCONFIG=...`, `idf.py` quietly uses the root `sdkconfig` (solution 1), even with `-B build_rtos`.

The Wokwi project and `wokwi.toml` run solution 1.

### Watching the tasks live in SystemView

[SEGGER SystemView](https://www.segger.com/products/development-tools/systemview/) shows which FreeRTOS task runs when, live while the board runs. For solution 2 you see `LED1`, `LED2` and `LED3` wake every 200, 500 and 1000 ms for a moment, then go back to *Blocked*, with `Idle` running the rest of the time. The trace goes out over the same COM port as flashing, so no extra cable is needed.

| File | What it does |
|---|---|
| [`sdkconfig.trace_uart`](sdkconfig.trace_uart) | Trace settings, added on top of `sdkconfig.rtos`: SystemView over UART0 (COM5) at 1 Mbaud, serial log and logging off, FreeRTOS on one core |
| [`main/idf_component.yml`](main/idf_component.yml) | Pulls in `espressif/esp_sysview` (SystemView moved out of ESP-IDF in v6.0), only when tracing is on. Pinned to `~1.0.2`: 1.1.x needs a newer `esp_trace` API than IDF v6.0.2 has |
| [`main/trace_params.c`](main/trace_params.c) | Trace build only. Tells ESP-IDF to open the `"sysview"` encoder. Without it the firmware aborts at boot with `Encoder 'ext' or transport 'apptrace' not found` |
| [`tools/sysview_bridge.py`](tools/sysview_bridge.py) | Connects SystemView to COM5 (see below) |

One-time setup: ESP-IDF numbers the FreeRTOS events its own way, so SystemView needs ESP-IDF's event description file. From an **administrator** PowerShell (the original is kept as a backup):

```powershell
$d = "C:\Program Files\SEGGER\SystemView\Description"
Copy-Item "$d\SYSVIEW_FreeRTOS.txt" "$d\SYSVIEW_FreeRTOS.txt.segger-original"
Copy-Item "$env:IDF_PATH\tools\esp_app_trace\SYSVIEW_FreeRTOS.txt" "$d\SYSVIEW_FreeRTOS.txt" -Force
```

Build and flash the trace firmware into its own folder `build_live/`. The first build downloads `esp_sysview` and stops with *"Re-run the build process"*; run the same command again.

```powershell
idf.py -B build_live -D SDKCONFIG=build_live/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.rtos;sdkconfig.trace_uart" -p COM5 build flash
python tools/sysview_bridge.py --port COM5
```

Then in SystemView: *Target → Recorder Configuration* → **IP**, address `127.0.0.1`, port `19111`, and *Target → Start Recording* (`F5`).

- **Why a bridge:** SystemView's own *UART* recorder stops with *"Error while recording"*. It starts with a short hello handshake that ESP-IDF's UART trace doesn't answer. The bridge answers it, then copies bytes both ways between SystemView's IP recorder and COM5. It also resets the board for every recording, so SystemView always gets the task list first. It holds COM5 until `Ctrl+C`, so stop it before flashing.
- **No serial log:** COM5 carries the trace, so `ESP_LOGI` is off in this build. Logging is turned off completely, so the LED tasks don't spend about 1 ms formatting lines nobody sees.
- **One core:** over UART SystemView traces one core, so this build runs FreeRTOS on core 0 only (`CONFIG_FREERTOS_UNICORE`). Every task is then in the trace.

What to look at:

- **Context window:** click the **Activations** header to sort. The rows that run come first: `LED1`, `LED2`, `LED3`, `Idle`, and the tick (`SYSTIMER_TARGET0` is the timer interrupt, with FreeRTOS's `SysTick` handler inside it, 100 per second). The many rows with 0 activations are just every interrupt the chip has; untick them to hide them from the timeline. In a 22.5 s recording: LED1 113, LED2 45 and LED3 22 activations (5 : 2 : 1), Idle 96 % of the CPU.
- **Timeline:** zoom out to about 1–2 s, otherwise the window is too short to contain an LED run. The gap between two runs of one task is its interval and doesn't grow (no drift).
- **Durations are inflated by tracing:** an LED run shows about 0.23 ms and a tick about 0.11 ms. Most of that is the time to push the trace events out of the UART. A bare pin toggle takes a few µs. The intervals are exact; the durations are mostly measurement overhead.

To get the normal firmware and log back: `idf.py -p COM5 flash`.

## Wiring

![Wiring: GPIO4, GPIO5 and GPIO6 each drive an LED through a 220 Ω resistor to GND](docs/wiring.svg)

Each LED: GPIO → 220 Ω resistor → LED anode (long leg), LED cathode (short leg) → GND. HIGH = on.

| LED | Board pin (silkscreen label) | Resistor | Return |
|---|---|---|---|
| LED1 | 4 | 220 Ω | GND |
| LED2 | 5 | 220 Ω | GND |
| LED3 | 6 | 220 Ω | GND |

Find the pins by the GPIO numbers printed next to them on the board. Logic is 3.3 V, and each LED draws about (3.3 − 2.0) / 220 ≈ 6 mA (assuming a red LED with Vf ≈ 2.0 V).

## Build and flash

Activate the ESP-IDF environment in the same PowerShell window, then:

```powershell
idf.py build
idf.py -p COM5 flash monitor
```

Exit the monitor with `Ctrl+]`.

## Simulate in Wokwi

**[▶ Open the Wokwi project](https://wokwi.com/projects/479851584497659905)** and press the green ▶ button. The first build can take a minute or two on Wokwi's free servers. Then watch the three LEDs blink at their own rates.

The Wokwi project runs this repo's own C code. Wokwi compiles ESP-IDF code with the Arduino ESP32 core, which already has its own `app_main()`. So [`wokwi/sketch.ino`](wokwi/sketch.ino) calls our `app_main`, renamed to `lesson_2_3_diods_app_main` in the Wokwi copy of `main.c`; see [`wokwi/README.md`](wokwi/README.md). The `ESP_LOGI` messages don't appear in Wokwi's serial monitor, so watch the LEDs instead.

- **VS Code:** install the *Wokwi Simulator* extension, run `idf.py build`, then run *Wokwi: Start Simulator*. This runs the real firmware: [`wokwi.toml`](wokwi.toml) points to the build, and [`diagram.json`](diagram.json) holds the circuit.

Wokwi blends LED brightness over about 100 ms, so in the simulator an LED's "on" time looks about 100 ms shorter and its "off" time about 100 ms longer. The full period (on + off) is not affected.

## Execution results

Run on 2026-11-05 in the Wokwi simulator.

### Serial log

The real ESP-IDF firmware from this repo (`idf.py build merge-bin`) running in Wokwi with *Upload Firmware and Start Simulation*. These are the first 24 lines. The log keeps going at about 8 lines per second (5 + 2 + 1 toggles).

```
I (209) main_task: Started on CPU0
I (209) main_task: Calling app_main()
I (219) leds: 3 LEDs ready, starting the superloop
I (429) leds: LED1 -> ON  at 563 ms
I (629) leds: LED1 -> OFF at 763 ms
I (729) leds: LED2 -> ON  at 863 ms
I (829) leds: LED1 -> ON  at 963 ms
I (1029) leds: LED1 -> OFF at 1163 ms
I (1229) leds: LED1 -> ON  at 1363 ms
I (1229) leds: LED2 -> OFF at 1363 ms
I (1229) leds: LED3 -> ON  at 1363 ms
I (1429) leds: LED1 -> OFF at 1563 ms
I (1629) leds: LED1 -> ON  at 1763 ms
I (1729) leds: LED2 -> ON  at 1863 ms
I (1829) leds: LED1 -> OFF at 1963 ms
I (2029) leds: LED1 -> ON  at 2163 ms
I (2229) leds: LED1 -> OFF at 2363 ms
I (2229) leds: LED2 -> OFF at 2363 ms
I (2229) leds: LED3 -> OFF at 2363 ms
I (2429) leds: LED1 -> ON  at 2563 ms
I (2629) leds: LED1 -> OFF at 2763 ms
I (2729) leds: LED2 -> ON  at 2863 ms
I (2829) leds: LED1 -> ON  at 2963 ms
I (3029) leds: LED1 -> OFF at 3163 ms
```

| LED | Toggle times from `esp_timer_get_time()` (ms) | Gap | Expected | Result |
|---|---|---|---|---|
| LED1 | 563, 763, 963, 1163, 1363, 1563, … | 200 ms | 200 ms | ✅ |
| LED2 | 863, 1363, 1863, 2363, 2863 | 500 ms | 500 ms | ✅ |
| LED3 | 1363, 2363 | 1000 ms | 1000 ms | ✅ |

- **The LEDs are independent.** At 1363 ms and 2363 ms, two or three LEDs toggle in the same loop pass, and none of them waits for another.
- **No drift.** Every gap is exact over 3 s, because each LED's clock moves forward by `+= interval`, not `= now`.
- **Two clocks.** The `I (…)` prefix is the log's own timestamp, and `at … ms` is `esp_timer_get_time()`. They differ by a constant 134 ms here, so the gaps are the same in both.

### LED timing in the shared Wokwi project

Measured by timing the simulated LED edges for 6 s in the [Wokwi project](https://wokwi.com/projects/479851584497659905). Each pair is one on time + one off time:

| LED | Full period expected | Full period in Wokwi (examples) | Result |
|---|---|---|---|
| LED1 | 400 ms | 98 + 298 = 396, 103 + 297 = 400, 104 + 300 = 404 ms | ✅ |
| LED2 | 1000 ms | 397 + 602 = 999, 401 + 597 = 998, 401 + 601 = 1002 ms | ✅ |
| LED3 | 2000 ms | 900 + 1097 = 1997, 906 + 1098 = 2004 ms | ✅ |

The uneven on/off split (about 100/300 ms instead of 200/200 ms) is Wokwi's LED smoothing, not the firmware: the serial log above shows exact 200 ms toggles. See [Simulate in Wokwi](#simulate-in-wokwi).
