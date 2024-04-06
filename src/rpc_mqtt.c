#include "rpc_mqtt.h"
#include "rpc_log.h"
#include "HomeRPC.h"
#include "mqtt_client.h"
#include "rpc_data.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>
#include <cJSON.h>

static esp_mqtt_client_handle_t client;
static const char* TAG = "rpc_mqtt";

typedef struct {
    QueueHandle_t device_queue;
    EventGroupHandle_t event_group;
    QueueHandle_t response_queue;
    DeviceList_t *device_list;
    char uri[32];
} SharedData_t;
static SharedData_t* SharedData = NULL;
char callback_topic[TOPIC_LEN_MAX];
static QueueHandle_t SearchQueue = NULL;

static rpc_any_t exec(rpc_func_t func, unsigned char len, const rpc_any_t* params) {
    switch (len) {
        case 0:
            return func();
#if PARAMS_MAX >= 1
        case 1:
            return ((rpc_any_t (*)(rpc_any_t))func)(params[0]);
#endif
#if PARAMS_MAX >= 2
        case 2:
            return ((rpc_any_t (*)(rpc_any_t, rpc_any_t))func)(params[0], params[1]);
#endif
#if PARAMS_MAX >= 3
        case 3:
            return ((rpc_any_t (*)(rpc_any_t, rpc_any_t, rpc_any_t))func)(params[0], params[1], params[2]);
#endif
#if PARAMS_MAX >= 4
        case 4:
            return ((rpc_any_t (*)(rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t))func)(params[0], params[1], params[2], params[3]);
#endif
#if PARAMS_MAX >= 5
        case 5:
            return ((rpc_any_t (*)(rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t))func)(params[0], params[1], params[2], params[3], params[4]);
#endif
#if PARAMS_MAX >= 6
        case 6:
            return ((rpc_any_t (*)(rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t))func)(params[0], params[1], params[2], params[3], params[4], params[5]);
#endif
#if PARAMS_MAX >= 7
        case 7:
            return ((rpc_any_t (*)(rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t))func)(params[0], params[1], params[2], params[3], params[4], params[5], params[6]);
#endif
#if PARAMS_MAX >= 8
        case 8:
            return ((rpc_any_t (*)(rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t))func)(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7]);
#endif
#if PARAMS_MAX >= 9
        case 9:
            return ((rpc_any_t (*)(rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t))func)(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8]);
#endif
#if PARAMS_MAX >= 10
        case 10:
            return ((rpc_any_t (*)(rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t, rpc_any_t))func)(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9]);
#endif
        default:
            rpc_log.log_error(TAG, "Max number of parameters exceeded");
            return (rpc_any_t) {
                    .i = -1
                };
    }
}

void rpc_mqtt_call(const Device_t *dev, const char *service, const rpc_any_t *params, const unsigned int params_num) {
    rpc_log.log_info(TAG, "rpc_mqtt_call");
    char *json_str = serialize_service(callback_topic, params, params_num);
    char topic[TOPIC_LEN_MAX];

    snprintf(topic, sizeof(topic), "/%s/%s/%u/%s", dev->place, dev->type, dev->id, service);
    int msg_id = esp_mqtt_client_publish(client, topic, json_str, strlen(json_str), 0, false);
    rpc_log.log_info(TAG, "calling service: %s, data: %s", topic, json_str);
    free(json_str);
    if (msg_id == -1)
        rpc_log.log_error(TAG, "Failed to publish message");
    else
        rpc_log.log_info(TAG, "sent publish successful, msg_id=%d", msg_id);
}

static void rpc_func_wrapper(void *arg) {
    rpc_any_t args[PARAMS_MAX];
    Service_t *service = (Service_t *)arg;
    char* callback = NULL;
    // rpc_any_t* params = NULL;
    unsigned int params_count = 0;

    while (1) {
        xEventGroupWaitBits(SharedData->event_group, service->_wait, pdTRUE, pdTRUE, portMAX_DELAY);
        
        rpc_log.log_info(TAG, "Executing function: %s", service->name);
        if (deserialize_service(service->_input, &callback, args, &params_count) != 0) {
            rpc_log.log_error(TAG, "Failed to deserialize the input JSON");
            continue;
        }

        // for (unsigned int i = 0; i < params_count; i++) {
        //     rpc_log.log_info(TAG, "Param %d: %s", i, params[i].str);
        //     switch (service->input_type[i]) {
        //         case 'i':
        //             args[i].l = atol(params[i].str);
        //             break;
        //         case 'f':
        //             args[i].f = atof(params[i].str);
        //             break;
        //         case 'd':
        //             args[i].d = atof(params[i].str);
        //             break;
        //         case 'c':
        //             args[i].c = params[i].str[0];
        //             break;
        //         case 's':
        //             args[i].str = params[i].str;
        //             break;
        //         default:
        //             rpc_log.log_error(TAG, "Invalid parameter type: %c", service->input_type[i]);
        //             continue;
        //     }
        rpc_log.log_info(TAG, "Param %d", params_count);
        rpc_any_t res = exec(service->func, strlen(service->input_type), args);

        cJSON* json = rpc_any_to_json(res);
        char* json_str = cJSON_Print(json);
        if (res.i == -1)
            rpc_log.log_error(TAG, "Failed to execute function: %s", service->name);
        else {
            int msg_id = esp_mqtt_client_publish(client, callback, json_str, strlen(json_str), 0, false);
            rpc_log.log_info(TAG, "response topic: %s, data: %s", callback, json_str);
            if (msg_id == -1)
                rpc_log.log_error(TAG, "Failed to publish message");
            else
                rpc_log.log_info(TAG, "sent publish successful, msg_id=%d", msg_id);
        }
        cJSON_Delete(service->_input);
        free(callback);
        callback = NULL;
    }
}

static void publish_device_info(Device_t* device, esp_mqtt_client_handle_t client) {
    char* json_str = serialize_device(device);
    int msg_id = esp_mqtt_client_enqueue(client, "/topics/register", json_str, 0, 1, 0, true);
    if (msg_id == -1) {
        rpc_log.log_error(TAG, "Failed to enqueue message");
        free(json_str);
        return;
    }
    rpc_log.log_info(TAG, "sent publish successful, msg = %s, msg_id=%d", json_str, msg_id);
    free(json_str);
}

static void rpc_mqtt_register(void *arg) {
    static Device_t *device;
    static char topic[TOPIC_LEN_MAX];
    while (1) {
        xQueueReceive(SharedData->device_queue, &device, portMAX_DELAY);
        publish_device_info(device, client);
        for (unsigned int i = 0; i < device->services_num; i++) {
            snprintf(topic, sizeof(topic), "/%s/%s/%u/%s", device->place, device->type, device->id, device->services[i].name);
            int sub_id = esp_mqtt_client_subscribe(client, topic, 0);
            if (sub_id == -1)
                rpc_log.log_error(TAG, "Failed to subscribe to topic: %s", topic);
            else {
                rpc_log.log_info(TAG, "Subscribed to topic: %s", topic);
                xTaskCreate(rpc_func_wrapper, device->services[i].name, 4096, &device->services[i], 5, NULL);
            }
        }
    }
}

void rpc_search_task(void *arg) {
    rpc_log.log_info(TAG, "rpc_search_task started");
    static cJSON* json = NULL;
    static char* topic_service = NULL;
    while (1) {
        xQueueReceive(SearchQueue, &json, portMAX_DELAY);
        topic_service = cJSON_GetObjectItem(json, "topic")->valuestring;
        for (DeviceList_t *device = SharedData->device_list; device != NULL; device = device->next) {
            for (unsigned int i = 0; i < device->device->services_num; i++)
                if (strcmp(device->device->services[i].name, topic_service) == 0) {
                    device->device->services[i]._input = json;
                    xEventGroupSetBits(SharedData->event_group, device->device->services[i]._wait);
                    break;
                }
        }
    }
}

static char* strrchr_len(const char* s, int c, size_t len) {
    const char* end = s + len;
    while (end != s) {
        end--;
        if (*end == (char)c) {
            return (char*)end;
        }
    }
    return NULL;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    rpc_log.log_info(TAG, "Event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    static BaseType_t result = pdFAIL;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            rpc_log.log_info(TAG, "MQTT_EVENT_CONNECTED");
            int sub_id = esp_mqtt_client_subscribe(client, callback_topic, 0);
            if (sub_id == -1)
                rpc_log.log_error(TAG, "Failed to subscribe to topic: %s", callback_topic);
            if (result != pdPASS)
                result = xTaskCreatePinnedToCore(rpc_mqtt_register, "mqtt_register", 4096, NULL, 5, NULL, 0);
            else
                for (DeviceList_t *device = SharedData->device_list; device != NULL; device = device->next)
                    publish_device_info(device->device, client);
            break;

        case MQTT_EVENT_DISCONNECTED:
            rpc_log.log_info(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            rpc_log.log_info(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            rpc_log.log_info(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            rpc_log.log_info(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            rpc_log.log_info(TAG, "TOPIC=%.*s, DATA=%.*s", event->topic_len, event->topic, event->data_len, event->data);
            cJSON* json = cJSON_Parse(event->data);
            if (json == NULL) {
                rpc_log.log_error(TAG, "Failed to parse input data");
                break;
            }

            if (strncmp(event->topic, callback_topic, event->topic_len) == 0) {
                rpc_any_t res = json_to_rpc_any(json);
                xQueueSend(SharedData->response_queue, &res, 0);
                cJSON_Delete(json);
            } else {
                char* topic_service = strrchr_len(event->topic, '/', event->topic_len);
                if (topic_service != NULL)
                    topic_service++;
                else
                    break;

                size_t topic_service_len = event->topic + event->topic_len - topic_service;

                char topic_service_buf[topic_service_len + 1];
                snprintf(topic_service_buf, sizeof(topic_service_buf), "%s", topic_service);

                rpc_log.log_info(TAG, "Service: %s", topic_service_buf);
                cJSON* json_topic = cJSON_CreateString(topic_service_buf);
                cJSON_AddItemToObject(json, "topic", json_topic);

                xQueueSend(SearchQueue, &json, 0);
            }
            break;

        case MQTT_EVENT_ERROR:
            rpc_log.log_info(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                rpc_log.log_error(TAG, "reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                rpc_log.log_error(TAG, "reported from tls stack", event->error_handle->esp_tls_stack_err);
                rpc_log.log_error(TAG, "captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                rpc_log.log_info(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
        default:
            rpc_log.log_info(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

void rpc_mqtt_task(void *arg) {
    rpc_log.log_info(TAG, "rpc_mqtt_task started");
    SharedData = (SharedData_t *)arg;
    const esp_mqtt_client_config_t config = {
        .broker.address.uri = SharedData->uri,
    };
    client = esp_mqtt_client_init(&config);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    rpc_log.log_info(TAG, "Connecting to MQTT broker");
    SearchQueue = xQueueCreate(10, sizeof(cJSON *));
    xTaskCreatePinnedToCore(rpc_search_task, "rpc_search", 4096, SearchQueue, 5, NULL, 0);
    vTaskDelete(NULL);
}