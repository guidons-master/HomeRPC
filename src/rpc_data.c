#include <cJSON.h>
#include "HomeRPC.h"
#include "rpc_data.h"
#include <string.h>
#include "rpc_log.h"
#include <stdlib.h>

char* serialize_device(const Device_t* device) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "place", device->place);
    cJSON_AddStringToObject(json, "type", device->type);
    cJSON_AddNumberToObject(json, "id", device->id);
    cJSON* services = cJSON_CreateArray();
    for (unsigned int i = 0; i < device->services_num; i++) {
        cJSON* service = cJSON_CreateObject();
        cJSON_AddStringToObject(service, "input_type", device->services[i].input_type);
        cJSON_AddStringToObject(service, "output_type", &device->services[i].output_type);
        cJSON_AddStringToObject(service, "name", device->services[i].name);
        cJSON_AddStringToObject(service, "desc", device->services[i].desc);
        cJSON_AddItemToArray(services, service);
    }
    cJSON_AddItemToObject(json, "services", services);
    cJSON_AddNumberToObject(json, "services_num", device->services_num);
    char* json_string = cJSON_Print(json);
    cJSON_Delete(json);
    return json_string;
}

// need to free the returned buffer!
static char* serializeRpcAny(const rpc_any_t* data) {
    char* buffer = (char*)malloc(sizeof(rpc_any_t) * 2 + 1);
    if (buffer == NULL)
        return NULL;

    for (size_t i = 0; i < sizeof(rpc_any_t); i++)
        sprintf(buffer + i * 2, "%02x", ((unsigned char*)data)[i]);

    buffer[sizeof(rpc_any_t) * 2] = '\0';
    return buffer;
}

char* serialize_service(const char* callback, const rpc_any_t* params, unsigned int params_num) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "callback", callback);
    cJSON* params_array = cJSON_CreateArray();
    char *param = NULL;
    for (unsigned int i = 0; i < params_num; i++) {
        param = serializeRpcAny(&params[i]);
        if (param == NULL) { 
            cJSON_Delete(json);
            return NULL;
        }
        cJSON_AddItemToArray(params_array, cJSON_CreateString(param));
        free(param); 
    }
    cJSON_AddItemToObject(json, "params", params_array);
    char* json_string = cJSON_Print(json);
    cJSON_Delete(json);
    return json_string;
}

static rpc_any_t deserializeRpcAny(const char* buffer) {
    rpc_any_t data;
    for (size_t i = 0; i < sizeof(rpc_any_t); i++) {
        sscanf(buffer + i * 2, "%02x", (unsigned int*)&((unsigned char*)&data)[i]);
    }
    return data;
}

int deserialize_service(cJSON *json, char** callback, rpc_any_t *params, unsigned int* params_num) {
    if (json == NULL) {
        return -1;
    }

    cJSON* callback_json = cJSON_GetObjectItem(json, "callback");
    if (callback_json == NULL) {
        return -1;
    }
    rpc_log.log_info("deserialize_service", "callback: %s", callback_json->valuestring);
    *callback = strdup(callback_json->valuestring);

    cJSON* params_array = cJSON_GetObjectItem(json, "params");
    if (params_array == NULL) {
        free(*callback);
        return -1;
    }
    rpc_log.log_info("deserialize_service", "params: %s", cJSON_Print(params_array));

    *params_num = cJSON_GetArraySize(params_array);
    if (*params_num > PARAMS_MAX) {
        free(*callback);
        return -1;
    }

    rpc_log.log_info("deserialize_service", "params_num: %d", *params_num);
    for (unsigned int i = 0; i < *params_num; i++) {
        cJSON* param_json = cJSON_GetArrayItem(params_array, i);
        if (param_json == NULL) {
            free(*callback);
            return -1;
        }
        params[i] = deserializeRpcAny(param_json->valuestring);
        rpc_log.log_info("deserialize_service", "params[%d]", i);
    }

    return 0;
}

cJSON* rpc_any_to_json(rpc_any_t data) {
    cJSON* json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    const char* str = serializeRpcAny(&data);
    if (str == NULL) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON* result = cJSON_AddStringToObject(json, "result", str);
    free((void*)str);
    if (result == NULL) {
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

rpc_any_t json_to_rpc_any(cJSON* json) {
    cJSON* result = cJSON_GetObjectItem(json, "result");
    if (result == NULL || strlen(result->valuestring) < sizeof(rpc_any_t) * 2) {
        return (rpc_any_t) { .i = -1 };
    }

    return deserializeRpcAny(result->valuestring);
}