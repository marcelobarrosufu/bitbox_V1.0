#include <stdio.h>
#include <stdint.h>
#include "gpio_periph.h"

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"

/* ---------- STATIC FUNCTIONS PROTOTYPES ------------*/

static void gpio_apply_config(const gpio_config_t *cfg);

static void gpio_config_save_update(const gpio_config_t *cfg);

/* ----------  GLOBAL FUNCTIONS SOURCES --------------*/

void gpio_set_new_configure(gpio_config_t *cfg);
{

}

/* --------- STATIC FUNCTIONS SOURCES ------------*/

static void gpio_apply_config(const gpio_config_t *cfg)
{

}

static void gpio_config_save_update(const gpio_config_t *cfg);
{

}