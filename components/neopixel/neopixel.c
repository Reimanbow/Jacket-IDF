#include "neopixel.h"
#include "led_strip.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "neopixel";

struct neopixel_t {
    led_strip_handle_t strip;
    uint16_t num_pixels;
};

neopixel_handle_t neopixel_init(gpio_num_t gpio_pin, uint16_t num_pixels)
{
    if (num_pixels == 0) {
        ESP_LOGE(TAG, "Number of pixels must be greater than 0");
        return NULL;
    }

    neopixel_handle_t handle = (neopixel_handle_t)malloc(sizeof(struct neopixel_t));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for neopixel handle");
        return NULL;
    }

    handle->num_pixels = num_pixels;

    // Configure LED strip
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_pin,
        .max_leds = num_pixels,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    // Configure RMT backend
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &handle->strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        free(handle);
        return NULL;
    }

    // Clear all LEDs initially
    neopixel_clear(handle);

    ESP_LOGI(TAG, "NeoPixel initialized: GPIO=%d, Pixels=%d", (int)gpio_pin, num_pixels);
    return handle;
}

esp_err_t neopixel_deinit(neopixel_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = led_strip_del(handle->strip);
    free(handle);
    return ret;
}

esp_err_t neopixel_set_pixel(neopixel_handle_t handle, uint16_t index, neopixel_color_t color, uint8_t brightness)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (index >= handle->num_pixels) {
        ESP_LOGE(TAG, "Pixel index %d out of range (max: %d)", index, handle->num_pixels - 1);
        return ESP_ERR_INVALID_ARG;
    }

    // Clamp brightness to max 127 (half of 255)
    if (brightness > 127) {
        brightness = 127;
    }

    // Set color based on selection (only one color at a time)
    uint8_t r = 0, g = 0, b = 0;
    switch (color) {
        case NEOPIXEL_COLOR_RED:
            r = brightness;
            break;
        case NEOPIXEL_COLOR_GREEN:
            g = brightness;
            break;
        case NEOPIXEL_COLOR_BLUE:
            b = brightness;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    return led_strip_set_pixel(handle->strip, index, r, g, b);
}

esp_err_t neopixel_set_all(neopixel_handle_t handle, neopixel_color_t color, uint8_t brightness)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clamp brightness to max 127 (half of 255)
    if (brightness > 127) {
        brightness = 127;
    }

    // Set color based on selection (only one color at a time)
    uint8_t r = 0, g = 0, b = 0;
    switch (color) {
        case NEOPIXEL_COLOR_RED:
            r = brightness;
            break;
        case NEOPIXEL_COLOR_GREEN:
            g = brightness;
            break;
        case NEOPIXEL_COLOR_BLUE:
            b = brightness;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    for (uint16_t i = 0; i < handle->num_pixels; i++) {
        esp_err_t ret = led_strip_set_pixel(handle->strip, i, r, g, b);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t neopixel_clear(neopixel_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = led_strip_clear(handle->strip);
    if (ret == ESP_OK) {
        ret = neopixel_show(handle);
    }
    return ret;
}

esp_err_t neopixel_show(neopixel_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return led_strip_refresh(handle->strip);
}
