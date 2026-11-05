# Two solutions, one at a time. Pick it in menuconfig:
# "Lesson 2.3 LEDs" -> "Blink solution" (see Kconfig.projbuild).
if(CONFIG_LEDS_SOLUTION_RTOS_TASKS)
    set(srcs "main_rtos.c")
else()
    set(srcs "main.c")
endif()
set(priv_requires esp_driver_gpio esp_timer)
set(extra_args "")

# SystemView trace build only (sdkconfig.trace_uart): tell esp_trace to use the
# "sysview" encoder from espressif/esp_sysview. See trace_params.c.
if(CONFIG_ESP_TRACE_LIB_EXTERNAL)
    list(APPEND srcs "trace_params.c")
    list(APPEND priv_requires esp_trace)
    # esp_trace already has a weak esp_trace_get_user_params(), so the linker
    # would never pull trace_params.c.obj out of libmain.a. WHOLE_ARCHIVE links
    # every object of this component, and our strong definition wins.
    list(APPEND extra_args WHOLE_ARCHIVE)
endif()

idf_component_register(SRCS ${srcs}
                       PRIV_REQUIRES ${priv_requires}
                       ${extra_args})
