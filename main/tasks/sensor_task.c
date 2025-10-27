#include "sensor_task.h"
#include "sensors/bme680_sensor.h"
#include "sensors/gas_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2cdev.h"

static const char *TAG = "sensor_task";

void sensor_manager_task(void *pvParameters)
{
    // ========== BME680初期化 ==========
    bme680_sensor_handle_t bme_handle;
    esp_err_t res = bme680_sensor_init(&bme_handle);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "BME680 init failed, will continue without it");
    }

    // ========== ガスセンサー初期化 ==========
    gas_sensor_handle_t gas_handle;
    res = gas_sensor_init(&gas_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Gas sensor init failed: %s", esp_err_to_name(res));
        vTaskDelete(NULL);
        return;
    }

    // ========== メインループ ==========
    while (1) {
        // ウォームアップ状態を更新
        gas_sensor_update_warmup(&gas_handle);

        // ========== BME680読み取り ==========
        if (bme680_sensor_is_available(&bme_handle)) {
            bme680_values_float_t bme_values;
            if (bme680_sensor_read(&bme_handle, &bme_values) == ESP_OK) {
                ESP_LOGI(TAG, "BME680: Temp=%.2f°C, Hum=%.2f%%, Press=%.2fhPa, Gas=%.2fΩ",
                    bme_values.temperature, bme_values.humidity,
                    bme_values.pressure, bme_values.gas_resistance);
            }
        }

        // ========== COセンサー読み取り ==========
        if (gas_sensor_is_ready(&gas_handle, GAS_SENSOR_CO)) {
            int co_adc = 0;
            if (gas_sensor_read(&gas_handle, GAS_SENSOR_CO, &co_adc) == ESP_OK) {
                ESP_LOGI(TAG, "CO Sensor: ADC=%d", co_adc);
            }
        }

        // ========== H2Sセンサー読み取り ==========
        if (gas_sensor_is_ready(&gas_handle, GAS_SENSOR_H2S)) {
            int h2s_adc = 0;
            if (gas_sensor_read(&gas_handle, GAS_SENSOR_H2S, &h2s_adc) == ESP_OK) {
                ESP_LOGI(TAG, "H2S Sensor: ADC=%d", h2s_adc);
            }
        }

        // 1秒待機
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t sensor_task_start(void)
{
    BaseType_t result = xTaskCreate(
        sensor_manager_task,
        "sensor_manager",
        configMINIMAL_STACK_SIZE * 8,
        NULL,
        5,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor manager task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sensor manager task started");
    return ESP_OK;
}
