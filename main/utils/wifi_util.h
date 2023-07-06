#pragma once

#define WIFI_CONNECTION_MAXIMUM_RETRY 2
#define INVALID_REASON                255
#define INVALID_RSSI                  -128


void record_wifi_conn_info(int rssi, uint8_t reason);
void wifi_connect(void);
bool wifi_reconnect(void);
int softap_get_current_connection_number(void);
void initialise_wifi(void);
esp_err_t wifi_scan(void);