#include <stdint.h>
#include <stdlib.h>
#include "esp_log.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "hal/uart_types.h"
#include "nvs.h"
#include "uart_periph.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "portmacro.h"

static const char *TAG = "APP_CONFIG";

esp_err_t app_config_save(const sys_config_t *cfg)
{
    nvs_handle_t nvs_handler;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handler);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao abrir nvs para salvar configuração!");
        return err;
    }

    err = nvs_set_blob(nvs_handler, CONFIG_KEY, cfg, sizeof(sys_config_t));
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao setar blob!");
        return err;
    }

    err = nvs_commit(nvs_handler);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao commitar!");
        return err;
    }

    nvs_close(nvs_handler);
    return err;
}

esp_err_t app_config_load(sys_config_t *cfg)
{
    nvs_handle_t nvs_handler;
    esp_err_t err;

    size_t size = sizeof(sys_config_t);

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handler);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao abrir configuração anterior!");
        return err;
    }

    err = nvs_get_blob(nvs_handler, CONFIG_KEY, cfg, &size);
    
    nvs_close(nvs_handler);
    return err;
}

void app_config_main(void)
{
    ESP_LOGI(TAG, "Procurando por atualizações anteriores!");

    sys_config_t current_cfg = {0};
    esp_err_t err = app_config_load(&current_cfg);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao abrir configuração anterior!");
        return;
    }
    
    for(uint8_t uart_pos = 0; uart_pos < UART_NUM_MAX; uart_pos++)
    {
        if(current_cfg.uarts[uart_pos].state)
            uart_set_new_configure(&current_cfg.uarts[uart_pos]);
    }
}

