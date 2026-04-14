#include "app_home.h"

#include <stdio.h>

#include "app_requests.h"

#define HOME_SHOULDER_SCROLL_STEP 48U

typedef enum HomeCommandPage {
    HOME_COMMAND_PAGE_TOOLS = 0,
    HOME_COMMAND_PAGE_SLASH_PRIMARY = 1,
    HOME_COMMAND_PAGE_SLASH_SECONDARY = 2,
} HomeCommandPage;

static HomeCommandPage home_page_for_command(HomeCommand command)
{
    if (command >= HOME_COMMAND_RESET && command <= HOME_COMMAND_ROLLBACK)
        return HOME_COMMAND_PAGE_SLASH_PRIMARY;
    if (command >= HOME_COMMAND_REASONING && command <= HOME_COMMAND_RESUME)
        return HOME_COMMAND_PAGE_SLASH_SECONDARY;
    return HOME_COMMAND_PAGE_TOOLS;
}

static HomeCommand home_page_default_selection(HomeCommandPage page)
{
    switch (page) {
        case HOME_COMMAND_PAGE_SLASH_PRIMARY:
            return HOME_COMMAND_RESET;
        case HOME_COMMAND_PAGE_SLASH_SECONDARY:
            return HOME_COMMAND_REASONING;
        case HOME_COMMAND_PAGE_TOOLS:
        default:
            return HOME_COMMAND_TEXT;
    }
}

static HomeCommand home_page_last_selection(HomeCommandPage page)
{
    switch (page) {
        case HOME_COMMAND_PAGE_SLASH_PRIMARY:
            return HOME_COMMAND_ROLLBACK;
        case HOME_COMMAND_PAGE_SLASH_SECONDARY:
            return HOME_COMMAND_RESUME;
        case HOME_COMMAND_PAGE_TOOLS:
        default:
            return HOME_COMMAND_PICTURE;
    }
}

static HomeCommandPage adjacent_home_page(HomeCommandPage page, int direction)
{
    if (direction < 0) {
        if (page == HOME_COMMAND_PAGE_SLASH_SECONDARY)
            return HOME_COMMAND_PAGE_SLASH_PRIMARY;
        return HOME_COMMAND_PAGE_TOOLS;
    }

    if (page == HOME_COMMAND_PAGE_TOOLS)
        return HOME_COMMAND_PAGE_SLASH_PRIMARY;
    return HOME_COMMAND_PAGE_SLASH_SECONDARY;
}

static void switch_home_page(AppHomeContext* context, HomeCommandPage page)
{
    if (context == NULL || context->command_selection == NULL)
        return;
    *context->command_selection = home_page_default_selection(page);
}

static bool scroll_home_history(AppHomeContext* context, size_t max_scroll, int direction, size_t step)
{
    if (context == NULL || context->history_scroll == NULL || direction == 0 || step == 0)
        return false;

    if (direction > 0) {
        if (*context->history_scroll >= max_scroll)
            return false;
        *context->history_scroll += step;
        if (*context->history_scroll > max_scroll)
            *context->history_scroll = max_scroll;
        return true;
    }

    if (*context->history_scroll == 0)
        return false;
    if (*context->history_scroll > step)
        *context->history_scroll -= step;
    else
        *context->history_scroll = 0;
    return true;
}

static void render_screen(AppScreen screen, const HermesAppConfig* config, const AppHomeContext* context)
{
    if (context == NULL || context->render == NULL || context->history_scroll == NULL ||
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
        *context->history_scroll,
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
        context->request_rc == NULL || context->history_scroll == NULL) {
        return;
    }

    gateway_health_result_reset(context->health_result);
    *context->request_rc = 0;
    *context->history_scroll = 0;

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

    snprintf(context->status_line, context->status_line_size, "Checking Hermes relay...");
    render_home_screen(config, context);
    gspWaitForVBlank();

    *context->request_rc = gateway_health_check_run(health_url, context->health_result);
    if (R_SUCCEEDED(*context->request_rc) && context->health_result->success)
        snprintf(context->status_line, context->status_line_size, "Hermes relay OK.");
    else if (context->health_result->error[0] == '\0')
        snprintf(context->status_line, context->status_line_size, "Relay check failed: 0x%08lX", (unsigned long)*context->request_rc);
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
        case HOME_COMMAND_TEXT:
            hermes_app_requests_handle_text(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc
            );
            break;
        case HOME_COMMAND_CHECK:
            handle_home_health_check(config, network_ready, context);
            break;
        case HOME_COMMAND_SESSIONS:
            hermes_app_conversations_open_picker(context->conversation_state, config, screen, context->status_line, context->status_line_size);
            render_screen(*screen, config, context);
            break;
        case HOME_COMMAND_SETTINGS:
            *screen = APP_SCREEN_SETTINGS;
            snprintf(context->status_line, context->status_line_size, "Settings opened.");
            render_screen(*screen, config, context);
            break;
        case HOME_COMMAND_AUDIO:
            hermes_app_requests_handle_voice(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc
            );
            break;
        case HOME_COMMAND_PICTURE:
            hermes_app_requests_handle_picture(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc
            );
            break;
        case HOME_COMMAND_RESET:
            hermes_app_requests_handle_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc,
                "/reset"
            );
            break;
        case HOME_COMMAND_CLEAR:
            hermes_app_requests_handle_clear_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc
            );
            break;
        case HOME_COMMAND_COMPRESS:
            hermes_app_requests_handle_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc,
                "/compress"
            );
            break;
        case HOME_COMMAND_HELP:
            hermes_app_requests_handle_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc,
                "/help"
            );
            break;
        case HOME_COMMAND_STATUS:
            hermes_app_requests_handle_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc,
                "/status"
            );
            break;
        case HOME_COMMAND_COMMANDS:
            hermes_app_requests_handle_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc,
                "/commands"
            );
            break;
        case HOME_COMMAND_PROVIDER:
            hermes_app_requests_handle_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc,
                "/provider"
            );
            break;
        case HOME_COMMAND_ROLLBACK:
            hermes_app_requests_handle_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc,
                "/rollback"
            );
            break;
        case HOME_COMMAND_REASONING:
            hermes_app_requests_handle_reasoning_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc
            );
            break;
        case HOME_COMMAND_FAST:
            hermes_app_requests_handle_fast_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc
            );
            break;
        case HOME_COMMAND_MODEL:
            hermes_app_requests_handle_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc,
                "/model"
            );
            break;
        case HOME_COMMAND_PERSONALITY:
            hermes_app_requests_handle_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc,
                "/personality"
            );
            break;
        case HOME_COMMAND_RESUME:
            hermes_app_requests_handle_command(
                config,
                network_ready,
                request_ui,
                context->chat_result,
                context->last_message,
                context->last_message_size,
                context->history_scroll,
                context->status_line,
                context->status_line_size,
                context->request_rc,
                "/resume"
            );
            break;
    }
}

static void move_home_command_selection(AppHomeContext* context, int direction)
{
    int selection;
    HomeCommandPage page;
    int first_command;
    int last_command;

    if (context == NULL || context->command_selection == NULL || direction == 0)
        return;

    page = home_page_for_command(*context->command_selection);
    first_command = (int)home_page_default_selection(page);
    last_command = (int)home_page_last_selection(page);

    selection = context->command_selection != NULL && *context->command_selection == HOME_COMMAND_NONE
        ? (direction > 0 ? first_command : last_command)
        : (int)*context->command_selection + direction;
    if (selection < first_command)
        selection = last_command;
    else if (selection > last_command)
        selection = first_command;

    *context->command_selection = (HomeCommand)selection;
}

static HomeCommand home_command_from_touch(HomeCommandPage page, int px, int py)
{
    if (page == HOME_COMMAND_PAGE_SLASH_PRIMARY) {
        if (px >= 16 && px < 152 && py >= 44 && py < 72)
            return HOME_COMMAND_RESET;
        if (px >= 168 && px < 304 && py >= 44 && py < 72)
            return HOME_COMMAND_CLEAR;
        if (px >= 16 && px < 152 && py >= 80 && py < 108)
            return HOME_COMMAND_COMPRESS;
        if (px >= 168 && px < 304 && py >= 80 && py < 108)
            return HOME_COMMAND_HELP;
        if (px >= 16 && px < 152 && py >= 116 && py < 144)
            return HOME_COMMAND_STATUS;
        if (px >= 168 && px < 304 && py >= 116 && py < 144)
            return HOME_COMMAND_COMMANDS;
        if (px >= 16 && px < 152 && py >= 152 && py < 180)
            return HOME_COMMAND_PROVIDER;
        if (px >= 168 && px < 304 && py >= 152 && py < 180)
            return HOME_COMMAND_ROLLBACK;
        return HOME_COMMAND_NONE;
    }

    if (page == HOME_COMMAND_PAGE_SLASH_SECONDARY) {
        if (px >= 16 && px < 152 && py >= 44 && py < 72)
            return HOME_COMMAND_REASONING;
        if (px >= 168 && px < 304 && py >= 44 && py < 72)
            return HOME_COMMAND_FAST;
        if (px >= 16 && px < 152 && py >= 80 && py < 108)
            return HOME_COMMAND_MODEL;
        if (px >= 168 && px < 304 && py >= 80 && py < 108)
            return HOME_COMMAND_PERSONALITY;
        if (px >= 92 && px < 228 && py >= 116 && py < 144)
            return HOME_COMMAND_RESUME;
        return HOME_COMMAND_NONE;
    }

    if (px >= 16 && px < 152 && py >= 44 && py < 72)
        return HOME_COMMAND_TEXT;
    if (px >= 168 && px < 304 && py >= 44 && py < 72)
        return HOME_COMMAND_CHECK;
    if (px >= 16 && px < 152 && py >= 80 && py < 108)
        return HOME_COMMAND_SESSIONS;
    if (px >= 168 && px < 304 && py >= 80 && py < 108)
        return HOME_COMMAND_SETTINGS;
    if (px >= 16 && px < 152 && py >= 116 && py < 144)
        return HOME_COMMAND_AUDIO;
    if (px >= 168 && px < 304 && py >= 116 && py < 144)
        return HOME_COMMAND_PICTURE;
    return HOME_COMMAND_NONE;
}

static bool home_page_toggle_touched(int px, int py)
{
    return px >= 244 && px < 304 && py >= 12 && py < 28;
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
    bool page_left;
    bool page_right;
    bool shoulder_up;
    bool shoulder_down;
    size_t max_scroll;
    HomeCommandPage active_page;
    HomeCommandPage target_page;

    if (config == NULL || screen == NULL || context == NULL || context->conversation_state == NULL ||
        context->chat_result == NULL || context->last_message == NULL || context->history_scroll == NULL ||
        context->command_selection == NULL || context->status_line == NULL || context->request_rc == NULL) {
        return false;
    }

    request_ui.selected_field = context->selected_field;
    request_ui.settings_dirty = context->settings_dirty;
    request_ui.health_result = context->health_result;
    request_ui.command_selection = (size_t)*context->command_selection;
    request_ui.render = context->render;
    active_page = home_page_for_command(*context->command_selection);

    nav_down = (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) != 0;
    nav_up = (kDown & (KEY_UP | KEY_CPAD_UP)) != 0;
    page_left = (kDown & KEY_LEFT) != 0;
    page_right = (kDown & KEY_RIGHT) != 0;
    shoulder_up = (kDown & KEY_L) != 0;
    shoulder_down = (kDown & KEY_R) != 0;

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

    if (page_left && active_page != HOME_COMMAND_PAGE_TOOLS) {
        target_page = adjacent_home_page(active_page, -1);
        switch_home_page(context, target_page);
        render_home_screen(config, context);
        return true;
    }

    if (page_right && active_page != HOME_COMMAND_PAGE_SLASH_SECONDARY) {
        target_page = adjacent_home_page(active_page, 1);
        switch_home_page(context, target_page);
        render_home_screen(config, context);
        return true;
    }

    max_scroll = hermes_app_ui_home_history_max_scroll(context->status_line);
    if (scroll_home_history(context, max_scroll, shoulder_up ? 1 : 0, HOME_SHOULDER_SCROLL_STEP)) {
        render_home_screen(config, context);
        return true;
    }

    if (scroll_home_history(context, max_scroll, shoulder_down ? -1 : 0, HOME_SHOULDER_SCROLL_STEP)) {
        render_home_screen(config, context);
        return true;
    }

    if ((kDown & KEY_TOUCH) != 0) {
        hidTouchRead(&touch);
        if (home_page_toggle_touched((int)touch.px, (int)touch.py)) {
            switch_home_page(context, active_page == HOME_COMMAND_PAGE_SLASH_SECONDARY ? HOME_COMMAND_PAGE_TOOLS : adjacent_home_page(active_page, 1));
            render_home_screen(config, context);
            return true;
        }
        *context->command_selection = home_command_from_touch(active_page, (int)touch.px, (int)touch.py);
        if (*context->command_selection != HOME_COMMAND_NONE) {
            request_ui.command_selection = (size_t)*context->command_selection;
            execute_selected_home_command(config, network_ready, screen, context, &request_ui);
            return true;
        }
    }

    if ((kDown & KEY_A) != 0) {
        if (*context->command_selection == HOME_COMMAND_NONE)
            *context->command_selection = HOME_COMMAND_TEXT;
        request_ui.command_selection = (size_t)*context->command_selection;
        execute_selected_home_command(config, network_ready, screen, context, &request_ui);
        return true;
    }

    return false;
}
