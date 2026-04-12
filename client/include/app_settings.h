#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#include "app_config.h"
#include "app_conversations.h"
#include "app_input.h"
#include "app_ui.h"

typedef struct AppSettingsContext {
    AppConversationState* conversation_state;
} AppSettingsContext;

bool hermes_app_settings_handle_input(
    u32 kDown,
    HermesAppConfig* config,
    const AppSettingsContext* context,
    AppScreen* screen,
    SettingsField* selected_field,
    bool* settings_dirty,
    char* status_line,
    size_t status_line_size
);
