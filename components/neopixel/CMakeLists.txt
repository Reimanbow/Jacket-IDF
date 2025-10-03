idf_component_register(SRCS "neopixel.c"
                    INCLUDE_DIRS "."
                    REQUIRES led_strip esp_driver_gpio)
