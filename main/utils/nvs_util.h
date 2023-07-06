
#pragma once

#include "nvs_flash.h"
#include "esp_wifi_types.h"

struct sensor_config
{
    //Number of readings on each day
    int readings;
    //Number of readings done on this day
    int current_readings;
    //Number of times the sensor wakes up to check alarms before 
    //registering a reading
    int wb_reading;
    //Number of times the sensor has woken up before reading
    int current_wb_readings;
};


esp_err_t nvs_init();
esp_err_t open_nvs(const char* namespace, nvs_handle_t* my_handle);
esp_err_t set_saved_wifi(wifi_config_t* wifi_config);
esp_err_t get_saved_wifi(wifi_config_t* wifi_config);
esp_err_t set_saved_config(struct sensor_config* sensor);
esp_err_t get_saved_config(struct sensor_config* sensor);
esp_err_t set_saved_readings(int* temp, float* ph, int size);
esp_err_t get_saved_readings(int* temp, float* ph);