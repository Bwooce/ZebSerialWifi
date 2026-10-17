/*
 * ZebSerialWifi: WiFi to RS232 bridge for a Zebra ZPL printer on a XIAO ESP32-C6.
 * Raw TCP 9100 <-> UART1 (MAX3232), advertised over mDNS as zebserial.local.
 */
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "bridge.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "printer_link.h"
#include "sdkconfig.h"
#include "settings.h"
#include "web.h"
#include "wifi_prov.h"

static const char *TAG = "main";

#define FACTORY_RESET_HOLD_MS 5000
#define STATUS_TICK_MS 100
#define PDL_SERVICE "_pdl-datastream"

/* model in the printer TXT records and service name; saved, and generic until a printer has ever answered ~HI */
static char s_mdns_model[32];
static char s_mdns_host[33];

static void mdns_setup(void)
{
    settings_net_t net;
    settings_get_net(&net);
    strlcpy(s_mdns_host, net.hostname, sizeof(s_mdns_host));
    /* Named after the last printer found, so the name does not change while the printer is off or slow to answer */
    settings_get_model(s_mdns_model);
    char instance[64];  /* a DNS label: 63 bytes at most, so long models are cut to fit a 32-byte hostname */
    char ty[48];
    char product[48];
    if (s_mdns_model[0] != '\0') {
        snprintf(instance, sizeof(instance), "Zebra %.22s (%s)", s_mdns_model, net.hostname);
        snprintf(ty, sizeof(ty), "Zebra %s", s_mdns_model);
        snprintf(product, sizeof(product), "(Zebra %s)", s_mdns_model);
    } else {
        snprintf(instance, sizeof(instance), "ZebSerial (%s)", net.hostname);
        strlcpy(ty, "Zebra ZPL Printer", sizeof(ty));
        strlcpy(product, "(Zebra ZPL Printer)", sizeof(product));
    }
    /* Bonjour printing spec: the printer's web page, shown as "Printer Website" in the macOS print queue */
    char adminurl[64];
    snprintf(adminurl, sizeof(adminurl), "http://%s.local/", net.hostname);
    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set(net.hostname);
    mdns_instance_name_set(instance);
    mdns_txt_item_t txt[] = {
        {"txtvers", "1"},
        {"qtotal", "1"},
        {"ty", ty},
        {"product", product},
        {"usb_MFG", "Zebra"},
        {"usb_MDL", s_mdns_model[0] != '\0' ? s_mdns_model : "ZPL Printer"},
        {"note", "ZebSerialWifi"},
        {"adminurl", adminurl},
    };
    mdns_service_add(instance, PDL_SERVICE, "_tcp", CONFIG_ZEB_TCP_PORT, txt,
                     sizeof(txt) / sizeof(txt[0]));
    mdns_service_add(instance, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS: %s.local, service name \"%s\"", net.hostname, instance);
}

/*
 * Put the model the printer reported into the TXT records print drivers match
 * on and into the service name ("Zebra GX430t (zebserial)", as the macOS
 * printer list shows it), and save it for the next boot.
 */
static void mdns_set_model(const char *model)
{
    char ty[48];
    char product[48];
    char instance[64];
    snprintf(instance, sizeof(instance), "Zebra %.22s (%s)", model, s_mdns_host);
    snprintf(ty, sizeof(ty), "Zebra %s", model);
    snprintf(product, sizeof(product), "(Zebra %s)", model);
    strlcpy(s_mdns_model, model, sizeof(s_mdns_model));  /* also on failure, so it warns once, not every tick */
    esp_err_t err = mdns_service_txt_item_set(PDL_SERVICE, "_tcp", "ty", ty);
    if (err == ESP_OK) {
        err = mdns_service_txt_item_set(PDL_SERVICE, "_tcp", "product", product);
    }
    if (err == ESP_OK) {
        err = mdns_service_txt_item_set(PDL_SERVICE, "_tcp", "usb_MDL", model);
    }
    if (err == ESP_OK) {
        err = mdns_service_instance_name_set(PDL_SERVICE, "_tcp", instance);
    }
    if (err == ESP_OK) {
        err = mdns_service_instance_name_set("_http", "_tcp", instance);
    }
    if (err == ESP_OK) {
        err = mdns_instance_name_set(instance);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS model update failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "mDNS model: %s, service name \"%s\"", model, instance);
    char saved[32];
    settings_get_model(saved);
    if (strcmp(saved, model) != 0 && settings_set_model(model) != ESP_OK) {
        ESP_LOGW(TAG, "could not save the printer model");
    }
}

static void status_task(void *arg)
{
    (void)arg;
    uint32_t held_ms = 0;
    uint32_t ticks = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(STATUS_TICK_MS));
        if (++ticks == 10000 / STATUS_TICK_MS) {
            ESP_LOGI(TAG, "status stack: %u bytes never used after 10 s", (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }

        printer_status_t p;
        printer_link_get_status(&p);
        if (p.model[0] != '\0' && strcmp(p.model, s_mdns_model) != 0) {
            mdns_set_model(p.model);
        }
        switch (wifi_prov_state()) {
        case WIFI_ST_AP:
            board_led_set_mode(LED_AP);
            break;
        case WIFI_ST_CONNECTING:
            board_led_set_mode(LED_CONNECTING);
            break;
        case WIFI_ST_CONNECTED:
            board_led_set_mode(p.answering ? LED_READY : LED_NO_PRINTER);
            break;
        }

        if (board_button_pressed()) {
            held_ms += STATUS_TICK_MS;
            if (held_ms >= FACTORY_RESET_HOLD_MS) {
                ESP_LOGW(TAG, "BOOT held for %d s: clearing WiFi and network settings and rebooting",
                         FACTORY_RESET_HOLD_MS / 1000);
                settings_clear_wifi();
                settings_clear_net();
                vTaskDelay(pdMS_TO_TICKS(200));
                esp_restart();
            }
        } else {
            held_ms = 0;
        }
    }
}

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "ZebSerialWifi %s built %s %s, IDF %s", app->version, app->date, app->time, app->idf_ver);

    ESP_ERROR_CHECK(settings_init());
    board_init();

    settings_options_t opt;
    settings_get_options(&opt);
    int tx;
    int rx;
    bool pins_sure = true;
    if (opt.pins == SETTINGS_PINS_TX_D3) {
        tx = 21;
        rx = 23;
        ESP_LOGI(TAG, "UART pins fixed by setting: TX=GPIO21 (D3) RX=GPIO23 (D5)");
    } else if (opt.pins == SETTINGS_PINS_TX_D5) {
        tx = 23;
        rx = 21;
        ESP_LOGI(TAG, "UART pins fixed by setting: TX=GPIO23 (D5) RX=GPIO21 (D3)");
    } else {
        pins_sure = board_detect_uart_pins(&tx, &rx);
    }
    ESP_ERROR_CHECK(printer_link_init(tx, rx));
    printer_link_allow_pin_swap(!pins_sure);
    printer_link_set_target_baud(opt.target_baud);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_prov_start();
    mdns_setup();
    web_start();
    bridge_start();
    printer_link_start();

    xTaskCreate(status_task, "status", 4096, NULL, 3, NULL);
}
