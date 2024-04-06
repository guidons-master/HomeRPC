#include "mdns.h"
#include "rpc_mdns.h"
#include "rpc_log.h"
#include "esp_err.h"
#include "esp_netif.h"

static const char *TAG = "rpc_mdns";

esp_err_t rpc_mdns_init(void)
{
    esp_err_t err = mdns_init();
    if (err) {
        rpc_log.log_error(TAG, "MDNS Init failed");
        return err;
    }
    return ESP_OK;
}

esp_err_t rpc_mdns_search(const char *host, esp_ip4_addr_t *ip)
{
    assert(ip != NULL);
    rpc_log.log_info(TAG, "Searching for %s", host);
    esp_err_t err = mdns_query_a(host, 500, ip);
    if (err) {
        if (err == ESP_ERR_NOT_FOUND)
            rpc_log.log_info(TAG, "Host was not found!");
        else
            rpc_log.log_info(TAG, "Query Failed");
        return err;
    } else {
        rpc_log.log_info(TAG, "Found %s at " IPSTR, host, IP2STR(ip));
        return ESP_OK;
    }
}

void rpc_mdns_stop(void)
{
    mdns_free();
}