#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "neopixel.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define NEOPIXEL_PIN GPIO_NUM_0
#define NEOPIXEL_COUNT 12

static const char *TAG = "main";

void app_main(void)
{
	// Initialize NeoPixel strip
	neopixel_handle_t strip = neopixel_init(NEOPIXEL_PIN, NEOPIXEL_COUNT);
	if (strip == NULL) {
		ESP_LOGE(TAG, "Failed to initialize NeoPixel");
		return;
	}

	ESP_LOGI(TAG, "NeoPixel initialized on GPIO %d with %d pixels", NEOPIXEL_PIN, NEOPIXEL_COUNT);

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
