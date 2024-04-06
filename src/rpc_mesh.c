#include "rpc_mesh.h"
#include "rpc_log.h"
#include "HomeRPC.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_wifi.h"
#include "nvs_flash.h"
#include <sys/socket.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#endif

#include "esp_bridge.h"
#include "esp_mesh_lite.h"

static const char *TAG = "rpc_mesh";

static esp_err_t esp_storage_init(void) {
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        rpc_log.log_info(TAG, "NVS Flash Erase");
        RPC_ERROR_CHECK(TAG, nvs_flash_erase());
        ret = nvs_flash_init();
    }

    rpc_log.log_info(TAG, "NVS Flash Init");
    return ret;
}

static void wifi_init(void) {
    // Station
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ROUTER_SSID,
            .password = CONFIG_ROUTER_PASSWORD,
        },
    };
    rpc_log.log_info(TAG, "Setting WiFi Station Config");
    esp_bridge_wifi_set_config(WIFI_IF_STA, &wifi_config);

    // Softap
    snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "%s", CONFIG_BRIDGE_SOFTAP_SSID);
    strlcpy((char *)wifi_config.ap.password, CONFIG_BRIDGE_SOFTAP_PASSWORD, sizeof(wifi_config.ap.password));
    rpc_log.log_info(TAG, "Setting WiFi SoftAP Config");
    esp_bridge_wifi_set_config(WIFI_IF_AP, &wifi_config);
}

void app_wifi_set_softap_info(void) {
    char softap_ssid[32];
    uint8_t softap_mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, softap_mac);
    memset(softap_ssid, 0x0, sizeof(softap_ssid));

#ifdef CONFIG_BRIDGE_SOFTAP_SSID_END_WITH_THE_MAC
    snprintf(softap_ssid, sizeof(softap_ssid), "%.25s_%02x%02x%02x", CONFIG_BRIDGE_SOFTAP_SSID, softap_mac[3], softap_mac[4], softap_mac[5]);
#else
    snprintf(softap_ssid, sizeof(softap_ssid), "%.32s", CONFIG_BRIDGE_SOFTAP_SSID);
#endif
    rpc_log.log_info(TAG, "Setting SoftAP Info");
    esp_mesh_lite_set_softap_ssid_to_nvs(softap_ssid);
    esp_mesh_lite_set_softap_psw_to_nvs(CONFIG_BRIDGE_SOFTAP_PASSWORD);
    esp_mesh_lite_set_softap_info(softap_ssid, CONFIG_BRIDGE_SOFTAP_PASSWORD);
}

void rpc_mesh_init(esp_event_handler_t got_ip_handler) {
    rpc_log.log_info(TAG, "Initializing RPC Mesh");
    esp_storage_init();

    RPC_ERROR_CHECK(TAG, esp_netif_init());
    RPC_ERROR_CHECK(TAG, esp_event_loop_create_default());

    esp_bridge_create_all_netif();
    rpc_log.log_info(TAG, "Creating Netif");

    wifi_init();
    rpc_log.log_info(TAG, "Setting WiFi Mode");

    esp_mesh_lite_config_t mesh_lite_config = ESP_MESH_LITE_DEFAULT_INIT();
    esp_mesh_lite_init(&mesh_lite_config);

    app_wifi_set_softap_info();

    esp_mesh_lite_start();
    rpc_log.log_info(TAG, "Starting Mesh Lite");

    RPC_ERROR_CHECK(TAG, esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_handler, NULL, NULL));
    rpc_log.log_info(TAG, "RPC Mesh Initialized");
}