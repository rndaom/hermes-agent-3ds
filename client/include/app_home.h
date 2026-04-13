#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#include "app_config.h"
#include "app_conversations.h"
#include "app_input.h"
#include "app_ui.h"
#include "bridge_chat.h"
#include "bridge_health.h"

typedef enum HomeCommand {
    HOME_COMMAND_NONE = -1,
    HOME_COMMAND_TEXT = 0,
    HOME_COMMAND_CHECK = 1,
    HOME_COMMAND_SESSIONS = 2,
    HOME_COMMAND_SETTINGS = 3,
    HOME_COMMAND_AUDIO = 4,
} HomeCommand;

typedef void (*AppHomeRenderFn)(
    AppScreen screen,
    const HermesAppConfig* config,
    SettingsField selected_field,
    bool settings_dirty,
    const GatewayHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t history_scroll,
    size_t command_selection,
    const char* status_line,
    Result last_rc
);

typedef struct AppHomeContext {
    AppConversationState* conversation_state;
    SettingsField selected_field;
    bool settings_dirty;
    GatewayHealthResult* health_result;
    BridgeChatResult* chat_result;
    char* last_message;
    size_t last_message_size;
    size_t* history_scroll;
    HomeCommand* command_selection;
    char* status_line;
    size_t status_line_size;
    Result* request_rc;
    AppHomeRenderFn render;
} AppHomeContext;

bool hermes_app_home_handle_input(
    u32 kDown,
    HermesAppConfig* config,
    bool network_ready,
    AppScreen* screen,
    AppHomeContext* context
);
