/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

#include "sapphire.h"
#include "init.h"
#include "threading.h"


void app_v_init( void ) __attribute__((weak));
void libs_v_init( void ) __attribute__((weak));

void wifi_init_sta();

void app_main()
{
    trace_printf("\r\nESP32 SDK version:%s\r\n", esp_get_idf_version());

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // set run time logging
    // esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    // esp_log_level_set("*", ESP_LOG_DEBUG);

    // sapphireos init
    if( sapphire_i8_init() == 0 ){
            
        if( app_v_init != 0 ){            

            app_v_init();
        }

        if( libs_v_init != 0 ){

            libs_v_init();
        }
    }

    // wifi_init_sta();
    
    sapphire_run();

    while(1){

        thread_core();

        
        // esp_sleep_enable_timer_wakeup( 10000 );
        // esp_light_sleep_start();    
    }
    
    #if 0
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
    #endif
}
