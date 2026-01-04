#pragma once

typedef enum gpio_available_ports_s
{
    GPIO_BOARD_1 = 1,
    GPIO_BOARD_2,
    GPIO_BOARD_3,
    GPIO_BOARD_4,
    GPIO_BOARD_5,

    GPIO_BOARD_33 = 33,
    GPIO_BOARD_34,
    GPIO_BOARD_35,
    GPIO_BOARD_36,
    GPIO_BOARD_37,

    GPIO_BOARD_MAX = 10,
} gpio_available_ports_t;

void gpio_set_new_configure(gpio_config_t *cfg);
