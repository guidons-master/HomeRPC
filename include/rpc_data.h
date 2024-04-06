#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "HomeRPC.h"
#include "cJSON.h"

char* serialize_device(const Device_t*);
char* serialize_service(const char*, const rpc_any_t*, unsigned int);
int deserialize_service(cJSON*, char**, rpc_any_t*, unsigned int*);
cJSON* rpc_any_to_json(rpc_any_t);
rpc_any_t json_to_rpc_any(cJSON *);

#ifdef __cplusplus
}
#endif