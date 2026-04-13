#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef DEFAULT_GATEWAY_HEALTH_URL
#define DEFAULT_GATEWAY_HEALTH_URL "http://10.75.76.156:8787/api/v2/health"
#endif

#define BRIDGE_HEALTH_TEXT_MAX 96
#define BRIDGE_HEALTH_ERROR_MAX 128

typedef struct GatewayHealthResult {
    bool success;
    u32 http_status;
    int socket_errno;
    char socket_stage[32];
    char service[BRIDGE_HEALTH_TEXT_MAX];
    char version[BRIDGE_HEALTH_TEXT_MAX];
    char model_name[BRIDGE_HEALTH_TEXT_MAX];
    u32 context_length;
    u32 context_tokens;
    u32 context_percent;
    char error[BRIDGE_HEALTH_ERROR_MAX];
} GatewayHealthResult;

void gateway_health_result_reset(GatewayHealthResult* result);
Result gateway_health_network_init(void);
void gateway_health_network_exit(void);
Result gateway_health_check_run(const char* url, GatewayHealthResult* result);
