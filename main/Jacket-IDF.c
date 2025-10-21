#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "bme680.h"
#include "i2cdev.h"

#include "neopixel.h"
#include "sd_storage.h"
#include "esp_adc/adc_oneshot.h"

#define NEOPIXEL_PIN GPIO_NUM_0
#define NEOPIXEL_COUNT 12

// CO Sensor settings
#define CO_SENSOR_ADC_CHANNEL ADC_CHANNEL_1  // GPIO1 = ADC1_CH1
#define CO_SENSOR_WARMUP_TIME_MS (5 * 60 * 1000)  // 5 minutes

// H2S Sensor settings
#define H2S_SENSOR_ADC_CHANNEL ADC_CHANNEL_2  // GPIO2 = ADC1_CH2
#define H2S_SENSOR_WARMUP_TIME_MS (5 * 60 * 1000)  // 5 minutes

// BME680 I2C settings
#define BME680_I2C_PORT I2C_NUM_0
#define BME680_I2C_ADDR BME680_I2C_ADDR_1  // 0x76 (or use BME680_I2C_ADDR_1 for 0x77)
#define BME680_I2C_SDA GPIO_NUM_22
#define BME680_I2C_SCL GPIO_NUM_23

static const char *TAG = "main";

// BME680 sensor task
void bme680_task(void *pvParameters)
{
	bme680_t sensor;
	memset(&sensor, 0, sizeof(bme680_t));

	ESP_LOGI(TAG, "Initializing BME680 sensor on I2C port %d, addr 0x%02X, SDA=%d, SCL=%d",
		BME680_I2C_PORT, BME680_I2C_ADDR, BME680_I2C_SDA, BME680_I2C_SCL);

	// Initialize sensor descriptor with explicit I2C configuration
	// Use 100kHz for better stability (default is 400kHz)
	esp_err_t res = bme680_init_desc(&sensor, BME680_I2C_ADDR, BME680_I2C_PORT, BME680_I2C_SDA, BME680_I2C_SCL);
	if (res == ESP_OK) {
		sensor.i2c_dev.cfg.master.clk_speed = 100000;  // 100kHz for stability
		sensor.i2c_dev.cfg.sda_pullup_en = true;       // Enable internal pullup
		sensor.i2c_dev.cfg.scl_pullup_en = true;       // Enable internal pullup
		ESP_LOGI(TAG, "I2C configured: 100kHz, pullups enabled");
	}
	if (res != ESP_OK) {
		ESP_LOGE(TAG, "Failed to init BME680 descriptor: %s (0x%x)", esp_err_to_name(res), res);
		ESP_LOGE(TAG, "Check I2C connections and address. Sensor task will exit.");
		vTaskDelete(NULL);
		return;
	}

	// Initialize the sensor
	res = bme680_init_sensor(&sensor);
	if (res != ESP_OK) {
		ESP_LOGE(TAG, "Failed to init BME680 sensor: %s (0x%x)", esp_err_to_name(res), res);
		ESP_LOGE(TAG, "Possible causes: sensor not connected, wrong I2C address, missing pullups");
		bme680_free_desc(&sensor);
		vTaskDelete(NULL);
		return;
	}

	// Configure oversampling rates: 1x for faster measurements
	bme680_set_oversampling_rates(&sensor, BME680_OSR_1X, BME680_OSR_1X, BME680_OSR_1X);

	// Set IIR filter size to 1 for faster response
	bme680_set_filter_size(&sensor, BME680_IIR_SIZE_1);

	// Configure heater profile 0: 300°C for 100ms (shorter for faster cycle)
	bme680_set_heater_profile(&sensor, 0, 300, 100);
	bme680_use_heater_profile(&sensor, 0);

	// Set ambient temperature to 20°C
	bme680_set_ambient_temperature(&sensor, 20);

	// Get measurement duration (constant as long as configuration doesn't change)
	uint32_t duration;
	bme680_get_measurement_duration(&sensor, &duration);

	ESP_LOGI(TAG, "BME680 initialized successfully (measurement duration: %"PRIu32"ms)", duration);
	ESP_LOGI(TAG, "Note: Gas measurement may take longer due to heater warm-up time");

	bme680_values_float_t values;

	while (1) {
		// Trigger one TPHG measurement cycle
		if (bme680_force_measurement(&sensor) == ESP_OK) {
			// Wait for the measurement duration plus minimal heater margin
			vTaskDelay(pdMS_TO_TICKS(duration + 50));

			// Poll until measurement is ready (with timeout)
			bool busy = true;
			bool measurement_ready = false;
			for (int retry = 0; retry < 30; retry++) {
				if (bme680_is_measuring(&sensor, &busy) == ESP_OK && !busy) {
					measurement_ready = true;
					break;
				}
				vTaskDelay(pdMS_TO_TICKS(10));
			}

			if (measurement_ready) {
				// Get and display results
				if (bme680_get_results_float(&sensor, &values) == ESP_OK) {
					ESP_LOGI(TAG, "BME680: Temp=%.2f°C, Hum=%.2f%%, Press=%.2fhPa, Gas=%.2fΩ",
						values.temperature, values.humidity, values.pressure, values.gas_resistance);
				} else {
					ESP_LOGW(TAG, "Failed to read BME680 values");
				}
			} else {
				ESP_LOGW(TAG, "BME680 measurement timeout (still busy after polling)");
			}
		} else {
			ESP_LOGW(TAG, "Failed to start BME680 measurement");
		}

		// Wait for 1 second between measurements (faster cycle)
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

// CO Sensor task
void co_sensor_task(void *pvParameters)
{
	adc_oneshot_unit_handle_t adc1_handle = (adc_oneshot_unit_handle_t)pvParameters;

	adc_oneshot_chan_cfg_t config = {
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.atten = ADC_ATTEN_DB_12,  // 0-3.3V range
	};
	ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, CO_SENSOR_ADC_CHANNEL, &config));

	ESP_LOGI(TAG, "CO Sensor warming up for 5 minutes...");
	vTaskDelay(pdMS_TO_TICKS(CO_SENSOR_WARMUP_TIME_MS));
	ESP_LOGI(TAG, "CO Sensor ready");

	int adc_reading;
	while (1) {
		ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, CO_SENSOR_ADC_CHANNEL, &adc_reading));
		ESP_LOGI(TAG, "CO Sensor: ADC raw value = %d", adc_reading);

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

// H2S Sensor task
void h2s_sensor_task(void *pvParameters)
{
	adc_oneshot_unit_handle_t adc1_handle = (adc_oneshot_unit_handle_t)pvParameters;

	adc_oneshot_chan_cfg_t config = {
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.atten = ADC_ATTEN_DB_12,  // 0-3.3V range
	};
	ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, H2S_SENSOR_ADC_CHANNEL, &config));

	ESP_LOGI(TAG, "H2S Sensor warming up for 5 minutes...");
	vTaskDelay(pdMS_TO_TICKS(H2S_SENSOR_WARMUP_TIME_MS));
	ESP_LOGI(TAG, "H2S Sensor ready");

	int adc_reading;
	while (1) {
		ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, H2S_SENSOR_ADC_CHANNEL, &adc_reading));
		ESP_LOGI(TAG, "H2S Sensor: ADC raw value = %d", adc_reading);

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void app_main(void)
{
	// Initialize I2C library
	ESP_ERROR_CHECK(i2cdev_init());

	sd_init();

	// Initialize NeoPixel strip
	neopixel_handle_t strip = neopixel_init(NEOPIXEL_PIN, NEOPIXEL_COUNT);
	if (strip == NULL) {
		ESP_LOGE(TAG, "Failed to initialize NeoPixel");
		return;
	}

	ESP_LOGI(TAG, "NeoPixel initialized on GPIO %d with %d pixels", NEOPIXEL_PIN, NEOPIXEL_COUNT);

	// Initialize ADC1 unit (shared by CO and H2S sensors)
	adc_oneshot_unit_handle_t adc1_handle;
	adc_oneshot_unit_init_cfg_t init_config = {
		.unit_id = ADC_UNIT_1,
	};
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));
	ESP_LOGI(TAG, "ADC1 initialized for CO and H2S sensors");

	// Create BME680 task
	xTaskCreate(bme680_task, "bme680_task", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL);

	// Create CO Sensor task
	xTaskCreate(co_sensor_task, "co_sensor_task", configMINIMAL_STACK_SIZE * 4, (void *)adc1_handle, 5, NULL);

	// Create H2S Sensor task
	xTaskCreate(h2s_sensor_task, "h2s_sensor_task", configMINIMAL_STACK_SIZE * 4, (void *)adc1_handle, 5, NULL);

	uint8_t brightness = 0;
	int8_t direction = 1;

	while (1) {
		// Breathing effect with single color (red) and limited brightness
		for (uint16_t i = 0; i < NEOPIXEL_COUNT; i++) {
			neopixel_set_pixel(strip, i, NEOPIXEL_COLOR_RED, brightness);
		}

		neopixel_show(strip);

		// Update brightness for breathing effect
		brightness += direction;
		if (brightness >= 127) {
			direction = -1;
		} else if (brightness == 0) {
			direction = 1;
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
