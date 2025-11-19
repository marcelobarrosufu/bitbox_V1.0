#pragma once

#define RX_BUFFER_SIZE 100000
#define TX_BUFFER_SIZE RX_BUFFER_SIZE

bool uart_periph_driver_init(void);
