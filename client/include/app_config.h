#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#define HERMES_APP_CONFIG_DIR "sdmc:/3ds/hermes-agent-3ds"
#define HERMES_APP_CONFIG_PATH "sdmc:/3ds/hermes-agent-3ds/config.ini"
#define DEFAULT_BRIDGE_HOST "10.75.76.156"
#define DEFAULT_BRIDGE_PORT 8787
#define HERMES_APP_CONFIG_HOST_MAX 64
#define HERMES_APP_CONFIG_TOKEN_MAX 128
#define HERMES_APP_HEALTH_URL_MAX 160
#define HERMES_APP_CHAT_URL_MAX 160

typedef struct HermesAppConfig {
    char host[HERMES_APP_CONFIG_HOST_MAX];
    u16 port;
    char token[HERMES_APP_CONFIG_TOKEN_MAX];
} HermesAppConfig;

typedef enum HermesAppConfigLoadStatus {
    HERMES_APP_CONFIG_LOAD_OK = 0,
    HERMES_APP_CONFIG_LOAD_DEFAULTED = 1,
    HERMES_APP_CONFIG_LOAD_ERROR = 2,
} HermesAppConfigLoadStatus;

void hermes_app_config_set_defaults(HermesAppConfig* config);
HermesAppConfigLoadStatus hermes_app_config_load(HermesAppConfig* config);
bool hermes_app_config_save(const HermesAppConfig* config);
bool hermes_app_config_build_health_url(const HermesAppConfig* config, char* out_url, size_t out_size);
bool hermes_app_config_build_chat_url(const HermesAppConfig* config, char* out_url, size_t out_size);
