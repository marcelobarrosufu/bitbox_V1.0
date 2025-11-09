/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include "driver/sdmmc_default_configs.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "portmacro.h"
#include "ff.h"

#include "driver/uart.h"
#include "hal/uart_types.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_system.h"
#include "soc/gpio_num.h"
#include "soc/soc_caps.h"

#include "utl_cbf.h"

#define RX_BUFFER_SIZE 100000
#define TX_BUFFER_SIZE RX_BUFFER_SIZE

const char *TAG = "MAIN_APP";

/* ------- SDMMC INTERFACE -------*/
#define SDMMC_CLK_PIN    GPIO_NUM_10
#define SDMMC_CMD_PIN    GPIO_NUM_11
#define SDMMC_D0_PIN     GPIO_NUM_9
#define SDMMC_D1_PIN     GPIO_NUM_8
#define SDMMC_D2_PIN     GPIO_NUM_13
#define SDMMC_D3_PIN     GPIO_NUM_12

sdmmc_host_t host;
sdmmc_slot_config_t slot_cfg;
sdmmc_card_t *card;

/*------- UART BUFFERS -------*/
UTL_CBF_DECLARE(uart_rx_buff_0, RX_BUFFER_SIZE);
UTL_CBF_DECLARE(uart_rx_buff_1, RX_BUFFER_SIZE);

/*------- UART CONFIGURATIONS -------*/ 
const uart_port_t uart_num = UART_NUM_0;
uart_config_t uart_cfg = 
{
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .stop_bits = UART_STOP_BITS_1,
    .parity = UART_PARITY_DISABLE,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
};

/* ------- FILE SYSTEM PATH -------*/
#define MOUNT_POINT "/sdcard"
#define FILE_PATH_0 MOUNT_POINT"/buff0.txt"
#define FILE_PATH_1 MOUNT_POINT"/buff1.txt"

/* ------- AUX VARS -------*/
int64_t t_start_buff_0 = 0;
int64_t t_start_buff_1 = 0;

int64_t t_end_buff_0 = 0;
int64_t t_end_buff_1 = 0;

int64_t t_buff_0 = 0;
int64_t t_buff_1 = 0;

bool buff_0_full = false;
bool buff_1_full = false;

bool sd_card_mounted = false; 

static void sdmmc_cfg_host(void)
{
    host = (sdmmc_host_t)SDMMC_HOST_DEFAULT();
    host.max_freq_khz = 40000;

    slot_cfg = (sdmmc_slot_config_t)SDMMC_SLOT_CONFIG_DEFAULT();
    slot_cfg.clk = SDMMC_CLK_PIN;
    slot_cfg.cmd = SDMMC_CMD_PIN;
    slot_cfg.d0  =  SDMMC_D0_PIN;
    slot_cfg.d1  =  SDMMC_D1_PIN;
    slot_cfg.d2  =  SDMMC_D2_PIN;
    slot_cfg.d3  =  SDMMC_D3_PIN;
    slot_cfg.width = 4;
} 

static bool sdmmc_mount_device(void)
{
    esp_err_t ret;
    // const char mount_point[] = "/sdcard";

    sdmmc_cfg_host();

    esp_vfs_fat_sdmmc_mount_config_t mount_config =
    {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    ESP_LOGI("SD_TEST", "Monting SD card...");
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host,
                                &slot_cfg,  &mount_config,
                                   &card);
    
    if(ret != ESP_OK)
    {
        ESP_LOGE("SD_TEST", "Failed mount SD card (%s)", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI("SD TEST", "SD card mounted. Type: %s", card->cid.name);

    sdmmc_card_print_info(stdout, card);


    FILE* test_file = fopen(MOUNT_POINT"/test.txt", "w");
    if(test_file == NULL) 
    {
        ESP_LOGE("SD_TEST", "Falha no teste de escrita no SD card");
        sd_card_mounted = false;
        return false;
    }
    fprintf(test_file, "Teste de escrita SD card\n");
    fclose(test_file);

    remove(MOUNT_POINT"/test.txt");

    sd_card_mounted = true;
    ESP_LOGI("SD_TEST", "SD card verificado e pronto para uso");
    return true;
}

static void app_init_periph(void)
{
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 100 *1024, 
                                        0, 0, 
                                        NULL, 0));
    
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_cfg));

    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 4, 
                                 5, UART_PIN_NO_CHANGE, 
                                 UART_PIN_NO_CHANGE));

    if (!sdmmc_mount_device()) 
    {
        ESP_LOGE(TAG, "Falha crítica: SD card não disponível");
    }
}

static void app_save_buff1(void *arg)
{
    FILE *f = fopen(FILE_PATH_1, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        vTaskDelete(NULL);
        return;
    }

    uint8_t c;
    int64_t start = esp_timer_get_time();
    uint32_t total = utl_cbf_bytes_available(&uart_rx_buff_1);

    while (utl_cbf_bytes_available(&uart_rx_buff_1)) {
        utl_cbf_get(&uart_rx_buff_1, &c);
        fwrite(&c, 1, 1, f);
    }

    fclose(f);
    int64_t end = esp_timer_get_time();
    ESP_LOGI(TAG, "Wrote %u bytes in %"PRId64" us", total, end - start);

    vTaskDelete(NULL);
}

static void app_save_buff0(void *arg)
{
    FILE *f = fopen(FILE_PATH_0, "w");

    if(f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        vTaskDelete(NULL);
        return;
    }

    uint8_t c;
    int64_t start = esp_timer_get_time();
    uint32_t total = utl_cbf_bytes_available(&uart_rx_buff_0);

    while(utl_cbf_bytes_available(&uart_rx_buff_0)) 
    {
        utl_cbf_get(&uart_rx_buff_0, &c);
        fwrite(&c, 1, 1, f);
    }

    fclose(f);
    int64_t end = esp_timer_get_time();
    ESP_LOGI(TAG, "Wrote %u bytes in %"PRId64" us", total, end - start);
    
    xTaskCreate(app_save_buff1, "app_save_buff1", 8192, NULL, 3, NULL);
    vTaskDelete(NULL);          
}

static void app_rx_task(void *arg)
{
    t_start_buff_0 = esp_timer_get_time();
    static const char *RX_TAG = "RX_TASK";
    esp_log_level_set(RX_TAG, ESP_LOG_INFO);
    while (1)
    {
        static uint8_t c = 0;
        if(uart_read_bytes(UART_NUM_1, &c, 1, pdMS_TO_TICKS(10)))
        {
            if(utl_cbf_put(&uart_rx_buff_0, c) == UTL_CBF_FULL)
            {   
                if(!buff_0_full)
                {
                    t_end_buff_0 = esp_timer_get_time();
                    t_start_buff_1 = esp_timer_get_time();

                    t_buff_0 = t_end_buff_0 - t_start_buff_0;
                    ESP_LOGI(RX_TAG, "Buffer 0 FULL!! %"PRId64" us\n", t_buff_0);
                    ESP_LOGI(RX_TAG, "Starting fill buff 1!!\n");

                    buff_0_full = true;
                }
                
                if(utl_cbf_put(&uart_rx_buff_1, c) == UTL_CBF_FULL)
                {
                    if(!buff_1_full)
                    {
                        t_end_buff_1 = esp_timer_get_time();

                        t_buff_1 = t_end_buff_1 - t_start_buff_1;

                        ESP_LOGI(RX_TAG, "Buffer 1 FULL!! %"PRId64" us\n", t_buff_1);
                        ESP_LOGI(RX_TAG, "Stoping the receive!\n");
                        buff_1_full = true;

                        xTaskCreate(app_save_buff0, "app_save_buff0", 8192, NULL, 3, NULL);
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "STARTING APP...");
    app_init_periph();
    xTaskCreate(app_rx_task, "uart_rx_task", 3072, NULL, 2, NULL);
}
