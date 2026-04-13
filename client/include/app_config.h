#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#include "app_theme.h"

#define HERMES_APP_CONFIG_DIR "sdmc:/3ds/hermes-agent-3ds"
#define HERMES_APP_CONFIG_PATH "sdmc:/3ds/hermes-agent-3ds/config.ini"
#define DEFAULT_GATEWAY_HOST "10.75.76.156"
#define DEFAULT_GATEWAY_PORT 8787
#define DEFAULT_CONVERSATION_ID "main"
#define HERMES_APP_CONFIG_HOST_MAX 64
#define HERMES_APP_CONFIG_TOKEN_MAX 128
#define HERMES_APP_HEALTH_URL_MAX 1024
#define HERMES_APP_CONVERSATIONS_URL_MAX 160
#define HERMES_APP_MESSAGES_URL_MAX 160
#define HERMES_APP_EVENTS_URL_MAX 160
#define HERMES_APP_VOICE_URL_MAX 160
#define HERMES_APP_INTERACTION_URL_MAX 192
#define HERMES_APP_DEVICE_ID_MAX 64
#define HERMES_APP_CONVERSATION_ID_MAX 64
#define HERMES_APP_RECENT_CONVERSATIONS_MAX 8

typedef struct HermesAppConfig {
    char host[HERMES_APP_CONFIG_HOST_MAX];
    u16 port;
    char token[HERMES_APP_CONFIG_TOKEN_MAX];
    char device_id[HERMES_APP_DEVICE_ID_MAX];
    char active_conversation_id[HERMES_APP_CONVERSATION_ID_MAX];
    size_t recent_conversation_count;
    char recent_conversations[HERMES_APP_RECENT_CONVERSATIONS_MAX][HERMES_APP_CONVERSATION_ID_MAX];
    PictochatThemeColor theme_color;
    bool dark_mode;
} HermesAppConfig;

typedef enum HermesAppConfigLoadStatus {
    HERMES_APP_CONFIG_LOAD_OK = 0,
    HERMES_APP_CONFIG_LOAD_DEFAULTED = 1,
    HERMES_APP_CONFIG_LOAD_ERROR = 2,
} HermesAppConfigLoadStatus;

void hermes_app_config_set_defaults(HermesAppConfig* config);
HermesAppConfigLoadStatus hermes_app_config_load(HermesAppConfig* config);
bool hermes_app_config_save(const HermesAppConfig* config);
bool hermes_app_config_is_valid_conversation_id(const char* conversation_id);
bool hermes_app_config_set_active_conversation(HermesAppConfig* config, const char* conversation_id);
void hermes_app_config_touch_recent_conversation(HermesAppConfig* config, const char* conversation_id);
bool hermes_app_config_build_health_url(const HermesAppConfig* config, char* out_url, size_t out_size);
bool hermes_app_config_build_conversations_url(const HermesAppConfig* config, char* out_url, size_t out_size);
bool hermes_app_config_build_messages_url(const HermesAppConfig* config, char* out_url, size_t out_size);
bool hermes_app_config_build_events_url(const HermesAppConfig* config, char* out_url, size_t out_size);
bool hermes_app_config_build_voice_url(const HermesAppConfig* config, char* out_url, size_t out_size);
bool hermes_app_config_build_interaction_url(const HermesAppConfig* config, const char* request_id, char* out_url, size_t out_size);
