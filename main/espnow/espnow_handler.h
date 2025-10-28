#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include "esp_err.h"
#include "wsp_protocol.h"

/**
 * @brief ESP-NOWハンドラを初期化
 *
 * WiFiとESP-NOWを初期化し、受信コールバックを登録する
 *
 * @param ctx WSPコンテキスト
 * @return esp_err_t 初期化結果
 */
esp_err_t espnow_handler_init(wsp_context_t *ctx);

/**
 * @brief ESP-NOWハンドラを停止
 *
 * @return esp_err_t
 */
esp_err_t espnow_handler_deinit(void);

/**
 * @brief ユニキャスト送信
 *
 * @param dst_mac 宛先MACアドレス
 * @param packet 送信するWSPパケット
 * @param len パケット長
 * @return esp_err_t 送信結果
 */
esp_err_t espnow_send_unicast(const uint8_t *dst_mac, const uint8_t *packet, size_t len);

/**
 * @brief ブロードキャスト送信
 *
 * @param packet 送信するWSPパケット
 * @param len パケット長
 * @return esp_err_t 送信結果
 */
esp_err_t espnow_send_broadcast(const uint8_t *packet, size_t len);

/**
 * @brief ピアを追加（ユニキャスト通信用）
 *
 * @param peer_mac ピアのMACアドレス
 * @return esp_err_t
 */
esp_err_t espnow_add_peer(const uint8_t *peer_mac);

/**
 * @brief ピアを削除
 *
 * @param peer_mac ピアのMACアドレス
 * @return esp_err_t
 */
esp_err_t espnow_remove_peer(const uint8_t *peer_mac);

#endif // ESPNOW_HANDLER_H
