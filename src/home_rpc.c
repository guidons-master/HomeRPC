#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include <string.h>
#include <inttypes.h>
#include "HomeRPC.h"
#include "rpc_mdns.h"
#include "rpc_mesh.h"
#include "rpc_mqtt.h"
#include "rpc_log.h"
#include "rpc_data.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_mac.h"

static DeviceList_t *device_list_end = NULL;
static SemaphoreHandle_t addDeviceMutex = NULL;

static const char *TAG = "core";
typedef struct {
    QueueHandle_t device_queue;
    EventGroupHandle_t event_group;
    QueueHandle_t response_queue;
    DeviceList_t *device_list;
    char uri[32];
} SharedData_t;
static SharedData_t SharedData;

static TimerHandle_t timer;

static void mdns_search() {
    static esp_ip4_addr_t addr = { 0 };
    if (addr.addr != 0) return;
    rpc_log.log_info(TAG, "MDNS Search");
    esp_err_t err = rpc_mdns_search(BROKER_URL, &addr);
    if (err == ESP_OK) {
        snprintf(SharedData.uri, sizeof(SharedData.uri), "ws://%d.%d.%d.%d:3000", IP2STR(&addr));
        xTaskCreatePinnedToCore(rpc_mqtt_task, "event_loop", 4096, (void *)&SharedData, 5, NULL, 0);
        xTimerDelete(timer, portMAX_DELAY);
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    rpc_log.log_info(TAG, "Got IP");
    static bool task = false;
    if (!task) {
        RPC_ERROR_CHECK(TAG, rpc_mdns_init());
        timer = xTimerCreate("mdns_search", 1000 / portTICK_PERIOD_MS, pdTRUE, NULL, mdns_search);
        xTimerStart(timer, 0);
        task = true;
    }
}

// only called once
static void rpc_start(void) {
    static bool initialized = false;
    if (initialized)
        return;

    rpc_log.log_info(TAG, "Setup");
    
    rpc_mesh_init(got_ip_event_handler);

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(callback_topic, sizeof(callback_topic), "/callback/%02X%02X%02X", mac[3], mac[4], mac[5]);

    SharedData.device_queue = xQueueCreate(10, sizeof(Device_t *));
    SharedData.event_group = xEventGroupCreate();
    SharedData.response_queue = xQueueCreate(10, sizeof(rpc_any_t));
    addDeviceMutex = xSemaphoreCreateMutex();

    initialized = true;
}

// thread-safe
void rpc_addDevice(const Device_t *dev) {
    if (xSemaphoreTake(addDeviceMutex, portMAX_DELAY) == pdTRUE) {
        DeviceList_t *new_device = malloc(sizeof(DeviceList_t));
        if (new_device == NULL) {
            rpc_log.log_error(TAG, "Failed to allocate memory for new device");
            xSemaphoreGive(addDeviceMutex);
            return;
        }
        new_device->device = malloc(sizeof(Device_t));
        if (new_device->device == NULL) {
            rpc_log.log_error(TAG, "Failed to allocate memory for new device data");
            free(new_device);
            xSemaphoreGive(addDeviceMutex);
            return;
        }
        memcpy(new_device->device, dev, sizeof(Device_t));
        new_device->next = NULL;

        static EventBits_t nextWaitBit = 1;
        for (unsigned int i = 0; i < new_device->device->services_num; i++) {
            new_device->device->services[i]._wait = nextWaitBit;
            nextWaitBit <<= 1;
        }

        if (SharedData.device_list == NULL) {
            SharedData.device_list = new_device;
            device_list_end = new_device;
        } else {
            device_list_end->next = new_device;
            device_list_end = new_device;
        }

        rpc_log.log_info(TAG, "Add Device");
        if (xQueueSendToBack(SharedData.device_queue, &new_device->device, 0) != pdTRUE) {
            rpc_log.log_error(TAG, "Failed to add device to queue");
        }

        xSemaphoreGive(addDeviceMutex);
    }
}

// thread-safe
static rpc_any_t rpc_callService(const Device_t *dev, const char *name, const rpc_any_t *params, unsigned int params_num) {
    rpc_log.log_info(TAG, "Call Service");
    rpc_mqtt_call(dev, name, params, params_num);
    rpc_any_t res;
    TickType_t timeout = pdMS_TO_TICKS(CALL_TIMEOUT);
    if (xQueueReceive(SharedData.response_queue, &res, timeout) == pdTRUE) {
        rpc_log.log_info(TAG, "Service called");
        return res;
    }
    rpc_log.log_error(TAG, "Timeout waiting for response");
    return (rpc_any_t) { .i = -1 };
}

HomeRPC_t HomeRPC = {
    .log_enable = true,
    .log_level = ESP_LOG_ERROR,
    .start = rpc_start,
    .addDevice = rpc_addDevice,
    ._callService = rpc_callService
};