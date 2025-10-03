#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct neopixel_t *neopixel_handle_t;

/**
 * @brief NeoPixel color options (single color only)
 */
typedef enum {
    NEOPIXEL_COLOR_RED = 0,   ///< Red color
    NEOPIXEL_COLOR_GREEN = 1, ///< Green color
    NEOPIXEL_COLOR_BLUE = 2   ///< Blue color
} neopixel_color_t;

/**
 * @brief Initialize NeoPixel LED strip
 *
 * @param gpio_pin GPIO pin number for data output (use GPIO_NUM_x)
 * @param num_pixels Number of LEDs in the strip
 * @return neopixel_handle_t Handle to the NeoPixel strip, or NULL on failure
 */
neopixel_handle_t neopixel_init(gpio_num_t gpio_pin, uint16_t num_pixels);

/**
 * @brief Deinitialize NeoPixel LED strip
 *
 * @param handle NeoPixel handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t neopixel_deinit(neopixel_handle_t handle);

/**
 * @brief Set a single pixel color (red, green, or blue only, brightness 0-127)
 *
 * @param handle NeoPixel handle
 * @param index Pixel index (0-based)
 * @param color Color selection (NEOPIXEL_COLOR_RED, GREEN, or BLUE)
 * @param brightness Brightness value (0-127, will be clamped)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t neopixel_set_pixel(neopixel_handle_t handle, uint16_t index, neopixel_color_t color, uint8_t brightness);

/**
 * @brief Set all pixels to the same color and brightness (red, green, or blue only, brightness 0-127)
 *
 * @param handle NeoPixel handle
 * @param color Color selection (NEOPIXEL_COLOR_RED, GREEN, or BLUE)
 * @param brightness Brightness value (0-127, will be clamped)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t neopixel_set_all(neopixel_handle_t handle, neopixel_color_t color, uint8_t brightness);

/**
 * @brief Clear all pixels (turn off)
 *
 * @param handle NeoPixel handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t neopixel_clear(neopixel_handle_t handle);

/**
 * @brief Update the LED strip with buffered pixel data
 *
 * @param handle NeoPixel handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t neopixel_show(neopixel_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // NEOPIXEL_H
