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

// Unified sensor manager task
void sensor_manager_task(void *pvParameters)
{
	// ========== ADC初期化 ==========
	adc_oneshot_unit_handle_t adc1_handle;
	adc_oneshot_unit_init_cfg_t adc_init_config = {
		.unit_id = ADC_UNIT_1,
	};
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_config, &adc1_handle));

	// COセンサー用ADCチャンネル設定
	adc_oneshot_chan_cfg_t adc_config = {
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.atten = ADC_ATTEN_DB_12,  // 0-3.3V range
	};
	ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, CO_SENSOR_ADC_CHANNEL, &adc_config));
	ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, H2S_SENSOR_ADC_CHANNEL, &adc_config));
	ESP_LOGI(TAG, "ADC1 initialized for CO (GPIO1) and H2S (GPIO2) sensors");

	// ========== BME680初期化 ==========
	bme680_t bme_sensor;
	memset(&bme_sensor, 0, sizeof(bme680_t));

	ESP_LOGI(TAG, "Initializing BME680 sensor on I2C port %d, addr 0x%02X, SDA=%d, SCL=%d",
		BME680_I2C_PORT, BME680_I2C_ADDR, BME680_I2C_SDA, BME680_I2C_SCL);

	esp_err_t res = bme680_init_desc(&bme_sensor, BME680_I2C_ADDR, BME680_I2C_PORT, BME680_I2C_SDA, BME680_I2C_SCL);
	if (res == ESP_OK) {
		bme_sensor.i2c_dev.cfg.master.clk_speed = 100000;  // 100kHz for stability
		bme_sensor.i2c_dev.cfg.sda_pullup_en = true;
		bme_sensor.i2c_dev.cfg.scl_pullup_en = true;
		ESP_LOGI(TAG, "I2C configured: 100kHz, pullups enabled");
	}

	bool bme680_available = false;
	if (res == ESP_OK) {
		res = bme680_init_sensor(&bme_sensor);
		if (res == ESP_OK) {
			bme680_set_oversampling_rates(&bme_sensor, BME680_OSR_1X, BME680_OSR_1X, BME680_OSR_1X);
			bme680_set_filter_size(&bme_sensor, BME680_IIR_SIZE_1);
			bme680_set_heater_profile(&bme_sensor, 0, 300, 100);
			bme680_use_heater_profile(&bme_sensor, 0);
			bme680_set_ambient_temperature(&bme_sensor, 20);
			bme680_available = true;
			ESP_LOGI(TAG, "BME680 initialized successfully");
		} else {
			ESP_LOGW(TAG, "BME680 init failed, will continue without it");
		}
	} else {
		ESP_LOGW(TAG, "BME680 descriptor init failed, will continue without it");
	}

	uint32_t bme_duration = 0;
	if (bme680_available) {
		bme680_get_measurement_duration(&bme_sensor, &bme_duration);
	}

	// ========== ウォームアップ管理 ==========
	bool co_ready = false;
	bool h2s_ready = false;
	uint32_t start_time = xTaskGetTickCount();

	ESP_LOGI(TAG, "CO and H2S sensors warming up for 5 minutes...");

	// ========== メインループ ==========
	while (1) {
		uint32_t elapsed_ms = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;

		// ウォームアップ状態チェック
		if (!co_ready && elapsed_ms >= CO_SENSOR_WARMUP_TIME_MS) {
			co_ready = true;
			ESP_LOGI(TAG, "CO Sensor ready");
		}
		if (!h2s_ready && elapsed_ms >= H2S_SENSOR_WARMUP_TIME_MS) {
			h2s_ready = true;
			ESP_LOGI(TAG, "H2S Sensor ready");
		}

		// ========== BME680読み取り ==========
		bme680_values_float_t bme_values = {0};
		bool bme_valid = false;

		if (bme680_available) {
			if (bme680_force_measurement(&bme_sensor) == ESP_OK) {
				vTaskDelay(pdMS_TO_TICKS(bme_duration + 50));

				bool busy = true;
				for (int retry = 0; retry < 30; retry++) {
					if (bme680_is_measuring(&bme_sensor, &busy) == ESP_OK && !busy) {
						if (bme680_get_results_float(&bme_sensor, &bme_values) == ESP_OK) {
							bme_valid = true;
						}
						break;
					}
					vTaskDelay(pdMS_TO_TICKS(10));
				}
			}
		}

		// ========== ADCセンサー読み取り ==========
		int co_adc = 0, h2s_adc = 0;

		if (co_ready) {
			adc_oneshot_read(adc1_handle, CO_SENSOR_ADC_CHANNEL, &co_adc);
		}
		if (h2s_ready) {
			adc_oneshot_read(adc1_handle, H2S_SENSOR_ADC_CHANNEL, &h2s_adc);
		}

		// ========== 統合ログ出力 ==========
		if (bme_valid) {
			ESP_LOGI(TAG, "BME680: Temp=%.2f°C, Hum=%.2f%%, Press=%.2fhPa, Gas=%.2fΩ",
				bme_values.temperature, bme_values.humidity, bme_values.pressure, bme_values.gas_resistance);
		}
		if (co_ready) {
			ESP_LOGI(TAG, "CO Sensor: ADC=%d", co_adc);
		}
		if (h2s_ready) {
			ESP_LOGI(TAG, "H2S Sensor: ADC=%d", h2s_adc);
		}

		// 1秒待機
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

	// Create unified sensor manager task
	xTaskCreate(sensor_manager_task, "sensor_manager", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL);

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
