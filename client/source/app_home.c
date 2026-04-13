#include "app_home.h"

#include <stdio.h>

#include "app_requests.h"

static void render_screen(AppScreen screen, const HermesAppConfig* config, const AppHomeContext* context)
{
    if (context == NULL || context->render == NULL || context->reply_page == NULL ||
        context->command_selection == NULL || context->status_line == NULL || context->request_rc == NULL) {
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
        (size_t)*context->command_selection,
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

static void execute_selected_home_command(
    const HermesAppConfig* config,
    bool network_ready,
    AppScreen* screen,
    AppHomeContext* context,
    AppRequestUiContext* request_ui
)
{
    if (config == NULL || screen == NULL || context == NULL || context->command_selection == NULL || request_ui == NULL)
        return;

    request_ui->command_selection = (size_t)*context->command_selection;

    switch (*context->command_selection) {
        case HOME_COMMAND_NONE:
            break;
        case HOME_COMMAND_ASK:
            hermes_app_requests_handle_text(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->reply_page,
                context->status_line,
                context->status_line_size,
                context->request_rc
            );
            break;
        case HOME_COMMAND_CHECK:
            handle_home_health_check(config, network_ready, context);
            break;
        case HOME_COMMAND_THREADS:
            hermes_app_conversations_open_picker(context->conversation_state, config, screen, context->status_line, context->status_line_size);
            render_screen(*screen, config, context);
            break;
        case HOME_COMMAND_CONFIG:
            *screen = APP_SCREEN_SETTINGS;
            snprintf(context->status_line, context->status_line_size, "Settings opened.");
            render_screen(*screen, config, context);
            break;
        case HOME_COMMAND_MIC:
            hermes_app_requests_handle_voice(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->reply_page,
                context->status_line,
                context->status_line_size,
                context->request_rc
            );
            break;
        case HOME_COMMAND_CLEAR:
            bridge_health_result_reset(context->health_result);
            bridge_chat_result_reset(context->chat_result);
            hermes_app_ui_home_history_reset();
            context->last_message[0] = '\0';
            *context->reply_page = 0;
            *context->request_rc = 0;
            snprintf(context->status_line, context->status_line_size, "Status cleared. Ready to test again.");
            render_home_screen(config, context);
            break;
    }
}

static void move_home_command_selection(AppHomeContext* context, int direction)
{
    int selection;

    if (context == NULL || context->command_selection == NULL || direction == 0)
        return;

    selection = context->command_selection != NULL && *context->command_selection == HOME_COMMAND_NONE
        ? (direction > 0 ? (int)HOME_COMMAND_ASK : (int)HOME_COMMAND_CLEAR)
        : (int)*context->command_selection + direction;
    if (selection < (int)HOME_COMMAND_ASK)
        selection = (int)HOME_COMMAND_CLEAR;
    else if (selection > (int)HOME_COMMAND_CLEAR)
        selection = (int)HOME_COMMAND_ASK;

    *context->command_selection = (HomeCommand)selection;
}

static HomeCommand home_command_from_touch(int px, int py)
{
    if (px >= 16 && px < 152 && py >= 44 && py < 72)
        return HOME_COMMAND_ASK;
    if (px >= 168 && px < 304 && py >= 44 && py < 72)
        return HOME_COMMAND_CHECK;
    if (px >= 16 && px < 152 && py >= 80 && py < 108)
        return HOME_COMMAND_THREADS;
    if (px >= 168 && px < 304 && py >= 80 && py < 108)
        return HOME_COMMAND_CONFIG;
    if (px >= 16 && px < 152 && py >= 116 && py < 144)
        return HOME_COMMAND_MIC;
    if (px >= 168 && px < 304 && py >= 116 && py < 144)
        return HOME_COMMAND_CLEAR;
    return HOME_COMMAND_NONE;
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
    touchPosition touch;
    bool nav_down;
    bool nav_up;
    bool nav_left;
    bool nav_right;

    if (config == NULL || screen == NULL || context == NULL || context->conversation_state == NULL ||
        context->chat_result == NULL || context->last_message == NULL || context->reply_page == NULL ||
        context->command_selection == NULL || context->status_line == NULL || context->request_rc == NULL) {
        return false;
    }

    request_ui.selected_field = context->selected_field;
    request_ui.settings_dirty = context->settings_dirty;
    request_ui.health_result = context->health_result;
    request_ui.command_selection = (size_t)*context->command_selection;
    request_ui.render = context->render;

    nav_down = (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) != 0;
    nav_up = (kDown & (KEY_UP | KEY_CPAD_UP)) != 0;
    nav_left = (kDown & (KEY_LEFT | KEY_CPAD_LEFT | KEY_L)) != 0;
    nav_right = (kDown & (KEY_RIGHT | KEY_CPAD_RIGHT | KEY_R)) != 0;

    if (nav_down) {
        move_home_command_selection(context, 1);
        render_home_screen(config, context);
        return true;
    }

    if (nav_up) {
        move_home_command_selection(context, -1);
        render_home_screen(config, context);
        return true;
    }

    if (nav_left && context->chat_result->success && *context->reply_page > 0) {
        (*context->reply_page)--;
        render_home_screen(config, context);
        return true;
    }

    if (nav_right && context->chat_result->success) {
        size_t page_count = hermes_app_ui_reply_page_count(context->chat_result->reply);
        if (*context->reply_page + 1 < page_count)
            (*context->reply_page)++;
        render_home_screen(config, context);
        return true;
    }

    if ((kDown & KEY_TOUCH) != 0) {
        hidTouchRead(&touch);
        *context->command_selection = home_command_from_touch((int)touch.px, (int)touch.py);
        if (*context->command_selection != HOME_COMMAND_NONE) {
            request_ui.command_selection = (size_t)*context->command_selection;
            execute_selected_home_command(config, network_ready, screen, context, &request_ui);
            return true;
        }
    }

    if ((kDown & KEY_A) != 0) {
        if (*context->command_selection == HOME_COMMAND_NONE)
            *context->command_selection = HOME_COMMAND_ASK;
        request_ui.command_selection = (size_t)*context->command_selection;
        execute_selected_home_command(config, network_ready, screen, context, &request_ui);
        return true;
    }

    return false;
}
