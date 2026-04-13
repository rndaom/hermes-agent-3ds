#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#include "app_config.h"
#include "app_input.h"
#include "app_ui.h"
#include "bridge_chat.h"
#include "bridge_health.h"
#include "bridge_v2.h"

/* Uses BridgeV2MessageResult and BridgeV2EventPollResult for native V2 roundtrips. */
typedef void (*AppRequestRenderFn)(
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

typedef struct AppRequestUiContext {
    SettingsField selected_field;
    bool settings_dirty;
    const GatewayHealthResult* health_result;
    size_t command_selection;
    AppRequestRenderFn render;
} AppRequestUiContext;

void hermes_app_requests_handle_text(
    const HermesAppConfig* config,
    bool network_ready,
    const AppRequestUiContext* ui,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* history_scroll,
    char* status_line,
    size_t status_line_size,
    Result* request_rc
);

void hermes_app_requests_handle_voice(
    const HermesAppConfig* config,
    bool network_ready,
    const AppRequestUiContext* ui,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* history_scroll,
    char* status_line,
    size_t status_line_size,
    Result* request_rc
);
