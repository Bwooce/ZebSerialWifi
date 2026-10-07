idf_component_register(
    SRCS "main.c" "board.c" "settings.c" "zpl_status.c" "urlcodec.c" "netcfg.c"
         "printer_link.c" "bridge.c" "wifi_prov.c" "dns_hijack.c" "web.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_driver_uart nvs_flash esp_wifi esp_netif esp_event
                  esp_http_server esp_timer esp_app_format lwip
)
