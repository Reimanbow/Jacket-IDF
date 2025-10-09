idf_component_register(SRCS "Jacket-IDF.c"
                    INCLUDE_DIRS "."
                    REQUIRES neopixel sd_storage esp-idf-lib__bme680 esp-idf-lib__i2cdev)
