#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "esp_err.h"

/**
 * @brief 統合センサーマネージャータスク
 *
 * BME680、COセンサー、H2Sセンサーを管理し、定期的にデータを読み取る
 *
 * @param pvParameters タスクパラメータ(未使用)
 */
void sensor_manager_task(void *pvParameters);

/**
 * @brief センサーマネージャータスクを開始
 *
 * @return esp_err_t タスク作成結果
 */
esp_err_t sensor_task_start(void);

#endif // SENSOR_TASK_H
