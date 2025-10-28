#include "espnow_handler.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

// MACアドレス表示用マクロ（ESP-IDF 5.4互換）
#ifndef MACSTR
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#endif

static const char *TAG = "ESPNOW";

// ブロードキャストMACアドレス
static const uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// WSPコンテキストへのポインタ（グローバル）
static wsp_context_t *s_wsp_ctx = NULL;

/**
 * @brief ESP-NOW受信コールバック
 */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (s_wsp_ctx == NULL) {
        ESP_LOGW(TAG, "WSP context not initialized");
        return;
    }

    // 送信元MACアドレスをログ
    ESP_LOGI(TAG, "Received %d bytes from " MACSTR, len,
             MAC2STR(recv_info->src_addr));

    // WSPパケット検証
    wsp_valid_result_t valid = wsp_validate_packet(s_wsp_ctx, data, len);
    if (valid != WSP_VALID_OK) {
        ESP_LOGW(TAG, "Invalid WSP packet: %d", valid);
        return;
    }

    // パケットをパース
    wsp_packet_header_t header;
    const uint8_t *payload;
    uint8_t payload_len;

    if (wsp_parse_packet(data, len, &header, &payload, &payload_len) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to parse WSP packet");
        return;
    }

    // パケットタイプ別処理
    ESP_LOGI(TAG, "Valid WSP packet: Type=0x%02X, Src=0x%02X, Dst=0x%02X, Len=%d",
             header.type, header.src_id, header.dst_id, header.length);

    // TODO: パケットタイプ別のハンドラ呼び出し
    // 例: handle_led_control(payload, payload_len), handle_alert(payload, payload_len), など
}

/**
 * @brief ESP-NOW送信コールバック
 */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "Send success to " MACSTR, MAC2STR(mac_addr));
    } else {
        ESP_LOGW(TAG, "Send failed to " MACSTR, MAC2STR(mac_addr));
    }
}

/**
 * @brief WiFi初期化
 */
static esp_err_t wifi_init(void)
{
    // NetIF初期化
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // WiFiチャンネルを設定（全デバイスで統一する必要がある）
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_LOGI(TAG, "WiFi initialized (STA mode, Channel 1)");
    return ESP_OK;
}

esp_err_t espnow_handler_init(wsp_context_t *ctx)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_wsp_ctx = ctx;

    // WiFi初期化
    esp_err_t ret = wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // ESP-NOW初期化
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // コールバック登録
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    // ブロードキャストピア追加
    esp_now_peer_info_t broadcast_peer = {
        .channel = 1,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(broadcast_peer.peer_addr, s_broadcast_mac, 6);

    ret = esp_now_add_peer(&broadcast_peer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add broadcast peer: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "ESP-NOW initialized successfully");
    ESP_LOGI(TAG, "Device: 0x%02X, UUID: %02X:%02X:%02X:%02X:%02X:%02X",
             ctx->device_id,
             ctx->user_uuid[0], ctx->user_uuid[1], ctx->user_uuid[2],
             ctx->user_uuid[3], ctx->user_uuid[4], ctx->user_uuid[5]);

    return ESP_OK;
}

esp_err_t espnow_handler_deinit(void)
{
    esp_now_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    s_wsp_ctx = NULL;
    ESP_LOGI(TAG, "ESP-NOW deinitialized");
    return ESP_OK;
}

esp_err_t espnow_send_unicast(const uint8_t *dst_mac, const uint8_t *packet, size_t len)
{
    if (dst_mac == NULL || packet == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_now_send(dst_mac, packet, len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Unicast send failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t espnow_send_broadcast(const uint8_t *packet, size_t len)
{
    if (packet == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_now_send(s_broadcast_mac, packet, len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Broadcast send failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t espnow_add_peer(const uint8_t *peer_mac)
{
    if (peer_mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 既に存在するかチェック
    if (esp_now_is_peer_exist(peer_mac)) {
        ESP_LOGD(TAG, "Peer already exists: " MACSTR, MAC2STR(peer_mac));
        return ESP_OK;
    }

    esp_now_peer_info_t peer_info = {
        .channel = 1,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer_info.peer_addr, peer_mac, 6);

    esp_err_t ret = esp_now_add_peer(&peer_info);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add peer " MACSTR ": %s",
                 MAC2STR(peer_mac), esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Peer added: " MACSTR, MAC2STR(peer_mac));
    }

    return ret;
}

esp_err_t espnow_remove_peer(const uint8_t *peer_mac)
{
    if (peer_mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_now_del_peer(peer_mac);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remove peer " MACSTR ": %s",
                 MAC2STR(peer_mac), esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Peer removed: " MACSTR, MAC2STR(peer_mac));
    }

    return ret;
}
