zephyr_include_directories(include)

if(CONFIG_CAKE_PER_KEY_LED)
  target_sources(app PRIVATE src/per_key_led.c)
endif()

if(CONFIG_CAKE_BEHAVIOR_STATUS_LEDS)
  target_sources(app PRIVATE src/behavior_status_leds.c)
endif()
