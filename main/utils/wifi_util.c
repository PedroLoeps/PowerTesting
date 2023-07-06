#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "esp_blufi_api.h"

#include "wifi_util.h"
#include "blufi_util.h"
#include "nvs_util.h"

static uint8_t wifi_retry = 0;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* store the station info to send back to phone */
extern struct wifi_info wifi_inf;

/* store the wifi configuration*/
wifi_config_t wifi_config;

void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    wifi_mode_t mode;

    switch (event_id) {
    case IP_EVENT_STA_GOT_IP: {
        esp_blufi_extra_info_t info;

        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        esp_wifi_get_mode(&mode);

        memset(&info, 0, sizeof(esp_blufi_extra_info_t));
        memcpy(info.sta_bssid, wifi_inf.sta_bssid, 6);
        info.sta_bssid_set = true;
        info.sta_ssid = wifi_inf.sta_ssid;
        info.sta_ssid_len = wifi_inf.sta_ssid_len;
        wifi_inf.sta_got_ip = true;
        if (wifi_inf.ble_is_connected == true) {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_get_current_connection_number(), &info);
        } else {
            BLUFI_INFO("BLUFI BLE is not connected yet\n");
        }
        break;
    }
    default:
        break;
    }
    return;
}

void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    wifi_event_sta_connected_t *event;
    wifi_event_sta_disconnected_t *disconnected_event;
    //wifi_mode_t mode;

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        wifi_connect();
        break;
    case WIFI_EVENT_STA_CONNECTED:
        wifi_inf.sta_connected = true;
        wifi_inf.sta_is_connecting = false;
        event = (wifi_event_sta_connected_t*) event_data;
        memcpy(wifi_inf.sta_bssid, event->bssid, 6);
        memcpy(wifi_inf.sta_ssid, event->ssid, event->ssid_len);
        wifi_inf.sta_ssid_len = event->ssid_len;
        BLUFI_INFO("Wifi connected");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        /* Only handle reconnection during connecting */
        if (wifi_inf.sta_connected == false && wifi_reconnect() == false) {
            wifi_inf.sta_is_connecting = false;
            disconnected_event = (wifi_event_sta_disconnected_t*) event_data;
            record_wifi_conn_info(disconnected_event->rssi, disconnected_event->reason);
        }
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        wifi_inf.sta_connected = false;
        wifi_inf.sta_got_ip = false;
        memset(wifi_inf.sta_ssid, 0, 32);
        memset(wifi_inf.sta_bssid, 0, 6);
        wifi_inf.sta_ssid_len = 0;
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    /*case WIFI_EVENT_AP_START:
        esp_wifi_get_mode(&mode);*/

        /* TODO: get config or information of softap, then set to report extra_info */
        /*if (wifi_inf.ble_is_connected == true) {
            if (wifi_inf.sta_connected) {
                esp_blufi_extra_info_t info;
                memset(&info, 0, sizeof(esp_blufi_extra_info_t));
                memcpy(info.sta_bssid, wifi_inf.sta_bssid, 6);
                info.sta_bssid_set = true;
                info.sta_ssid = wifi_inf.sta_ssid;
                info.sta_ssid_len = wifi_inf.sta_ssid_len;
                esp_blufi_send_wifi_conn_report(mode, wifi_inf.sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &info);
            } else if (wifi_inf.sta_is_connecting) {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_get_current_connection_number(), &wifi_inf.sta_conn_info);
            } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_get_current_connection_number(), &wifi_inf.sta_conn_info);
            }
        } else {
            BLUFI_INFO("BLUFI BLE is not connected yet\n");
        }
        break;*/
    case WIFI_EVENT_SCAN_DONE: {
        uint16_t apCount = 0;
        esp_wifi_scan_get_ap_num(&apCount);
        if (apCount == 0) {
            BLUFI_INFO("Nothing AP found");
            break;
        }
        wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
        if (!ap_list) {
            BLUFI_ERROR("malloc error, ap_list is NULL");
            break;
        }
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, ap_list));
        esp_blufi_ap_record_t * blufi_ap_list = (esp_blufi_ap_record_t *)malloc(apCount * sizeof(esp_blufi_ap_record_t));
        if (!blufi_ap_list) {
            if (ap_list) {
                free(ap_list);
            }
            BLUFI_ERROR("malloc error, blufi_ap_list is NULL");
            break;
        }
        for (int i = 0; i < apCount; ++i)
        {
            blufi_ap_list[i].rssi = ap_list[i].rssi;
            memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
        }

        if (wifi_inf.ble_is_connected == true) {
            esp_blufi_send_wifi_list(apCount, blufi_ap_list);
        } else {
            BLUFI_INFO("BLUFI BLE is not connected yet\n");
        }

        esp_wifi_scan_stop();
        free(ap_list);
        free(blufi_ap_list);
        break;
    }/*
    case WIFI_EVENT_AP_STACONNECTED: {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        BLUFI_INFO("station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
        break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        BLUFI_INFO("station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
        break;
    }*/

    default:
        break;
    }
    return;
}

void record_wifi_conn_info(int rssi, uint8_t reason)
{
    memset(&wifi_inf.sta_conn_info, 0, sizeof(esp_blufi_extra_info_t));
    if (wifi_inf.sta_is_connecting) {
        wifi_inf.sta_conn_info.sta_max_conn_retry_set = true;
        wifi_inf.sta_conn_info.sta_max_conn_retry = WIFI_CONNECTION_MAXIMUM_RETRY;
    } else {
        wifi_inf.sta_conn_info.sta_conn_rssi_set = true;
        wifi_inf.sta_conn_info.sta_conn_rssi = rssi;
        wifi_inf.sta_conn_info.sta_conn_end_reason_set = true;
        wifi_inf.sta_conn_info.sta_conn_end_reason = reason;
    }
}

void wifi_connect(void)
{
    wifi_retry = 0;
    wifi_inf.sta_is_connecting = (esp_wifi_connect() == ESP_OK);
    record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);
}

bool wifi_reconnect(void)
{
    bool err;
    if (wifi_inf.sta_is_connecting && wifi_retry++ < WIFI_CONNECTION_MAXIMUM_RETRY) {
        BLUFI_INFO("BLUFI WiFi starts reconnection\n");
        wifi_inf.sta_is_connecting = (esp_wifi_connect() == ESP_OK);
        record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);
        err = true;
    } else {
        err = false;
    }
    return err;
}

int softap_get_current_connection_number(void)
{
    //BLUFI_INFO("softap");
    esp_err_t err;
    err = esp_wifi_ap_get_sta_list(&wifi_inf.sta_list);
    if (err == ESP_OK)
    {
        return wifi_inf.sta_list.num;
    }

    return 0;
}

esp_err_t wifi_scan(void)
{
    wifi_scan_config_t scanConf = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = false
        };
    
    return esp_wifi_scan_start(&scanConf, true);
}

void initialise_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);

    if(get_saved_wifi(&wifi_config) == ESP_OK)
    {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    }

    ESP_ERROR_CHECK(esp_wifi_start());
}