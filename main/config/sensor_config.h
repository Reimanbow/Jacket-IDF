#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

// ========== CO Sensor settings ==========
#define CO_SENSOR_ADC_CHANNEL ADC_CHANNEL_1  // GPIO1 = ADC1_CH1
#define CO_SENSOR_WARMUP_TIME_MS (5 * 60 * 1000)  // 5 minutes

// ========== H2S Sensor settings ==========
#define H2S_SENSOR_ADC_CHANNEL ADC_CHANNEL_2  // GPIO2 = ADC1_CH2
#define H2S_SENSOR_WARMUP_TIME_MS (5 * 60 * 1000)  // 5 minutes

// ========== BME680 I2C settings ==========
#define BME680_I2C_PORT I2C_NUM_0
#define BME680_I2C_ADDR BME680_I2C_ADDR_1  // 0x77 (or use BME680_I2C_ADDR_0 for 0x76)
#define BME680_I2C_SDA GPIO_NUM_22
#define BME680_I2C_SCL GPIO_NUM_23
#define BME680_I2C_FREQ_HZ 100000  // 100kHz for stability

#endif // SENSOR_CONFIG_H
