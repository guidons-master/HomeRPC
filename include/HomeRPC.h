#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define ROUTER_SSID     "xxxxxxxxx"
#define ROUTER_PASSWORD "xxxxxxxxx"

#define BROKER_URL      "homerpc"

#define PARAMS_MAX      10
#define TOPIC_LEN_MAX   128
#define CALL_TIMEOUT    60000

#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define RPC_ERROR_CHECK(TAG, err) if ((err)) { \
    rpc_log.log_error((TAG), "Error occurred: %s", esp_err_to_name((err))); \
    esp_restart(); \
}

typedef union {
    char c;
    unsigned char uc;
    short s;
    unsigned short us;
    int i;
    unsigned int ui;
    long l;
    float f;
    double d;
    // char* str; // todo
} __attribute__((aligned(1))) rpc_any_t;

typedef rpc_any_t (*rpc_func_t)(void);

typedef struct {
    /* Public */
    rpc_func_t func;
    char *input_type;
    char output_type;
    const char *name;
    const char *desc;
    /* Private */
    cJSON *_input;
    EventBits_t _wait;
} Service_t;

typedef struct {
    char *place;
    char *type;
    unsigned int id;
    Service_t *services;
    unsigned int services_num;
} Device_t;

struct List_t {
    Device_t* device;
    struct List_t *next;
};
typedef struct List_t DeviceList_t;

typedef struct {
    esp_log_level_t log_level;
    uint8_t log_enable;
    void (*start)(void);
    void (*addDevice)(const Device_t *);
    rpc_any_t (*_callService)(const Device_t *, const char *, const rpc_any_t *, unsigned int);
} HomeRPC_t;

#define callService(device, service, params) _callService((device), (service), (params), (sizeof((params)) / sizeof(rpc_any_t)))

extern HomeRPC_t HomeRPC;

#ifdef __cplusplus
}
#endif