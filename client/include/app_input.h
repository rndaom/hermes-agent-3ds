#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#include "app_config.h"

typedef enum SettingsField {
    SETTINGS_FIELD_HOST = 0,
    SETTINGS_FIELD_PORT = 1,
    SETTINGS_FIELD_TOKEN = 2,
    SETTINGS_FIELD_DEVICE_ID = 3,
    SETTINGS_FIELD_COUNT = 4,
} SettingsField;

const char* settings_field_label(SettingsField field);
bool edit_selected_setting(HermesAppConfig* config, SettingsField field, char* status_line, size_t status_line_size);
bool prompt_message_input(char* out_message, size_t out_size);
bool prompt_conversation_input(const char* initial_text, char* out_conversation_id, size_t out_size);
