#include "app_home.h"

#include <stdio.h>

#include "app_requests.h"

static void render_screen(AppScreen screen, const HermesAppConfig* config, const AppHomeContext* context)
{
    if (context == NULL || context->render == NULL || context->reply_page == NULL ||
        context->status_line == NULL || context->request_rc == NULL) {
        return;
    }

    context->render(
        screen,
        config,
        context->selected_field,
        context->settings_dirty,
        context->health_result,
        context->chat_result,
        context->last_message,
        *context->reply_page,
        context->status_line,
        *context->request_rc
    );
}

static void render_home_screen(const HermesAppConfig* config, const AppHomeContext* context)
{
    render_screen(APP_SCREEN_HOME, config, context);
}

static void handle_home_health_check(const HermesAppConfig* config, bool network_ready, AppHomeContext* context)
{
    char health_url[HERMES_APP_HEALTH_URL_MAX];

    if (context == NULL || context->health_result == NULL || context->status_line == NULL ||
        context->request_rc == NULL || context->reply_page == NULL) {
        return;
    }

    bridge_health_result_reset(context->health_result);
    *context->request_rc = 0;
    *context->reply_page = 0;

    if (!network_ready) {
        snprintf(context->status_line, context->status_line_size, "Networking services failed to start.");
        render_home_screen(config, context);
        return;
    }

    if (!hermes_app_config_build_health_url(config, health_url, sizeof(health_url))) {
        snprintf(context->status_line, context->status_line_size, "Local config is incomplete.");
        render_home_screen(config, context);
        return;
    }

    snprintf(context->status_line, context->status_line_size, "Checking Hermes gateway...");
    render_home_screen(config, context);
    gspWaitForVBlank();

    *context->request_rc = bridge_health_check_run(health_url, context->health_result);
    if (R_SUCCEEDED(*context->request_rc) && context->health_result->success)
        snprintf(context->status_line, context->status_line_size, "Hermes gateway OK.");
    else if (context->health_result->error[0] == '\0')
        snprintf(context->status_line, context->status_line_size, "Gateway check failed: 0x%08lX", (unsigned long)*context->request_rc);
    else
        snprintf(context->status_line, context->status_line_size, "%s", context->health_result->error);

    render_home_screen(config, context);
}

bool hermes_app_home_handle_input(
    u32 kDown,
    HermesAppConfig* config,
    bool network_ready,
    AppScreen* screen,
    AppHomeContext* context
)
{
    AppRequestUiContext request_ui;

    if (config == NULL || screen == NULL || context == NULL || context->conversation_state == NULL ||
        context->chat_result == NULL || context->last_message == NULL || context->reply_page == NULL ||
        context->status_line == NULL || context->request_rc == NULL) {
        return false;
    }

    request_ui.top_console = context->top_console;
    request_ui.bottom_console = context->bottom_console;
    request_ui.selected_field = context->selected_field;
    request_ui.settings_dirty = context->settings_dirty;
    request_ui.health_result = context->health_result;
    request_ui.render = context->render;

    if ((kDown & KEY_Y) != 0) {
        bridge_health_result_reset(context->health_result);
        bridge_chat_result_reset(context->chat_result);
        context->last_message[0] = '\0';
        *context->reply_page = 0;
        *context->request_rc = 0;
        snprintf(context->status_line, context->status_line_size, "Status cleared. Ready to test again.");
        render_home_screen(config, context);
        return true;
    }

    if ((kDown & KEY_X) != 0) {
        *screen = APP_SCREEN_SETTINGS;
        snprintf(context->status_line, context->status_line_size, "Settings opened.");
        render_screen(*screen, config, context);
        return true;
    }

    if ((kDown & KEY_SELECT) != 0) {
        hermes_app_conversations_open_picker(context->conversation_state, config, screen, context->status_line, context->status_line_size);
        render_screen(*screen, config, context);
        return true;
    }

    if ((kDown & KEY_L) != 0 && context->chat_result->success && *context->reply_page > 0) {
        (*context->reply_page)--;
        render_home_screen(config, context);
        return true;
    }

    if ((kDown & KEY_R) != 0 && context->chat_result->success) {
        size_t page_count = hermes_app_ui_reply_page_count(context->chat_result->reply);
        if (*context->reply_page + 1 < page_count)
            (*context->reply_page)++;
        render_home_screen(config, context);
        return true;
    }

    if ((kDown & KEY_A) != 0) {
        handle_home_health_check(config, network_ready, context);
        return true;
    }

    if ((kDown & KEY_B) != 0) {
        hermes_app_requests_handle_text(
            config,
            network_ready,
            &request_ui,
            context->chat_result,
            context->last_message,
            context->last_message_size,
            context->reply_page,
            context->status_line,
            context->status_line_size,
            context->request_rc
        );
        return true;
    }

    if ((kDown & KEY_UP) != 0) {
        hermes_app_requests_handle_voice(
            config,
            network_ready,
            &request_ui,
            context->chat_result,
            context->last_message,
            context->last_message_size,
            context->reply_page,
            context->status_line,
            context->status_line_size,
            context->request_rc
        );
        return true;
    }

    return false;
}
