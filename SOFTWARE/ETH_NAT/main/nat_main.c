/* ethernet Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

   2018 March 26 - this example is based on the one for ESP32-EVB:
   2017 June 15 rudi ;-)
   change log
   edit ETH PHY Example for ESP32-GATEWAY REV B IoT LAN8710 Board with CAN
   - Board use LAN8710 without OSC Power Enable PIN
   - - TLK110 was removed
   - - Kconfig menuconfig TLK110 entry removed cause not need here
   - - we use LAN8720 config for the LAN8710 ( compatible )
   - - Power PIN was not used in shematic and board so not used in code
   - -


*/
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_eth.h"

#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/netif.h"

#include "rom/ets_sys.h"
#include "rom/gpio.h"

#include "soc/dport_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"

#include "tcpip_adapter.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "eth_phy/phy_lan8720.h"
#define DEFAULT_ETHERNET_PHY_CONFIG phy_lan8720_default_ethernet_config

static const char *TAG = "Olimex_ESP32_GATEWAY_REV_B_eth_example";

// not need here in this Board Configuration
// #define PIN_PHY_POWER CONFIG_PHY_POWER_PIN
#define PIN_SMI_MDC   CONFIG_PHY_SMI_MDC_PIN
#define PIN_SMI_MDIO  CONFIG_PHY_SMI_MDIO_PIN

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_MAX_STA_CONN       CONFIG_MAX_STA_CONN

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;


static void eth_gpio_config_rmii(void)
{
    // RMII data pins are fixed:
    // TXD0 = GPIO19
    // TXD1 = GPIO22
    // TX_EN = GPIO21
    // RXD0 = GPIO25
    // RXD1 = GPIO26
    // CLK == GPIO0
    phy_rmii_configure_data_interface_pins();
    // MDC is GPIO 23, MDIO is GPIO 18
    phy_rmii_smi_configure_pins(PIN_SMI_MDC, PIN_SMI_MDIO);
}

void eth_task(void *pvParameter)
{
    tcpip_adapter_ip_info_t ip;
    memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    while (1) {

        vTaskDelay(2000 / portTICK_PERIOD_MS);

        if (tcpip_adapter_get_ip_info(ESP_IF_ETH, &ip) == 0) {
        	ESP_LOGI(TAG, "~~~~~~~~~~~");
            ESP_LOGI(TAG, "ETHIP:"IPSTR, IP2STR(&ip.ip));
            ESP_LOGI(TAG, "ETHPMASK:"IPSTR, IP2STR(&ip.netmask));
            ESP_LOGI(TAG, "ETHPGW:"IPSTR, IP2STR(&ip.gw));
            ESP_LOGI(TAG, "~~~~~~~~~~~");
        }
    }
}


/**
 * @brief event handler for ethernet
 *
 * @param ctx
 * @param event
 * @return esp_err_t
 */
static esp_err_t eth_event_handler(void *ctx, system_event_t *event)
{
    tcpip_adapter_ip_info_t ip;

    switch (event->event_id) {
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;

    case SYSTEM_EVENT_ETH_CONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Up");
        break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case SYSTEM_EVENT_ETH_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case SYSTEM_EVENT_ETH_GOT_IP:
        memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));
        ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(ESP_IF_ETH, &ip));
        ESP_LOGI(TAG, "Ethernet Got IP Addr");
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip.ip));
        ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip.netmask));
        ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip.gw));
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        break;
    case SYSTEM_EVENT_ETH_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_softap()
{
    s_wifi_event_group = xEventGroupCreate();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}


void app_main()
{
    esp_err_t ret = ESP_OK;
    tcpip_adapter_init();
    //esp_event_loop_init(NULL, NULL);
    ESP_ERROR_CHECK(esp_event_loop_init(eth_event_handler, NULL));


    //Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();



    eth_config_t config = DEFAULT_ETHERNET_PHY_CONFIG;
    /* Set the PHY address in the example configuration */
    config.phy_addr = CONFIG_PHY_ADDRESS;
    config.gpio_config = eth_gpio_config_rmii;
    config.tcpip_input = tcpip_adapter_eth_input;

    ret = esp_eth_init(&config);

    // List all interfaces.
    struct netif *netif;

    for (netif = netif_list; netif != NULL; netif = netif->next) {
        printf("%c%c%d\n",netif->name[0],netif->name[1],netif->num);
        fflush(stdout);
    }


    netif=netif_find("st1");
    if (!netif) {
        printf("st1 not found!!\n");
    }


    if(ret == ESP_OK) {
        esp_eth_enable();
        xTaskCreate(eth_task, "eth_task", 2048, NULL, (tskIDLE_PRIORITY + 2), NULL);
    }

}
