#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "mdns.h"
#include "esp_netif.h"

esp_err_t rpc_mdns_init(void);
esp_err_t rpc_mdns_search(const char *, esp_ip4_addr_t *);
void rpc_mdns_stop(void);

#ifdef __cplusplus
}
#endif