

#include <stdio.h>
#include "esp_err.h"
#include "esp_blufi_api.h"
#include "esp_log.h"
#include "esp_blufi.h"
#include "esp_bt.h"
#include "esp_wifi.h"
#include "esp_blufi_api.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "console/console.h"

#include "blufi_util.h"
#include "wifi_util.h"
#include "nvs_util.h"

#define WIFI_LIST_NUM   10 //Is this used anywhere?

struct wifi_info wifi_inf;

extern wifi_config_t wifi_config;

bool config_done;

static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

static esp_blufi_callbacks_t blufi_callbacks = {
    .event_cb = blufi_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};

static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        BLUFI_INFO("BLUFI init finish\n");
        esp_blufi_adv_start();
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        BLUFI_INFO("BLUFI deinit finish\n");
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        BLUFI_INFO("BLUFI ble connect\n");
        wifi_inf.ble_is_connected = true;
        esp_blufi_adv_stop();
        blufi_security_init();
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        BLUFI_INFO("BLUFI ble disconnect\n");
        wifi_inf.ble_is_connected = false;
        blufi_security_deinit();
        esp_blufi_adv_start();
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        BLUFI_INFO("BLUFI Set WIFI opmode %d\n", param->wifi_mode.op_mode);
        ESP_ERROR_CHECK( esp_wifi_set_mode(param->wifi_mode.op_mode) );
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        BLUFI_INFO("BLUFI request wifi connect to AP\n");
        /* there is no wifi callback when the device has already connected to this wifi
        so disconnect wifi before connection.
        */
        esp_wifi_disconnect();
        wifi_connect();
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        BLUFI_INFO("BLUFI requset wifi disconnect from AP\n");
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        BLUFI_ERROR("BLUFI report error, error code %d\n", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;

        esp_wifi_get_mode(&mode);

        if (wifi_inf.sta_connected) {
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
        BLUFI_INFO("BLUFI get wifi status from AP\n");

        break;
    }
    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        BLUFI_INFO("blufi close a gatt connection");
        esp_blufi_disconnect();
        break;
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        /* TODO */
        break;
	case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(wifi_config.sta.bssid, param->sta_bssid.bssid, 6);
        wifi_config.sta.bssid_set = 1;
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        BLUFI_INFO("Recv STA BSSID %s\n", wifi_config.sta.ssid);
        break;
	case ESP_BLUFI_EVENT_RECV_STA_SSID:
        strncpy((char *)wifi_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        wifi_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        BLUFI_INFO("Recv STA SSID %s\n", wifi_config.sta.ssid);
        break;
	case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        strncpy((char *)wifi_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        wifi_config.sta.password[param->sta_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        BLUFI_INFO("Recv STA PASSWORD %s\n", wifi_config.sta.password);
        config_done = true;//find better way to do this
        set_saved_wifi(&wifi_config);//find better place to do this
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_LIST:{
        
        esp_err_t err = wifi_scan();
        if (err != ESP_OK) {
            esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        }
        break;
    }
    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
        BLUFI_INFO("Recv Custom Data %" PRIu32 "\n", param->custom_data.data_len);
        esp_log_buffer_hex("Custom Data", param->custom_data.data, param->custom_data.data_len);
        break;
	case ESP_BLUFI_EVENT_RECV_USERNAME:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_CA_CERT:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
        /* Not handle currently */
        break;;
	case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
        /* Not handle currently */
        break;
    default:
        break;
    }
}

void ble_store_config_init(void); // Need to be here?

static void blufi_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void blufi_on_sync(void)
{
  esp_blufi_profile_init();
}

void bleprph_host_task(void *param)
{
    BLUFI_INFO("BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

esp_err_t esp_blufi_host_init(void)
{
    esp_err_t err;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) {
        BLUFI_ERROR("%s initialize bt controller failed: %s\n", __func__, esp_err_to_name(err));
        return ESP_FAIL; //Should we create a different fail macro? --
    }

    err = esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK;
    if (err != ESP_OK) {
        BLUFI_ERROR("%s enable bt controller failed: %s\n", __func__, esp_err_to_name(err));
        return ESP_FAIL; //--
    }
    
    err = esp_nimble_init();
    if (err != ESP_OK) {
        BLUFI_ERROR("%s failed: %s\n", __func__, esp_err_to_name(err));
        return ESP_FAIL;
    }

/* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = blufi_on_reset;
    ble_hs_cfg.sync_cb = blufi_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = 4;
    /*
#ifdef CONFIG_BONDING
    ble_hs_cfg.sm_bonding = 1;
#endif
#ifdef CONFIG_MITM
    ble_hs_cfg.sm_mitm = 1;
#endif
#ifdef CONFIG_USE_SC
    ble_hs_cfg.sm_sc = 1;
#else
    ble_hs_cfg.sm_sc = 0; Need to define?
#ifdef CONFIG_BONDING
    ble_hs_cfg.sm_our_key_dist = 1;
    ble_hs_cfg.sm_their_key_dist = 1;
#endif
#endif */

    int rc;
    rc = esp_blufi_gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set(BLUFI_DEVICE_NAME);
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    esp_blufi_btc_init();

    err = esp_nimble_enable(bleprph_host_task);
    if (err) {
        BLUFI_ERROR("%s failed: %s\n", __func__, esp_err_to_name(err));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_blufi_host_deinit(void)
{
    esp_err_t err = ESP_OK;

    err = esp_blufi_profile_deinit();
    if(err != ESP_OK) {
        return err;
    }

    esp_blufi_btc_deinit();

    err = nimble_port_stop();
    if (err == 0) {
        nimble_port_deinit();
    }

    return err;
}

static esp_err_t esp_blufi_gap_register_callback(void)
{
    return ESP_OK;
}

esp_err_t esp_blufi_host_and_cb_init(void)
{

    config_done = false;
    esp_err_t err = ESP_OK;

    err = esp_blufi_register_callbacks(&blufi_callbacks);
    if(err){
        BLUFI_ERROR("%s blufi register failed, error code = %x\n", __func__, err);
        return err;
    }


    //Find more about GAP API(Might not be needed)
    err = esp_blufi_gap_register_callback();
    if(err){
        BLUFI_ERROR("%s gap register failed, error code = %x\n", __func__, err);
        return err;
    }

    err = esp_blufi_host_init();
    if (err) {
        BLUFI_ERROR("%s initialise host failed: %s\n", __func__, esp_err_to_name(err));
        return err;
    }

    return err;
}