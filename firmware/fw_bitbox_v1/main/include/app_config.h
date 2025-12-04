#pragma once

#define STORAGE_NAMESPACE "storage"
#define CONFIG_KEY        "sys_cfg"

#include "hal/uart_types.h"
#include "uart_periph.h"
#include "esp_err.h"

typedef struct sys_config_s
{
    uint8_t uart_cnt;
    uart_cfg_t uarts[UART_NUM_MAX];
}sys_config_t; 

esp_err_t app_config_save(const sys_config_t *cfg);

esp_err_t app_config_load(sys_config_t *cfg);

void app_config_main(void);

