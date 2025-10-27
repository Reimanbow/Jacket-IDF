#ifndef GAS_SENSOR_H
#define GAS_SENSOR_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

/**
 * @brief ガスセンサーの種類
 */
typedef enum {
    GAS_SENSOR_CO,   // 一酸化炭素センサー
    GAS_SENSOR_H2S,  // 硫化水素センサー
    GAS_SENSOR_MAX
} gas_sensor_type_t;

/**
 * @brief ガスセンサーマネージャーのハンドル
 */
typedef struct {
    adc_oneshot_unit_handle_t adc_handle;
    bool co_ready;
    bool h2s_ready;
    uint32_t start_time_ms;
} gas_sensor_handle_t;

/**
 * @brief ガスセンサーマネージャーを初期化
 *
 * @param handle センサーハンドルへのポインタ
 * @return esp_err_t 初期化結果
 */
esp_err_t gas_sensor_init(gas_sensor_handle_t *handle);

/**
 * @brief ウォームアップ状態を更新
 *
 * @param handle センサーハンドル
 */
void gas_sensor_update_warmup(gas_sensor_handle_t *handle);

/**
 * @brief ガスセンサーからADC値を読み取る
 *
 * @param handle センサーハンドル
 * @param type センサーの種類
 * @param adc_value 読み取ったADC値を格納する変数
 * @return esp_err_t 読み取り結果
 */
esp_err_t gas_sensor_read(gas_sensor_handle_t *handle, gas_sensor_type_t type, int *adc_value);

/**
 * @brief センサーが準備完了しているかチェック
 *
 * @param handle センサーハンドル
 * @param type センサーの種類
 * @return true 準備完了
 * @return false ウォームアップ中
 */
bool gas_sensor_is_ready(const gas_sensor_handle_t *handle, gas_sensor_type_t type);

#endif // GAS_SENSOR_H
