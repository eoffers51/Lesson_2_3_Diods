#include <stdint.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "leds";

// External LEDs: GPIO -> 220 ohm -> LED -> GND, so HIGH = on (active HIGH).
#define LED1_GPIO GPIO_NUM_4
#define LED2_GPIO GPIO_NUM_5
#define LED3_GPIO GPIO_NUM_6

// Time between two toggles. One full blink (on + off) takes twice as long.
#define LED1_INTERVAL_MS 200
#define LED2_INTERVAL_MS 500
#define LED3_INTERVAL_MS 1000

// esp_timer_get_time() counts in microseconds, so all times are kept in us.
// The int64_t cast makes the multiply 64-bit, so big values can't overflow.
#define MS_TO_US(ms) ((int64_t)(ms) * 1000)

// Everything one LED needs to blink on its own. Each LED keeps its own clock.
typedef struct {
    gpio_num_t gpio;        // which pin drives this LED
    int64_t interval_us;    // how long to wait between toggles
    int64_t last_toggle_us; // when this LED last toggled (or was due to)
    uint32_t level;         // what we last wrote to the pin: 0 = off, 1 = on
} blink_led_t;

// The only list of LEDs. The pin mask, the setup and the main loop all read
// from it, so adding a 4th LED means adding one line here.
static blink_led_t s_leds[] = {
    {.gpio = LED1_GPIO, .interval_us = MS_TO_US(LED1_INTERVAL_MS)},
    {.gpio = LED2_GPIO, .interval_us = MS_TO_US(LED2_INTERVAL_MS)},
    {.gpio = LED3_GPIO, .interval_us = MS_TO_US(LED3_INTERVAL_MS)},
};
#define LED_COUNT (sizeof(s_leds) / sizeof(s_leds[0]))

void app_main(void)
{
    // Step 1: build the pin mask from the array, so the pins are written down only once.
    uint64_t pin_mask = 0;
    for (size_t i = 0; i < LED_COUNT; i++) {
        pin_mask |= 1ULL << s_leds[i].gpio; // 1ULL: GPIO numbers go above 31, so the bit must be 64-bit
    }

    const gpio_config_t led_config = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,          // push-pull output: drives both HIGH and LOW
        .pull_up_en = GPIO_PULLUP_DISABLE, // the pin drives the line itself, no pull needed
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE, // outputs don't need interrupts
    };
    // A wrong pin config is a bug we can't recover from, so stop with a clear error.
    ESP_ERROR_CHECK(gpio_config(&led_config));

    // Step 2: start every LED off and start every LED's clock at the same moment.
    const int64_t start_us = esp_timer_get_time();
    for (size_t i = 0; i < LED_COUNT; i++) {
        s_leds[i].level = 0;
        s_leds[i].last_toggle_us = start_us;
        ESP_ERROR_CHECK(gpio_set_level(s_leds[i].gpio, s_leds[i].level));
    }
    ESP_LOGI(TAG, "%d LEDs ready, starting the superloop", (int)LED_COUNT);

    // Step 3: the superloop (Arduino's loop()). Each pass only checks the clock
    // and returns at once. It never waits for time, so all LEDs run independently.
    while (1) {
        const int64_t now_us = esp_timer_get_time();

        for (size_t i = 0; i < LED_COUNT; i++) {
            blink_led_t *led = &s_leds[i];

            // Subtracting (instead of comparing to a stored "next time") is the
            // standard millis() pattern. It answers "has my interval passed yet?"
            if (now_us - led->last_toggle_us >= led->interval_us) {
                // Flip the level we remember. We don't read it back with
                // gpio_get_level(): in GPIO_MODE_OUTPUT the pin's input buffer is
                // off, so it would always read 0.
                led->level = !led->level;
                ESP_ERROR_CHECK(gpio_set_level(led->gpio, led->level));

                // Move the clock forward by exactly one interval, not to now_us.
                // If this pass ran a few ms late, the next toggle is still due on
                // the original schedule, so the small delays never add up (no drift).
                // After a long stall (e.g. 1 s) the missed toggles are caught up
                // one per pass, so the LED flickers briefly, then is back on time.
                led->last_toggle_us += led->interval_us;

                // now_us / 1000 = milliseconds since boot. Check the gaps in the log.
                ESP_LOGI(TAG, "LED%d -> %s at %lld ms", (int)(i + 1), led->level ? "ON " : "OFF",
                         (long long)(now_us / 1000));
            }
        }

        // NOT used for timing: a yield, not a delay. It sleeps until the next
        // FreeRTOS tick, so the idle task can run and the task watchdog (which
        // watches the idle task) doesn't fire. With the tick at 100 Hz that is
        // 0..10 ms, so a toggle can be up to ~10 ms late, but it never drifts.
        vTaskDelay(1);
    }
}
