#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "esp_netif.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"

#include "wifi_conn.h"

#include "esp_http_server.h"
#include "app_config.h"

#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/dns.h"

#pragma pack(push, 1)
typedef struct 
{
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_hdr_t;
#pragma pack(pop)

#define WIFI_MAX_RETRYS 5

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "WIFI_CONN";

static const char *html_page =
"<!DOCTYPE html>"
"<html lang='pt-br'>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>Datalogger Setup</title>"
"<style>"
"body {"
"  font-family: Arial, sans-serif;"
"  background: #0f172a;"
"  color: #e5e7eb;"
"  display: flex;"
"  align-items: center;"
"  justify-content: center;"
"  height: 100vh;"
"  margin: 0;"
"}"
".card {"
"  background: #020617;"
"  border-radius: 14px;"
"  padding: 26px 28px;"
"  width: 100%;"
"  max-width: 400px;"
"  box-shadow: 0 10px 30px rgba(0,0,0,0.6);"
"}"
"h2 {"
"  margin: 0 0 18px 0;"
"  text-align: center;"
"  font-weight: 600;"
"}"
"label {"
"  font-size: 0.85rem;"
"  color: #cbd5f5;"
"}"
"input {"
"  width: 100%;"
"  padding: 11px 12px;"
"  margin-top: 6px;"
"  margin-bottom: 14px;"
"  border-radius: 8px;"
"  border: 1px solid #1e293b;"
"  background: #020617;"
"  color: #e5e7eb;"
"}"
"input:focus {"
"  outline: none;"
"  border-color: #38bdf8;"
"}"
"button {"
"  width: 100%;"
"  padding: 12px;"
"  border: none;"
"  border-radius: 10px;"
"  background: #0284c7;"
"  color: white;"
"  font-size: 1rem;"
"  font-weight: 600;"
"  cursor: pointer;"
"}"
"button:hover {"
"  background: #0369a1;"
"}"
".footer {"
"  margin-top: 14px;"
"  font-size: 0.75rem;"
"  color: #64748b;"
"  text-align: center;"
"}"
".hidden {"
"  display: none;"
"}"
".spinner {"
"  width: 42px;"
"  height: 42px;"
"  border: 4px solid #1e293b;"
"  border-top: 4px solid #38bdf8;"
"  border-radius: 50%;"
"  animation: spin 1s linear infinite;"
"  margin: 20px auto;"
"}"
"@keyframes spin {"
"  0% { transform: rotate(0deg); }"
"  100% { transform: rotate(360deg); }"
"}"
"</style>"
"</head>"

"<body>"

"<div class='card' id='formCard'>"
"<h2>Datalogger Setup</h2>"
"<form method='POST' action='/save' onsubmit='showLoading()'>"

"<label>SSID</label>"
"<input name='ssid' placeholder='Nome da rede Wi-Fi' required>"

"<label>Senha</label>"
"<input name='pass' type='password' placeholder='Senha do Wi-Fi'>"

"<label>MQTT Broker</label>"
"<input name='mqtt' placeholder='ex: mqtt://192.168.1.10' required>"

"<button type='submit'>Salvar Configuração</button>"
"</form>"
"<div class='footer'>Firmware Config Portal</div>"
"</div>"

"<div class='card hidden' id='loadingCard'>"
"<h2>Configurando dispositivo</h2>"
"<div class='spinner'></div>"
"<p style='text-align:center;font-size:0.9rem;color:#cbd5f5;'>"
"Salvando configurações e conectando ao Wi-Fi…"
"</p>"
"</div>"

"<script>"
"function showLoading() {"
"  document.getElementById('formCard').classList.add('hidden');"
"  document.getElementById('loadingCard').classList.remove('hidden');"
"}"
"</script>"

"</body>"
"</html>";


static EventGroupHandle_t wifi_event_group;
static int retry_num = 0;

static struct udp_pcb *dns_pcb;

static const char *portal_uris[] = 
{
    "/",
    "/hotspot-detect.html",          // Apple
    "/library/test/success.html",    // Apple
    "/generate_204",                 // Android
    "/connecttest.txt",              // Windows
};

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t captive_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[224];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0)
    {
        return ESP_FAIL;
    }
        
    buf[len] = '\0';

    sys_config_netw_t netw_cfg = { 0 };

    httpd_query_key_value(buf, "ssid", netw_cfg.ssid, sizeof(netw_cfg.ssid));
    httpd_query_key_value(buf, "pass", netw_cfg.pass, sizeof(netw_cfg.pass));
    httpd_query_key_value(buf, "mqtt", netw_cfg.broker, sizeof(netw_cfg.broker));

    app_config_netw_save(&netw_cfg);

    httpd_resp_sendstr(req, "Configuração salva. Reiniciando...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
        esp_wifi_connect();
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if(retry_num < WIFI_MAX_RETRYS)
        {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "Tentando se conectar no WiFi");
        }
        else
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);

        ESP_LOGI(TAG, "Falha na Conexão Wi-Fi");
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Conectado com IP: "IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}   

static void http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server;

    httpd_start(&server, &config);

    for (int i = 0; i < sizeof(portal_uris)/sizeof(portal_uris[0]); i++)
    {
        httpd_uri_t uri = 
        {
            .uri = portal_uris[i],
            .method = HTTP_GET,
            .handler = captive_get_handler
        };

        httpd_register_uri_handler(server, &uri);
    }

    httpd_uri_t save = 
    {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_post_handler
    };

    httpd_register_uri_handler(server, &save);
}

static void dns_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    if (!p) 
    {
        return;
    }

    dns_hdr_t *hdr = (dns_hdr_t *)p->payload;

    hdr->flags = htons(0x8180); 
    hdr->ancount = htons(1);

    uint8_t *payload = (uint8_t *)p->payload;
    uint16_t len = p->len;

    uint16_t idx = sizeof(dns_hdr_t);

    while (payload[idx] != 0 && idx < len)
        idx += payload[idx] + 1;
    idx++;            
    idx += 4;         

    payload[idx++] = 0xC0; payload[idx++] = 0x0C; 
    payload[idx++] = 0x00; payload[idx++] = 0x01; 
    payload[idx++] = 0x00; payload[idx++] = 0x01; 
    payload[idx++] = 0x00; payload[idx++] = 0x00;
    payload[idx++] = 0x00; payload[idx++] = 0x3C; 
    payload[idx++] = 0x00; payload[idx++] = 0x04; 

    payload[idx++] = 192;
    payload[idx++] = 168;
    payload[idx++] = 4;
    payload[idx++] = 1;

    p->len = idx;
    p->tot_len = idx;

    udp_sendto(pcb, p, addr, port);
    pbuf_free(p);
}

static void wifi_dns_start(void)
{
    dns_pcb = udp_new();
    udp_bind(dns_pcb, IP_ADDR_ANY, 53);
    udp_recv(dns_pcb, dns_recv_cb, NULL);
}

static void wifi_init_ap(void)
{
    wifi_config_t wifi_ap_cfg = 
    {
        .ap = 
        {
            .ssid = "BITBOX_POLENTA_E_DROGA",
            .ssid_len = strlen("BITBOX_POLENTA_E_DROGA"),
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        }
    };
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_cfg);
    esp_wifi_start();
}

void wifi_start_captive_portal(void)
{
    ESP_LOGI(TAG, "Iniciando captive portal");

    wifi_init_ap();
    wifi_dns_start();
    http_server_start();

    ESP_LOGI(TAG, "Captive portal ativo em http://192.168.4.1");
}

void wifi_conn_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    sys_config_netw_t netw_cfg = { 0 };                        

    if(app_config_netw_load(&netw_cfg))
    {
        ESP_LOGW(TAG, "Nenhum wifi anterior configurado! Abrindo portal.");
        wifi_start_captive_portal();
        return;
    }

    ESP_LOGI(TAG, "Tentando realizar conexão com a rede %s!", netw_cfg.ssid);

    wifi_config_t wifi_cfg = 
    {
        .sta =
        {
            .pmf_cfg =
            {
                .capable = true,
                .required = false,
            },
        },
    };
    
    strncpy((char *)wifi_cfg.sta.ssid, netw_cfg.ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, netw_cfg.pass, sizeof(wifi_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Inicializando Wi-Fi STA...");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) 
    {
        ESP_LOGI(TAG, "Conectado à rede: %s", wifi_cfg.sta.ssid);
        retry_num = 0;
    }
    else if (bits & WIFI_FAIL_BIT) 
    {
        ESP_LOGE(TAG, "Falha ao conectar à rede: %s", wifi_cfg.sta.ssid);
        wifi_start_captive_portal();
    }
    else 
        ESP_LOGE(TAG, "Evento inesperado");

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(wifi_event_group);
}
