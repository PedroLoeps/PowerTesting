#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>


#include "nvs_flash.h"
#include "wifi_util.h"
#include "blufi_util.h"
#include "sensor_util.h"
#include "nvs_util.h"
#include "mqtt_util.h"
#include "esp_log.h"

#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include "esp_sleep.h"
#include "esp_mac.h"

#include "driver/gpio.h"

extern struct wifi_info wifi_inf;

//Remove so RTC Slow Memory stays off during deep sleep
//static RTC_DATA_ATTR struct timeval sleep_enter_time;

static const char *TAG = "TESTING";

const static char* LOG_TOPIC = "sensor/log";

extern bool config_done;

void app_main(void)
{
    nvs_init();

    struct timeval now, start;
    //gettimeofday(&now, NULL);
    //int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;

    //TODO get MAC adress from the microcontroller

    switch(esp_sleep_get_wakeup_cause()) 
    {
        case ESP_SLEEP_WAKEUP_TIMER:
            //printf("Wake up from timer. Time spent in deep sleep: %dms\n", sleep_time_ms);

            ESP_LOGI(TAG, "Waiting 20 seconds");
            vTaskDelay(20000 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "Waiting 20 seconds");

            ESP_LOGI(TAG, "GPIO 8 set to 1");
            gpio_set_direction(GPIO_NUM_8, GPIO_MODE_OUTPUT);
            gpio_set_level(GPIO_NUM_8, 1);

            ESP_LOGI(TAG, "GPIO 8 on hold");
            gpio_hold_en(GPIO_NUM_8);

            ESP_LOGI(TAG, "Enter light sleep for 20 secs");
            esp_sleep_enable_timer_wakeup(20 * 1000000);
            esp_light_sleep_start();

            ESP_LOGI(TAG, "Wakeup from light sleep");

            ESP_LOGI(TAG, "GPIO 8 not on hold");
            gpio_hold_dis(GPIO_NUM_8);
            ESP_LOGI(TAG, "GPIO 8 set to 0");
            gpio_set_level(GPIO_NUM_8, 0);

            //sensors_init();

            ESP_LOGI(TAG, "WIFI Initialized");
            initialise_wifi();
            vTaskDelay(20000 / portTICK_PERIOD_MS);

            ESP_LOGI(TAG, "MQTT Initialized");
            mqtt_client_init();
            vTaskDelay(20000 / portTICK_PERIOD_MS);

            char log_message[60];
            sprintf(log_message, "00:00:00:00:00:00 4 30 7.0 30 7.0 30 7.0 30 7.0");


            gettimeofday(&start, NULL);
            gettimeofday(&now, NULL);
            int m_sent = 0;

            ESP_LOGI(TAG, "Sending Messages");
            while((now.tv_sec - start.tv_sec) < 20)
            {
                mqtt_send_data(LOG_TOPIC, log_message);
                m_sent++;
                gettimeofday(&now, NULL);
            }
            int i = (now.tv_sec - start.tv_sec) * 1000000 + (now.tv_usec - start.tv_usec);
            ESP_LOGI(TAG, "Messages Done");
            ESP_LOGI(TAG, "%d Messages sent", m_sent);

            ESP_LOGI(TAG, "%d us Time Spent", i);
                

            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            printf("Not a deep sleep reset\n");
            
            esp_err_t err = esp_blufi_host_and_cb_init();
            if (err) 
            {
                BLUFI_ERROR("%s initialise failed: %s\n", __func__, esp_err_to_name(err));
                return;
            }
            BLUFI_INFO("BLUFI VERSION %04x\n", esp_blufi_get_version());
            ESP_LOGI(TAG, "Bluetooth Initiated");
            vTaskDelay(20000 / portTICK_PERIOD_MS);
            initialise_wifi();
            ESP_LOGI(TAG, "Waiting Configuration");
            while(config_done == false)
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            ESP_LOGI(TAG, "Bluetooth Terminated");
            //esp_blufi_host_deinit();
    }

    const int wakeup_time_sec = 20;
    printf("Enabling timer wakeup, %ds\n", wakeup_time_sec);
    esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);



    printf("Entering deep sleep\n");
    //gettimeofday(&sleep_enter_time, NULL);

    vTaskDelay(10 / portTICK_PERIOD_MS);

    esp_deep_sleep_start();
    
}