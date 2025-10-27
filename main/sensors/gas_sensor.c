#include "gas_sensor.h"
#include "config/sensor_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "gas_sensor";

esp_err_t gas_sensor_init(gas_sensor_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(handle, 0, sizeof(gas_sensor_handle_t));

    // ADC初期化
    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t res = adc_oneshot_new_unit(&adc_init_config, &handle->adc_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed: %s", esp_err_to_name(res));
        return res;
    }

    // COセンサー用ADCチャンネル設定
    adc_oneshot_chan_cfg_t adc_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,  // 0-3.3V range
    };

    res = adc_oneshot_config_channel(handle->adc_handle, CO_SENSOR_ADC_CHANNEL, &adc_config);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "CO sensor ADC config failed: %s", esp_err_to_name(res));
        return res;
    }

    res = adc_oneshot_config_channel(handle->adc_handle, H2S_SENSOR_ADC_CHANNEL, &adc_config);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "H2S sensor ADC config failed: %s", esp_err_to_name(res));
        return res;
    }

    ESP_LOGI(TAG, "ADC initialized for CO (CH%d) and H2S (CH%d) sensors",
             CO_SENSOR_ADC_CHANNEL, H2S_SENSOR_ADC_CHANNEL);

    // ウォームアップ開始時刻を記録
    handle->start_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    handle->co_ready = false;
    handle->h2s_ready = false;

    ESP_LOGI(TAG, "CO and H2S sensors warming up for 5 minutes...");

    return ESP_OK;
}

void gas_sensor_update_warmup(gas_sensor_handle_t *handle)
{
    if (handle == NULL) {
        return;
    }

    uint32_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed_ms = current_time_ms - handle->start_time_ms;

    // COセンサーのウォームアップチェック
    if (!handle->co_ready && elapsed_ms >= CO_SENSOR_WARMUP_TIME_MS) {
        handle->co_ready = true;
        ESP_LOGI(TAG, "CO Sensor ready");
    }

    // H2Sセンサーのウォームアップチェック
    if (!handle->h2s_ready && elapsed_ms >= H2S_SENSOR_WARMUP_TIME_MS) {
        handle->h2s_ready = true;
        ESP_LOGI(TAG, "H2S Sensor ready");
    }
}

esp_err_t gas_sensor_read(gas_sensor_handle_t *handle, gas_sensor_type_t type, int *adc_value)
{
    if (handle == NULL || adc_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    adc_channel_t channel;
    bool ready;

    switch (type) {
        case GAS_SENSOR_CO:
            channel = CO_SENSOR_ADC_CHANNEL;
            ready = handle->co_ready;
            break;

        case GAS_SENSOR_H2S:
            channel = H2S_SENSOR_ADC_CHANNEL;
            ready = handle->h2s_ready;
            break;

        default:
            return ESP_ERR_INVALID_ARG;
    }

    if (!ready) {
        return ESP_ERR_INVALID_STATE;
    }

    return adc_oneshot_read(handle->adc_handle, channel, adc_value);
}

bool gas_sensor_is_ready(const gas_sensor_handle_t *handle, gas_sensor_type_t type)
{
    if (handle == NULL) {
        return false;
    }

    switch (type) {
        case GAS_SENSOR_CO:
            return handle->co_ready;

        case GAS_SENSOR_H2S:
            return handle->h2s_ready;

        default:
            return false;
    }
}
