#pragma once

#include "embled.h"

typedef enum port_pinout_e
{
    PORT_STATUS,
    PORT_OPER,
    PORT_MAX_LEDS,
} port_pinout_t;

void embled_app_main(void);