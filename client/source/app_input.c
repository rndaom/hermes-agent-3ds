#include "app_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_bounded_string(char* dest, size_t dest_size, const char* src)
{
    size_t length;

    if (dest == NULL || dest_size == 0)
        return;

    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    length = strlen(src);
    if (length >= dest_size)
        length = dest_size - 1;

    memcpy(dest, src, length);
    dest[length] = '\0';
}

const char* settings_field_label(SettingsField field)
{
    switch (field) {
        case SETTINGS_FIELD_HOST:
            return "Host";
        case SETTINGS_FIELD_PORT:
            return "Port";
        case SETTINGS_FIELD_TOKEN:
            return "Token";
        case SETTINGS_FIELD_DEVICE_ID:
            return "Device ID";
        default:
            return "Unknown";
    }
}

static bool prompt_text_input(SwkbdType type, const char* hint, const char* initial_text, char* out_text, size_t out_size, bool password_mode)
{
    SwkbdState swkbd;
    SwkbdButton button;

    if (out_text == NULL || out_size == 0)
        return false;

    out_text[0] = '\0';
    swkbdInit(&swkbd, type, 1, (int)out_size - 1);
    if (initial_text != NULL && initial_text[0] != '\0')
        swkbdSetInitialText(&swkbd, initial_text);
    if (hint != NULL)
        swkbdSetHintText(&swkbd, hint);

    if (password_mode)
        swkbdSetPasswordMode(&swkbd, SWKBD_PASSWORD_HIDE_DELAY);

    if (type == SWKBD_TYPE_NUMPAD) {
        swkbdSetValidation(&swkbd, SWKBD_ANYTHING, 0, 0);
        swkbdSetFeatures(&swkbd, SWKBD_FIXED_WIDTH);
    } else {
        swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    }

    button = swkbdInputText(&swkbd, out_text, out_size);
    return button == SWKBD_BUTTON_CONFIRM;
}

bool edit_selected_setting(HermesAppConfig* config, SettingsField field, char* status_line, size_t status_line_size)
{
    char edit_buffer[HERMES_APP_CONFIG_TOKEN_MAX];

    if (config == NULL || status_line == NULL || status_line_size == 0)
        return false;

    switch (field) {
        case SETTINGS_FIELD_HOST:
            if (!prompt_text_input(SWKBD_TYPE_WESTERN, "Bridge host or IPv4", config->host, edit_buffer, sizeof(config->host), false)) {
                snprintf(status_line, status_line_size, "%s edit canceled.", settings_field_label(field));
                return false;
            }

            copy_bounded_string(config->host, sizeof(config->host), edit_buffer);
            snprintf(status_line, status_line_size, "Host updated.");
            return true;

        case SETTINGS_FIELD_PORT: {
            char port_text[8];
            char* end_ptr = NULL;
            unsigned long parsed_port;

            snprintf(port_text, sizeof(port_text), "%u", (unsigned int)config->port);
            if (!prompt_text_input(SWKBD_TYPE_NUMPAD, "Bridge port", port_text, edit_buffer, sizeof(port_text), false)) {
                snprintf(status_line, status_line_size, "%s edit canceled.", settings_field_label(field));
                return false;
            }

            parsed_port = strtoul(edit_buffer, &end_ptr, 10);
            if (end_ptr == edit_buffer || *end_ptr != '\0' || parsed_port == 0 || parsed_port > 65535) {
                snprintf(status_line, status_line_size, "Port must be between 1 and 65535.");
                return false;
            }

            config->port = (u16)parsed_port;
            snprintf(status_line, status_line_size, "Port updated.");
            return true;
        }

        case SETTINGS_FIELD_TOKEN:
            if (!prompt_text_input(SWKBD_TYPE_WESTERN, "Bridge token (optional for now)", config->token, edit_buffer, sizeof(config->token), true)) {
                snprintf(status_line, status_line_size, "%s edit canceled.", settings_field_label(field));
                return false;
            }

            copy_bounded_string(config->token, sizeof(config->token), edit_buffer);
            snprintf(status_line, status_line_size, "Token updated.");
            return true;

        case SETTINGS_FIELD_DEVICE_ID:
            if (!prompt_text_input(SWKBD_TYPE_WESTERN, "Stable device ID for native v2", config->device_id, edit_buffer, sizeof(config->device_id), false)) {
                snprintf(status_line, status_line_size, "%s edit canceled.", settings_field_label(field));
                return false;
            }

            copy_bounded_string(config->device_id, sizeof(config->device_id), edit_buffer);
            snprintf(status_line, status_line_size, "Device ID updated.");
            return true;

        default:
            snprintf(status_line, status_line_size, "Unknown settings field.");
            return false;
    }
}

bool prompt_message_input(char* out_message, size_t out_size)
{
    return prompt_text_input(
        SWKBD_TYPE_WESTERN,
        "Write a note for Hermes",
        "",
        out_message,
        out_size,
        false
    );
}

bool prompt_conversation_input(const char* initial_text, char* out_conversation_id, size_t out_size)
{
    return prompt_text_input(
        SWKBD_TYPE_WESTERN,
        "Room ID: letters, numbers, - _ .",
        initial_text,
        out_conversation_id,
        out_size,
        false
    );
}
