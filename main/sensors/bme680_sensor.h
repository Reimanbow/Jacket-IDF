#ifndef BME680_SENSOR_H
#define BME680_SENSOR_H

#include <stdbool.h>
#include "esp_err.h"
#include "bme680.h"

/**
 * @brief BME680センサーのハンドル
 */
typedef struct {
    bme680_t device;
    bool available;
    uint32_t measurement_duration_ms;
} bme680_sensor_handle_t;

/**
 * @brief BME680センサーを初期化
 *
 * @param handle センサーハンドルへのポインタ
 * @return esp_err_t 初期化結果
 */
esp_err_t bme680_sensor_init(bme680_sensor_handle_t *handle);

/**
 * @brief BME680センサーからデータを読み取る
 *
 * @param handle センサーハンドル
 * @param values 読み取った値を格納する構造体
 * @return esp_err_t 読み取り結果
 */
esp_err_t bme680_sensor_read(bme680_sensor_handle_t *handle, bme680_values_float_t *values);

/**
 * @brief BME680センサーが利用可能かチェック
 *
 * @param handle センサーハンドル
 * @return true 利用可能
 * @return false 利用不可
 */
bool bme680_sensor_is_available(const bme680_sensor_handle_t *handle);

#endif // BME680_SENSOR_H
