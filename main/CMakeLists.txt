idf_component_register(SRCS "Jacket-IDF.c"
                           "sensors/bme680_sensor.c"
                           "sensors/gas_sensor.c"
                           "tasks/sensor_task.c"
                           "espnow/espnow_handler.c"
                    INCLUDE_DIRS "."
                    REQUIRES neopixel sd_storage esp-idf-lib__bme680 esp-idf-lib__i2cdev esp_adc wsp esp_wifi nvs_flash)
