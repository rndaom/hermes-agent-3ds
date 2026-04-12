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

typedef void (*AppHomeRenderFn)(
    AppScreen screen,
    const HermesAppConfig* config,
    SettingsField selected_field,
    bool settings_dirty,
    const BridgeHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t reply_page,
    const char* status_line,
    Result last_rc
);

typedef struct AppHomeContext {
    AppConversationState* conversation_state;
    PrintConsole* top_console;
    PrintConsole* bottom_console;
    SettingsField selected_field;
    bool settings_dirty;
    BridgeHealthResult* health_result;
    BridgeChatResult* chat_result;
    char* last_message;
    size_t last_message_size;
    size_t* reply_page;
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
