idf_component_register(SRCS "app_config.c" "app_tasks.c" "main_app.c"
                       PRIV_REQUIRES utl_cbf utl_io esp_driver_gpio esp_timer sdmmc_storage uart_periph
                       INCLUDE_DIRS "include")
