#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "HomeRPC.h"

extern char callback_topic[CONFIG_TOPIC_LEN_MAX];
void rpc_mqtt_call(const Device_t *, const char *, const rpc_any_t*, const unsigned int);
void rpc_mqtt_task(void *);

#ifdef __cplusplus
}
#endif