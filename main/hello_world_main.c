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



TaskHandle_t task_1;
TaskHandle_t task_2;
TaskHandle_t task_3; // Control LED On or LED Off 
QueueHandle_t xQueue1;
EventGroupHandle_t sensor_event;

int global_var;
int data_flag = 0;
int sock_listen;

void blink_led(void*)
{
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT); // LED
    xEventGroupWaitBits(sensor_event, (1 << 2), pdTRUE, pdFALSE, pdMS_TO_TICKS(0xffffffff));
    while(1)
    {
        char rx_buffer[128] = {0};
        int len = recv(sock_listen, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0)
        {
            printf("Recv Failed");
            return;
        }

        printf("Data Recv = %s\n", rx_buffer);

        if (strstr(rx_buffer, "led on") != 0)
        {
            gpio_set_level(GPIO_NUM_2, 1);
        }
            
        else if (strstr(rx_buffer, "led off") != 0 )
        {
            gpio_set_level(GPIO_NUM_2, 0);
        }

        /*
        gpio_set_level(GPIO_NUM_2, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(GPIO_NUM_2, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        */
    }
}

void led_control(void*)
{
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT); // LED
    
    while(1)
    {
        /**/
        char rx_buffer[128];
        int len = recv(sock_listen, rx_buffer, sizeof(rx_buffer) - 1, 0);

        if (len < 0)
        {
            printf("Recv failed");
        }
        else
        {
            printf("Data Recv = %s\n", rx_buffer);
            if (strstr(rx_buffer, "led on"))
            {
                gpio_set_level(GPIO_NUM_2, 1);
            }
            
            else if (strstr(rx_buffer, "led off"))
            {
                gpio_set_level(GPIO_NUM_2, 0);
            }
        }
    }
    
}


void read_sensor_data(void* param)
{
    int ss_data = 0;
    int cnt = 0;
    while(1)
    {

        //global_var = ss_data;
        if (xQueue1 != 0)
        {
            if (xQueueSend(xQueue1, &ss_data, pdMS_TO_TICKS(10000) != 0))
            {
                // Failed to post the msg
            }
        }
        ss_data ++;
        cnt ++;
        if (cnt == 10)
        {
            //set flag(Event)
            data_flag = 1;
            xEventGroupSetBits(sensor_event, 1 << 0);
            cnt = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

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

    xTaskCreate(blink_led, "blink_led", 2048, NULL, 1, &task_1);
    xTaskCreate(read_sensor_data, "read_sensor", 2048, NULL, 1, &task_2);
    //xTaskCreate(led_control, "led_control", 2048, NULL, 1, &task_3);


    xQueue1 = xQueueCreate(100, sizeof(int));
    if (xQueue1 == 0)
    {
        printf("Queue was not created");
        // Queue was not created
    }
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

    // Create Socket
    int ip_protocol = 0;
    sock_listen = socket(AF_INET, SOCK_STREAM, ip_protocol);
    struct sockaddr_in dest_addr;
    if (sock_listen < 0)
    {
        printf("Create Socket Failed\n");
        return;
    }
    else
    {
        printf("Create Socket Success\n");
    }

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(1234);
    //int error = connect (sock_listen, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    int error = bind(sock_listen, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    if (error != 0)
    {
        printf("Bind To Server Failed\n");
    }
    else
    {
        printf("Bind To Server Success\n");

    }

    error = listen(sock_listen, 1);
    if (error != 0) {
        printf("Error occurred during listen\n");
    }

    while(1)
    {   
        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(sock_listen, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            printf("Unable to accept connection\n");
            break;
        }

        printf("Have a client connection. IP: %s\n",((struct sockaddr *)&source_addr)->sa_data);
    }
}
