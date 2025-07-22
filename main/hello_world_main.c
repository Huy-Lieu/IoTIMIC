#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <sys/socket.h>
#include <string.h>
#include <esp_http_server.h>



TaskHandle_t task_1;
TaskHandle_t task_2;
TaskHandle_t task_3; // Control LED On or LED Off 
QueueHandle_t xQueue1;
EventGroupHandle_t sensor_event;

#ifndef MAC2STR
    #define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
    #define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch(event_id)
        {
            case WIFI_EVENT_WIFI_READY:
                printf("WIFI READY\n");
                break;
            case WIFI_EVENT_STA_START:
                printf("WIFI STATION START\n");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                uint8_t mac[6] = {0};
                if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK)
                {
                    ESP_LOGW("MAC", "STA MAC: " MACSTR "", MAC2STR(mac));
                }
                printf("WIFI CONNECTED\n");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                printf("WIFI DISCONNECTED\n");
                break;
            default:
                printf("event id: %ld\n", event_id);
                break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch(event_id)
        {
            case IP_EVENT_STA_GOT_IP:
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                printf("ESP 32 IP: %d.%d.%d.%d\n",  
                                                (unsigned char)(event->ip_info.ip.addr & 0xFF),
                                                (unsigned char)(event->ip_info.ip.addr >> 8 & 0xFF),
                                                (unsigned char)(event->ip_info.ip.addr >> 16 & 0xFF),
                                                (unsigned char)(event->ip_info.ip.addr >> 24 & 0xFF));
                
                 xEventGroupSetBits(sensor_event, 1 << 1);

                break;
        }
    }
}

void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        printf("Error");
    }


    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT); // Button BOOT
    
    // Create Event
    sensor_event = xEventGroupCreate();

    int ss_data;

    // WIFI IOT
    
    //Create Network Interface
    esp_netif_init();

    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) == ESP_OK)
    {
        printf("Initalize Wifi success\n");
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                        &event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &event_handler, NULL, &instance_got_ip);
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "MyInternet",
            .password = "ChatIsThisReallyInternet?"
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    ESP_ERROR_CHECK(esp_wifi_start());

    xEventGroupWaitBits(sensor_event, (1 << 1), pdTRUE, pdFALSE, pdMS_TO_TICKS(10000000000));


    while(1)
    {   
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
