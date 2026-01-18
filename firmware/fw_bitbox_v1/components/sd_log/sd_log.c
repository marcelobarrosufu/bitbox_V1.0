#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"

#include "sd_log.h"
#include "uart_periph.h"
#include "sdmmc_storage.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_timer.h"

#define SD_LOG_QUEUE_DEPTH 64

static const char *TAG = "SD_LOG";

bool sd_log_data(const sd_log_msg_t *msg)
{
    if(!msg)
    {
        return false;
    }

    const sd_log_msg_t *record_data = msg;
    
    switch(record_data->log_header.log_type)
    {
        case SD_LOG_UART:

            if (record_data->uart.payload_len > UART_MAX_PAYLOAD_LEN)
            {
                ESP_LOGE(TAG, "Payload UART inválido: %u", record_data->uart.payload_len);
                return false;
            }
            
            if(!sdmmc_stor_record_data_bin(&record_data->log_header, sizeof(record_data->log_header)))
            {
                return false;
            }

            if(!sdmmc_stor_record_data_bin(&record_data->uart.payload_len, sizeof(record_data->uart.payload_len)))
            {
                return false;
            }

            if(!sdmmc_stor_record_data_bin(record_data->uart.payload,record_data->uart.payload_len))
            {
                return false;
            }

            ESP_LOGI(TAG, "%d bytes da UART%d gravados!", record_data->uart.payload_len, record_data->log_header.periph_num);

            return true;

        case SD_LOG_GPIO:

            if(!sdmmc_stor_record_data_bin(&record_data->log_header, sizeof(record_data->log_header)))
            {
                return false;
            }

            if(!sdmmc_stor_record_data_bin(&record_data->gpio, sizeof(record_data->gpio)))
            {
                return false;
            }

            ESP_LOGI(TAG, "Estado do GPIO%d: %d! Gravado!", record_data->log_header.periph_num, record_data->gpio.level);

            return true;

        default:

            ESP_LOGW(TAG, "Tipo de log não suportado!");

            return false;
    }
}

