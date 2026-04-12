#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#include "app_config.h"
#include "app_input.h"
#include "app_ui.h"
#include "bridge_chat.h"
#include "bridge_v2.h"

typedef struct AppConversationState {
    BridgeV2ConversationListResult list;
    size_t selection;
} AppConversationState;

void hermes_app_conversations_init(AppConversationState* state);
void hermes_app_conversations_refresh_selection_from_active(AppConversationState* state, const HermesAppConfig* config);
void hermes_app_conversations_open_picker(
    AppConversationState* state,
    const HermesAppConfig* config,
    AppScreen* screen,
    char* status_line,
    size_t status_line_size
);
bool hermes_app_conversations_handle_picker_input(
    AppConversationState* state,
    HermesAppConfig* config,
    bool network_ready,
    u32 kDown,
    AppScreen* screen,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* reply_page,
    char* status_line,
    size_t status_line_size,
    Result* request_rc
);
