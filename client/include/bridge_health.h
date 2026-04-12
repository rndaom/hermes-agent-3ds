#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef DEFAULT_BRIDGE_HEALTH_URL
#define DEFAULT_BRIDGE_HEALTH_URL "http://10.75.76.156:8787/api/v2/health"
#endif

#define BRIDGE_HEALTH_TEXT_MAX 96
#define BRIDGE_HEALTH_ERROR_MAX 128

typedef struct BridgeHealthResult {
    bool success;
    u32 http_status;
    int socket_errno;
    char socket_stage[32];
    char service[BRIDGE_HEALTH_TEXT_MAX];
    char version[BRIDGE_HEALTH_TEXT_MAX];
    char error[BRIDGE_HEALTH_ERROR_MAX];
} BridgeHealthResult;

void bridge_health_result_reset(BridgeHealthResult* result);
Result bridge_health_network_init(void);
void bridge_health_network_exit(void);
Result bridge_health_check_run(const char* url, BridgeHealthResult* result);
