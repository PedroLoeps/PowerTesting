#pragma once

#include "esp_wifi_types.h"
#include "esp_blufi_api.h"

#define BLUFI_TAG "BLUFI"
#define BLUFI_INFO(fmt, ...)   ESP_LOGI(BLUFI_TAG, fmt, ##__VA_ARGS__)
#define BLUFI_ERROR(fmt, ...)  ESP_LOGE(BLUFI_TAG, fmt, ##__VA_ARGS__)

struct wifi_info 
{
    bool sta_connected;
    bool sta_got_ip;
    bool ble_is_connected;
    uint8_t sta_bssid[6];
    uint8_t sta_ssid[32];
    int sta_ssid_len;
    wifi_sta_list_t sta_list;
    bool sta_is_connecting;
    esp_blufi_extra_info_t sta_conn_info;
};

void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);

int blufi_security_init(void);
void blufi_security_deinit(void);
esp_err_t esp_blufi_host_init(void);
esp_err_t esp_blufi_host_and_cb_init();
esp_err_t esp_blufi_host_deinit(void);