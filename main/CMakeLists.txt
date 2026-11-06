# Two solutions, one at a time. Pick it in menuconfig:
# "Lesson 2.3 LEDs" -> "Blink solution" (see Kconfig.projbuild).
if(CONFIG_LEDS_SOLUTION_RTOS_TASKS)
    set(srcs "main_rtos.c")
else()
    set(srcs "main.c")
endif()

idf_component_register(SRCS ${srcs}
                       PRIV_REQUIRES esp_driver_gpio esp_timer)
