idf_component_register(SRCS "gpio_periph.c"
                    REQUIRES sd_log esp_driver_gpio 
                    INCLUDE_DIRS "include")
