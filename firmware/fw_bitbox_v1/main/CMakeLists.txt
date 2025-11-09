idf_component_register(SRCS "main_app.c"
                       PRIV_REQUIRES utl_cbf utl_io esp_driver_uart esp_driver_gpio esp_timer fatfs esp_driver_sdmmc
                       INCLUDE_DIRS ".")
