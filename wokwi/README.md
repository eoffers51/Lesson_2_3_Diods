# Wokwi-only files

The [Wokwi project](https://wokwi.com/projects/479851584497659905) holds a copy of this repo's code so it runs in the browser. Wokwi compiles ESP-IDF code with the **Arduino ESP32 core**, which needs one small change:

| File in Wokwi | Comes from |
|---|---|
| `sketch.ino` | [`wokwi/sketch.ino`](sketch.ino): `setup()` calls our `app_main`, renamed to `lesson_2_3_diods_app_main` (the Arduino core already has an `app_main`) |
| `main.c` | [`main/main.c`](../main/main.c) with `#define app_main lesson_2_3_diods_app_main` added as the first line |
| `diagram.json` | [`diagram.json`](../diagram.json) |

When the code changes, regenerate the Wokwi copy of `main.c` (the `#define` line plus `main/main.c`), upload it to the Wokwi project (file menu ▾ → *Upload file(s)…*), and press **Save**.
