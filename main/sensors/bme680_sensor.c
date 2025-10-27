#include "bme680_sensor.h"
#include "config/sensor_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "bme680_sensor";

esp_err_t bme680_sensor_init(bme680_sensor_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(handle, 0, sizeof(bme680_sensor_handle_t));
    handle->available = false;

    ESP_LOGI(TAG, "Initializing BME680 sensor on I2C port %d, addr 0x%02X, SDA=%d, SCL=%d",
        BME680_I2C_PORT, BME680_I2C_ADDR, BME680_I2C_SDA, BME680_I2C_SCL);

    // ディスクリプタ初期化
    esp_err_t res = bme680_init_desc(&handle->device, BME680_I2C_ADDR, BME680_I2C_PORT,
                                      BME680_I2C_SDA, BME680_I2C_SCL);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "BME680 descriptor init failed: %s", esp_err_to_name(res));
        return res;
    }

    // I2C設定
    handle->device.i2c_dev.cfg.master.clk_speed = BME680_I2C_FREQ_HZ;
    handle->device.i2c_dev.cfg.sda_pullup_en = true;
    handle->device.i2c_dev.cfg.scl_pullup_en = true;
    ESP_LOGI(TAG, "I2C configured: %d Hz, pullups enabled", BME680_I2C_FREQ_HZ);

    // センサー初期化
    res = bme680_init_sensor(&handle->device);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "BME680 sensor init failed: %s", esp_err_to_name(res));
        return res;
    }

    // センサー設定
    bme680_set_oversampling_rates(&handle->device, BME680_OSR_1X, BME680_OSR_1X, BME680_OSR_1X);
    bme680_set_filter_size(&handle->device, BME680_IIR_SIZE_1);
    bme680_set_heater_profile(&handle->device, 0, 300, 100);
    bme680_use_heater_profile(&handle->device, 0);
    bme680_set_ambient_temperature(&handle->device, 20);

    // 測定時間を取得
    bme680_get_measurement_duration(&handle->device, &handle->measurement_duration_ms);

    handle->available = true;
    ESP_LOGI(TAG, "BME680 initialized successfully (measurement duration: %lu ms)",
             handle->measurement_duration_ms);

    return ESP_OK;
}

esp_err_t bme680_sensor_read(bme680_sensor_handle_t *handle, bme680_values_float_t *values)
{
    if (handle == NULL || values == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->available) {
        return ESP_ERR_INVALID_STATE;
    }

    // 測定開始
    esp_err_t res = bme680_force_measurement(&handle->device);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "Force measurement failed: %s", esp_err_to_name(res));
        return res;
    }

    // 測定完了まで待機
    vTaskDelay(pdMS_TO_TICKS(handle->measurement_duration_ms + 50));

    // 測定完了確認
    bool busy = true;
    for (int retry = 0; retry < 30; retry++) {
        if (bme680_is_measuring(&handle->device, &busy) == ESP_OK && !busy) {
            // データ読み取り
            res = bme680_get_results_float(&handle->device, values);
            if (res == ESP_OK) {
                return ESP_OK;
            }
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGW(TAG, "Failed to read BME680 data");
    return ESP_FAIL;
}

bool bme680_sensor_is_available(const bme680_sensor_handle_t *handle)
{
    return (handle != NULL) && handle->available;
}
