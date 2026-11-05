// Solution 2: one FreeRTOS task per LED.
//
// Built instead of main.c when "Blink solution" in menuconfig is set to
// "One FreeRTOS task per LED" (see main/Kconfig.projbuild and README).
//
// Idea: the superloop in main.c does the scheduling itself (it checks every
// LED's clock on every pass). Here the FreeRTOS scheduler does that job.
// Each LED gets its own task with its own simple loop "wait, toggle, repeat".
// While a task waits it is Blocked and uses no CPU, so the other tasks run.
#include <stdint.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "leds_rtos";

// External LEDs: GPIO -> 220 ohm -> LED -> GND, so HIGH = on (active HIGH).
#define LED1_GPIO GPIO_NUM_4
#define LED2_GPIO GPIO_NUM_5
#define LED3_GPIO GPIO_NUM_6

// Time between two toggles. One full blink (on + off) takes twice as long.
#define LED1_INTERVAL_MS 200
#define LED2_INTERVAL_MS 500
#define LED3_INTERVAL_MS 1000

// Stack size in BYTES (ESP-IDF's FreeRTOS counts bytes, not words).
// ESP_LOGI formats a string on the stack, so 2 KB would be tight.
#define BLINK_TASK_STACK_BYTES 3072
// All blink tasks get the same priority. Above idle (0), so they run on time.
#define BLINK_TASK_PRIORITY (tskIDLE_PRIORITY + 2)

// What one task needs to know about its LED. The tasks only read this.
typedef struct {
    const char *name; // task name, shows up in logs and debuggers
    gpio_num_t gpio;
    uint32_t interval_ms;
} blink_led_t;

static const blink_led_t s_leds[] = {
    {.name = "LED1", .gpio = LED1_GPIO, .interval_ms = LED1_INTERVAL_MS},
    {.name = "LED2", .gpio = LED2_GPIO, .interval_ms = LED2_INTERVAL_MS},
    {.name = "LED3", .gpio = LED3_GPIO, .interval_ms = LED3_INTERVAL_MS},
};
#define LED_COUNT (sizeof(s_leds) / sizeof(s_leds[0]))

// The tick when all LEDs start counting. app_main writes it once BEFORE it
// creates any task, and after that the tasks only read it, so no lock is needed.
static TickType_t s_start_tick;

// The same function runs in three tasks. Each task gets a pointer to its own
// LED in `arg`, and its local variables (level, last_wake) live on its own stack.
static void blink_task(void *arg)
{
    const blink_led_t *led = (const blink_led_t *)arg;

    // pdMS_TO_TICKS rounds DOWN to whole ticks. With CONFIG_FREERTOS_HZ=100 one
    // tick is 10 ms, so 200/500/1000 ms are exact (20/50/100 ticks).
    const TickType_t period = pdMS_TO_TICKS(led->interval_ms);

    uint32_t level = 0; // only this task touches this LED, so no sharing problems
    TickType_t last_wake = s_start_tick;

    for (;;) {
        // xTaskDelayUntil, not vTaskDelay: it wakes at last_wake + period and
        // then moves last_wake forward by exactly one period. The time spent on
        // gpio_set_level and logging is not added on top, so the LED never drifts.
        // It is the RTOS version of `last_toggle_us += interval_us` in main.c.
        xTaskDelayUntil(&last_wake, period);

        level = !level;
        ESP_ERROR_CHECK(gpio_set_level(led->gpio, level));

        // last_wake is the tick this toggle was due at. Convert it to ms since boot.
        ESP_LOGI(TAG, "%s -> %s at %lu ms", led->name, level ? "ON " : "OFF",
                 (unsigned long)(last_wake * portTICK_PERIOD_MS));
    }
    // A FreeRTOS task must never return. If it had to stop, it would call
    // vTaskDelete(NULL) here.
}

void app_main(void)
{
    // Step 1: configure all LED pins as outputs, same as in main.c.
    uint64_t pin_mask = 0;
    for (size_t i = 0; i < LED_COUNT; i++) {
        pin_mask |= 1ULL << s_leds[i].gpio;
    }
    const gpio_config_t led_config = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_config));
    for (size_t i = 0; i < LED_COUNT; i++) {
        ESP_ERROR_CHECK(gpio_set_level(s_leds[i].gpio, 0));
    }

    // Step 2: one shared start tick, so all three schedules line up
    // (LED1, LED2 and LED3 toggle together at 1000 ms, 2000 ms, ...).
    s_start_tick = xTaskGetTickCount();

    // Step 3: one task per LED. xTaskCreate copies nothing from s_leds[i], it
    // just passes the pointer, which is fine because s_leds is static.
    for (size_t i = 0; i < LED_COUNT; i++) {
        const BaseType_t ok = xTaskCreate(blink_task, s_leds[i].name, BLINK_TASK_STACK_BYTES, (void *)&s_leds[i],
                                          BLINK_TASK_PRIORITY, NULL);
        if (ok != pdPASS) {
            // Only fails when there is not enough heap for the stack.
            ESP_LOGE(TAG, "xTaskCreate(%s) failed: out of memory", s_leds[i].name);
            abort();
        }
    }
    ESP_LOGI(TAG, "%d blink tasks started", (int)LED_COUNT);

    // Step 4: nothing left to do. In ESP-IDF, returning from app_main is
    // allowed: the main task deletes itself and the three blink tasks keep running.
}
