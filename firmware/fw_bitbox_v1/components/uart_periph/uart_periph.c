#include <stdio.h>
#include <stdlib.h>
#include "freertos/idf_additions.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"

#include "uart_periph.h"
#include "sdmmc_storage.h"
#include "main_app.h"

#include "portmacro.h"
#include "utl_cbf.h"

#define STRINGFY(x) #x

#define PREDEFINED_SIZE 5000

UTL_CBF_DECLARE(uart_rx_buff_0, RX_BUFFER_SIZE);
UTL_CBF_DECLARE(uart_rx_buff_1, RX_BUFFER_SIZE);

static const char *TAG = "UART_PERIPH";

static QueueHandle_t uart_queue;

typedef struct 
{
    const char *file_name; // file_name deve conter apenas o nome do arquivo, sem "/" e sem extensão ".txt ou .bin"
    SemaphoreHandle_t semaphore_full_buff;
}save_data_params_t;

typedef enum
{
    UART_BUFFER_0,
    UART_BUFFER_1,
    MAX_BUFFER_NUMS,
}uart_buff_t;

save_data_params_t params_buff_0 =
{
    .file_name = "BUFF_0",
};

save_data_params_t params_buff_1 =
{
    .file_name = "BUFF_1",
};

uart_buff_t active_buffer = UART_BUFFER_0;

bool cbf_0_ready_to_fill = true;
bool cbf_1_ready_to_fill = true;

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
/* ---------- STATIC FUNCTIONS PROTOTYPES ------------*/

static void uart_periph_process_uart_rx(uint8_t c); // uart "cbk" 

static size_t uart_periph_get_buf_data(uint8_t *p_buff, size_t size);

static void uart_periph_record_data_SD(void *args);

/* ----------  GLOBAL FUNCTIONS SOURCES --------------*/

bool uart_periph_driver_init(void)
{   
    if(uart_param_config(uart_num, &uart_cfg) != ESP_OK)
        return false;

    if(uart_set_pin(uart_num, 4, 5, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
        return false;

    if(uart_driver_install(uart_num, 100 *1024, 0, 20, &uart_queue, 0) != ESP_OK)
        return false;

    SemaphoreHandle_t sem_buff_0 = xSemaphoreCreateBinary();
    SemaphoreHandle_t sem_buff_1 = xSemaphoreCreateBinary();

    params_buff_0.semaphore_full_buff = sem_buff_0;
    params_buff_1.semaphore_full_buff = sem_buff_1;

    xTaskCreate(uart_periph_record_data_SD, "task_buff_0", 4096, &params_buff_0, 1, NULL);
    xTaskCreate(uart_periph_record_data_SD, "task_buff_1", 4096, &params_buff_1, 1, NULL);

    return true;
}   

void uart_periph_driver_task(void *arg)
{
    uart_event_t event;
    uint8_t data[128] = {0};

    while (1)
    {
        if(xQueueReceive(uart_queue, &event, portMAX_DELAY))
        {
            switch (event.type)
            {
                case UART_DATA:
                    int len = uart_read_bytes(uart_num, data, event.size, portMAX_DELAY);
                    for(int idx = 0; idx < len; idx++)
                        uart_periph_process_uart_rx(data[idx]);

                    break;

                case UART_BUFFER_FULL:
                    uart_flush_input(uart_num);
                    xQueueReset(uart_queue);
                
                default:
                    break;
            }
        }
    }
}

/* --------- STATIC FUNCTIONS SOURCES ------------*/

static size_t uart_periph_get_buf_data(uint8_t *p_buff, size_t size)
{
    utl_cbf_t *buff = (active_buffer == UART_BUFFER_0) ? &uart_rx_buff_1 : &uart_rx_buff_0;

    size_t bytes_to_read = size;
    size_t bytes_written = 0;

    if(bytes_to_read >= utl_cbf_bytes_available(buff))
        bytes_to_read = utl_cbf_bytes_available(buff);

    bytes_to_read++;
    
    for(bytes_written = 0; bytes_written < bytes_to_read; bytes_written++)
    {
        if(utl_cbf_get(buff, &p_buff[bytes_written]) == UTL_CBF_EMPTY)
        {
            ESP_LOGW(TAG, "Aborting NOW. %d bytes have been written!", bytes_written);
            (active_buffer == UART_BUFFER_0) ? (cbf_0_ready_to_fill = true) : (cbf_1_ready_to_fill = true);
            break;
        }
    }

    return bytes_written;
}

static void uart_periph_process_uart_rx(uint8_t c)
{
    utl_cbf_t *buff = (active_buffer == UART_BUFFER_0) ? &uart_rx_buff_0 : &uart_rx_buff_1;
    save_data_params_t *params = (active_buffer == UART_BUFFER_0) ? &params_buff_0 : &params_buff_1;

    if(utl_cbf_put(buff, c) != UTL_CBF_OK)
    {
        if(active_buffer == UART_BUFFER_0)
        {
            cbf_0_ready_to_fill = false;
            cbf_1_ready_to_fill = true;
            active_buffer = UART_BUFFER_1;
            utl_cbf_put(&uart_rx_buff_1, c);
            ESP_LOGI(TAG, "%s cheio, trocando para %s!",STRINGFY(UART_BUFFER_0), STRINGFY(UART_BUFFER_1));
        }
        else
        {
            cbf_0_ready_to_fill = true;
            cbf_1_ready_to_fill = false;
            active_buffer = UART_BUFFER_0;
            utl_cbf_put(&uart_rx_buff_0, c);
            ESP_LOGI(TAG, "%s cheio, trocando para %s!",STRINGFY(UART_BUFFER_1), STRINGFY(UART_BUFFER_0));
        }

        xSemaphoreGive(params->semaphore_full_buff);
    }
}

static void uart_periph_record_data_SD_task(void *args)
{
    save_data_params_t *data_params = (save_data_params_t *)args;

    static uint8_t temp_buff[PREDEFINED_SIZE] = {0};

    while(1)
    {
        if(xSemaphoreTake(data_params->semaphore_full_buff, portMAX_DELAY) == pdTRUE)
        {
           ESP_LOGI(TAG, "SD Task (Buff %s): Sinal recebido! Gravando dados.", data_params->file_name);
            
            size_t bytes_lidos = uart_periph_get_buf_data(temp_buff, PREDEFINED_SIZE);
            
            if (bytes_lidos > 0)
            {
                sdmmc_stor_record_data_txt(temp_buff, bytes_lidos, data_params->file_name);
                ESP_LOGI(TAG, "SD Task (Buff %s): Gravado %d bytes.", data_params->file_name, bytes_lidos);
            } 
        }
    }
}