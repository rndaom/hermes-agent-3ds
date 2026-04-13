#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#include "app_config.h"
#include "app_input.h"
#include "bridge_chat.h"
#include "bridge_health.h"
#include "bridge_v2.h"

typedef enum AppScreen {
    APP_SCREEN_HOME = 0,
    APP_SCREEN_SETTINGS = 1,
    APP_SCREEN_CONVERSATIONS = 2,
} AppScreen;

size_t hermes_app_ui_reply_page_count(const char* reply_text);

bool hermes_app_ui_init(void);
void hermes_app_ui_exit(void);

void hermes_app_ui_render(
    AppScreen screen,
    const HermesAppConfig* config,
    SettingsField selected_field,
    bool settings_dirty,
    const BridgeHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t reply_page,
    size_t command_selection,
    const char* status_line,
    Result last_rc,
    const BridgeV2ConversationListResult* conversation_list,
    size_t conversation_selection
);

void hermes_app_ui_render_approval_prompt(const char* request_id);
void hermes_app_ui_render_voice_recording(unsigned long tenths, size_t pcm_size, const char* status_line, bool waiting_for_up_release);
