#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "i2cdev.h"
#include "nvs_flash.h"

#include "neopixel.h"
#include "sd_storage.h"
#include "tasks/sensor_task.h"
#include "wsp_protocol.h"
#include "espnow/espnow_handler.h"

#define NEOPIXEL_PIN GPIO_NUM_0
#define NEOPIXEL_COUNT 12

static const char *TAG = "main";

// WSPコンテキスト（グローバル）
static wsp_context_t g_wsp_ctx;

void app_main(void)
{
	// Initialize NVS (必須: WiFi/ESP-NOWで使用)
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// Initialize WSP Protocol
	// デバイスタイプとインスタンス番号を設定
	// TODO: NVSから読み込むか、menuconfigで設定可能にする
	ESP_ERROR_CHECK(wsp_init(&g_wsp_ctx, WSP_DEVICE_TYPE_JACKET, 0));
	ESP_LOGI(TAG, "WSP initialized: Device ID=0x%02X", g_wsp_ctx.device_id);

	// Initialize ESP-NOW
	ESP_ERROR_CHECK(espnow_handler_init(&g_wsp_ctx));

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

	// Start sensor manager task
	sensor_task_start();

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
