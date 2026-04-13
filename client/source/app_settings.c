#include "app_settings.h"

#include <stdio.h>

bool hermes_app_settings_handle_input(
    u32 kDown,
    HermesAppConfig* config,
    const AppSettingsContext* context,
    AppScreen* screen,
    SettingsField* selected_field,
    bool* settings_dirty,
    char* status_line,
    size_t status_line_size
)
{
    if (config == NULL || screen == NULL || selected_field == NULL || settings_dirty == NULL ||
        status_line == NULL || status_line_size == 0) {
        return false;
    }

    if ((kDown & KEY_B) != 0) {
        *screen = APP_SCREEN_HOME;
        snprintf(status_line, status_line_size, *settings_dirty ? "Setup closed with unsaved changes." : "Returned home.");
        return true;
    }

    if ((kDown & (KEY_UP | KEY_CPAD_UP)) != 0) {
        if (*selected_field == 0)
            *selected_field = SETTINGS_FIELD_COUNT - 1;
        else
            *selected_field = (SettingsField)(*selected_field - 1);
        snprintf(status_line, status_line_size, "Selected %s.", settings_field_label(*selected_field));
        return true;
    }

    if ((kDown & (KEY_DOWN | KEY_CPAD_DOWN)) != 0) {
        *selected_field = (SettingsField)((*selected_field + 1) % SETTINGS_FIELD_COUNT);
        snprintf(status_line, status_line_size, "Selected %s.", settings_field_label(*selected_field));
        return true;
    }

    if ((kDown & KEY_Y) != 0) {
        hermes_app_config_set_defaults(config);
        if (context != NULL && context->conversation_state != NULL)
            hermes_app_conversations_refresh_selection_from_active(context->conversation_state, config);
        *settings_dirty = true;
        snprintf(status_line, status_line_size, "Defaults restored. Save setup to keep them.");
        return true;
    }

    if ((kDown & KEY_A) != 0) {
        if (edit_selected_setting(config, *selected_field, status_line, status_line_size))
            *settings_dirty = true;
        return true;
    }

    if ((kDown & KEY_X) != 0) {
        if (hermes_app_config_save(config)) {
            *settings_dirty = false;
            snprintf(status_line, status_line_size, "Setup saved to SD card.");
        } else {
            snprintf(status_line, status_line_size, "Could not save setup to SD card.");
        }
        return true;
    }

    return false;
}
