#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "sensor_util.h"

#include "nvs_util.h"


static const char *TAG = "NVS_UTIL";

esp_err_t nvs_init()
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    return err; //Is this necessary? if err != ESP_OK the program will terminate
}

esp_err_t open_nvs(const char* namespace, nvs_handle_t* my_handle)
{
    // Open
    esp_err_t err = nvs_open(namespace, NVS_READWRITE, my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS!", esp_err_to_name(err));
    }
    return err;
}

esp_err_t set_saved_wifi(wifi_config_t* wifi_config) 
{
    esp_err_t err = nvs_init();
    if (err != ESP_OK) return err;

    nvs_handle_t my_handle;
    err = open_nvs("saved_params", &my_handle);

    if (err != ESP_OK) return err;

    size_t required_size = sizeof(wifi_config_t);

    err = nvs_set_blob(my_handle, "saved_wifi", wifi_config, required_size);

    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return err;
}

esp_err_t get_saved_wifi(wifi_config_t* wifi_config) 
{
    esp_err_t err = nvs_init();

    if (err != ESP_OK) return err;

    nvs_handle_t my_handle;
    err = open_nvs("saved_params", &my_handle);

    if (err != ESP_OK) return err;


    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "saved_wifi", NULL, &required_size);

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;


    if (required_size == 0) {
        ESP_LOGE(TAG, "There is no saved wifi!");
        return err;
    } else {

        err = nvs_get_blob(my_handle, "saved_wifi", wifi_config, &required_size);

        if (err != ESP_OK) {
            return err;
        }

        return err;
    }
}

esp_err_t set_saved_config(struct sensor_config* sensor) 
{
    esp_err_t err = nvs_init();
    if (err != ESP_OK) return err;

    nvs_handle_t my_handle;
    err = open_nvs("saved_params", &my_handle);

    if (err != ESP_OK) return err;
    
    size_t required_size = sizeof(struct sensor_config);

    err = nvs_set_blob(my_handle, "saved_config", sensor, required_size);

    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return err;
}

esp_err_t get_saved_config(struct sensor_config* sensor) 
{
    esp_err_t err = nvs_init();

    if (err != ESP_OK) return err;

    nvs_handle_t my_handle;
    err = open_nvs("saved_params", &my_handle);

    if (err != ESP_OK) return err;


    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "saved_config", NULL, &required_size);

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;


    if (required_size == 0) {
        ESP_LOGE(TAG, "There is no saved configuration!");
        return err;
    } else {

        err = nvs_get_blob(my_handle, "saved_config", sensor, &required_size);

        if (err != ESP_OK) {
            return err;
        }

        return err;
    }
}

esp_err_t set_saved_readings(int* temp, float* ph, int size) 
{
    esp_err_t err = nvs_init();
    if (err != ESP_OK) return err;

    nvs_handle_t my_handle;
    err = open_nvs("saved_params", &my_handle);

    if (err != ESP_OK) return err;
    
    size_t required_size_temp = sizeof(int)*size;
    size_t required_size_ph = sizeof(float)*size;

    err = nvs_set_blob(my_handle, "saved_temp", temp, required_size_temp);

    if (err != ESP_OK) return err;

    err = nvs_set_blob(my_handle, "saved_ph", ph, required_size_ph);

    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return err;
}

esp_err_t get_saved_readings(int* temp, float* ph) 
{
    esp_err_t err = nvs_init();

    if (err != ESP_OK) return err;

    nvs_handle_t my_handle;
    err = open_nvs("saved_params", &my_handle);

    if (err != ESP_OK) return err;


    size_t required_size_temp, required_size_ph = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "saved_temp", NULL, &required_size_temp);

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    err = nvs_get_blob(my_handle, "saved_ph", NULL, &required_size_ph);

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;


    if (required_size_temp == 0 || required_size_ph == 0) {
        ESP_LOGE(TAG, "There is no saved configuration!");
        return err;
    } else {

        err = nvs_get_blob(my_handle, "saved_temp", temp, &required_size_temp);

        if (err != ESP_OK) {
            return err;
        }

        err = nvs_get_blob(my_handle, "saved_ph", ph, &required_size_ph);

        if (err != ESP_OK) {
            return err;
        }

        return err;
    }
}
