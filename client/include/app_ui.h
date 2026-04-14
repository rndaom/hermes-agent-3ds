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

typedef enum AppUiMessageAuthor {
    APP_UI_MESSAGE_USER = 0,
    APP_UI_MESSAGE_HERMES = 1,
} AppUiMessageAuthor;

size_t hermes_app_ui_home_history_max_scroll(const char* status_line);
void hermes_app_ui_home_history_reset(void);
void hermes_app_ui_home_history_push(AppUiMessageAuthor author, const char* text);
void hermes_app_ui_home_history_upsert(AppUiMessageAuthor author, const char* text);
void hermes_app_ui_home_history_push_image(AppUiMessageAuthor author, const char* text, const u8* rgba8_data, u16 width, u16 height);
void hermes_app_ui_home_history_upsert_image(AppUiMessageAuthor author, const char* text, const u8* rgba8_data, u16 width, u16 height);

bool hermes_app_ui_init(void);
void hermes_app_ui_exit(void);

void hermes_app_ui_render(
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
    Result last_rc,
    const BridgeV2ConversationListResult* conversation_list,
    size_t conversation_selection
);

void hermes_app_ui_render_interaction_prompt(
    const HermesAppConfig* config,
    const char* header,
    const char* title,
    const char* body,
    const BridgeV2InteractionOption* options,
    size_t option_count,
    size_t selection,
    const char* hint_line
);

void hermes_app_ui_render_approval_prompt(const HermesAppConfig* config, const char* request_id);
void hermes_app_ui_render_voice_recording(
    const HermesAppConfig* config,
    unsigned long tenths,
    size_t pcm_size,
    const char* status_line,
    bool waiting_for_a_release
);
void hermes_app_ui_render_picture_capture(
    const HermesAppConfig* config,
    const char* status_line,
    bool waiting_for_a_release
);
