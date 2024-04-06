#include <cJSON.h>
#include "HomeRPC.h"
#include "rpc_data.h"
#include <string.h>
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
    if (params_num == 0) {
        char* json_string = cJSON_Print(json);
        cJSON_Delete(json);
        return json_string;
    }
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
    *callback = strdup(callback_json->valuestring);

    cJSON* params_array = cJSON_GetObjectItem(json, "params");
    if (params_array == NULL) {
        *params_num = 0;
        return 0;
    }

    *params_num = cJSON_GetArraySize(params_array);
    if (*params_num > CONFIG_PARAMS_MAX) {
        free(*callback);
        return -1;
    }

    for (unsigned int i = 0; i < *params_num; i++) {
        cJSON* param_json = cJSON_GetArrayItem(params_array, i);
        if (param_json == NULL) {
            free(*callback);
            return -1;
        }
        params[i] = deserializeRpcAny(param_json->valuestring);
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