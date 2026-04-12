#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "app_conversations.h"
#include "app_input.h"
#include "app_requests.h"
#include "app_ui.h"
#include "bridge_chat.h"
#include "bridge_health.h"
#include "bridge_v2.h"
#include "voice_input.h"

static PrintConsole top_console;
static PrintConsole bottom_console;
static AppConversationState g_conversation_state;

static void render_ui(
    AppScreen screen,
    const HermesAppConfig* config,
    SettingsField selected_field,
    bool settings_dirty,
    const BridgeHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t reply_page,
    const char* status_line,
    Result last_rc
)
{
    hermes_app_ui_render(
        &top_console,
        &bottom_console,
        screen,
        config,
        selected_field,
        settings_dirty,
        health_result,
        chat_result,
        last_message,
        reply_page,
        status_line,
        last_rc,
        &g_conversation_state.list,
        g_conversation_state.selection
    );
}

static void handle_home_health_check(
    const HermesAppConfig* config,
    bool network_ready,
    SettingsField selected_field,
    bool settings_dirty,
    BridgeHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t* reply_page,
    char* status_line,
    Result* request_rc
)
{
    char health_url[HERMES_APP_HEALTH_URL_MAX];

    if (health_result == NULL || status_line == NULL || request_rc == NULL || reply_page == NULL)
        return;

    bridge_health_result_reset(health_result);
    *request_rc = 0;
    *reply_page = 0;

    if (!network_ready) {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Networking services failed to start.");
        render_ui(APP_SCREEN_HOME, config, selected_field, settings_dirty, health_result, chat_result, last_message, *reply_page, status_line, *request_rc);
        return;
    }

    if (!hermes_app_config_build_health_url(config, health_url, sizeof(health_url))) {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Local config is incomplete.");
        render_ui(APP_SCREEN_HOME, config, selected_field, settings_dirty, health_result, chat_result, last_message, *reply_page, status_line, *request_rc);
        return;
    }

    snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Checking Hermes gateway...");
    render_ui(APP_SCREEN_HOME, config, selected_field, settings_dirty, health_result, chat_result, last_message, *reply_page, status_line, *request_rc);
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();

    *request_rc = bridge_health_check_run(health_url, health_result);
    if (R_SUCCEEDED(*request_rc) && health_result->success)
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Hermes gateway OK.");
    else if (health_result->error[0] == '\0')
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Gateway check failed: 0x%08lX", (unsigned long)*request_rc);
    else
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "%s", health_result->error);

    render_ui(APP_SCREEN_HOME, config, selected_field, settings_dirty, health_result, chat_result, last_message, *reply_page, status_line, *request_rc);
}

int main(int argc, char* argv[])
{
    Result init_rc = 0;
    Result request_rc = 0;
    bool ac_ready = false;
    bool network_ready = false;
    bool settings_dirty = false;
    AppScreen screen = APP_SCREEN_HOME;
    SettingsField selected_field = SETTINGS_FIELD_HOST;
    HermesAppConfig config;
    HermesAppConfigLoadStatus load_status;
    BridgeHealthResult health_result;
    BridgeChatResult chat_result;
    char last_message[BRIDGE_CHAT_MESSAGE_MAX];
    char status_line[BRIDGE_CHAT_ERROR_MAX];
    size_t reply_page = 0;

    (void)argc;
    (void)argv;

    bridge_health_result_reset(&health_result);
    bridge_chat_result_reset(&chat_result);
    hermes_app_conversations_init(&g_conversation_state);
    last_message[0] = '\0';

    load_status = hermes_app_config_load(&config);
    if (load_status == HERMES_APP_CONFIG_LOAD_OK)
        snprintf(status_line, sizeof(status_line), "Loaded saved config from SD card.");
    else if (load_status == HERMES_APP_CONFIG_LOAD_DEFAULTED)
        snprintf(status_line, sizeof(status_line), "No saved config found. Using defaults.");
    else
        snprintf(status_line, sizeof(status_line), "Config load failed. Using defaults.");

    hermes_app_conversations_refresh_selection_from_active(&g_conversation_state, &config);

    // Old 3DS-friendly UI: text-first rendering split across top and bottom screens.
    gfxInitDefault();
    gfxSet3D(false);
    consoleInit(GFX_TOP, &top_console);
    consoleInit(GFX_BOTTOM, &bottom_console);

    init_rc = acInit();
    if (R_FAILED(init_rc)) {
        snprintf(status_line, sizeof(status_line), "acInit failed: 0x%08lX", (unsigned long)init_rc);
    } else {
        ac_ready = true;
        init_rc = bridge_health_network_init();
        if (R_FAILED(init_rc)) {
            snprintf(status_line, sizeof(status_line), "socInit failed: 0x%08lX", (unsigned long)init_rc);
        } else {
            network_ready = true;
        }
    }

    render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, init_rc);

    while (aptMainLoop())
    {
        u32 kDown;

        gspWaitForVBlank();
        hidScanInput();
        kDown = hidKeysDown();

        if (kDown & KEY_START)
            break;

        if (screen == APP_SCREEN_HOME) {
            AppRequestUiContext request_ui = {
                &top_console,
                &bottom_console,
                selected_field,
                settings_dirty,
                &health_result,
                render_ui,
            };

            if ((kDown & KEY_Y) != 0) {
                bridge_health_result_reset(&health_result);
                bridge_chat_result_reset(&chat_result);
                last_message[0] = '\0';
                reply_page = 0;
                request_rc = 0;
                snprintf(status_line, sizeof(status_line), "Status cleared. Ready to test again.");
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_X) != 0) {
                screen = APP_SCREEN_SETTINGS;
                snprintf(status_line, sizeof(status_line), "Settings opened.");
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_SELECT) != 0) {
                hermes_app_conversations_open_picker(&g_conversation_state, &config, &screen, status_line, sizeof(status_line));
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_L) != 0 && chat_result.success && reply_page > 0) {
                reply_page--;
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_R) != 0 && chat_result.success) {
                size_t page_count = hermes_app_ui_reply_page_count(chat_result.reply);
                if (reply_page + 1 < page_count)
                    reply_page++;
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_A) != 0)
                handle_home_health_check(&config, network_ready, selected_field, settings_dirty, &health_result, &chat_result, last_message, &reply_page, status_line, &request_rc);

            if ((kDown & KEY_B) != 0)
                hermes_app_requests_handle_text(&config, network_ready, &request_ui, &chat_result, last_message, sizeof(last_message), &reply_page, status_line, sizeof(status_line), &request_rc);

            if ((kDown & KEY_UP) != 0)
                hermes_app_requests_handle_voice(&config, network_ready, &request_ui, &chat_result, last_message, sizeof(last_message), &reply_page, status_line, sizeof(status_line), &request_rc);
        } else if (screen == APP_SCREEN_CONVERSATIONS) {
            if (hermes_app_conversations_handle_picker_input(
                    &g_conversation_state,
                    &config,
                    network_ready,
                    kDown,
                    &screen,
                    &chat_result,
                    last_message,
                    sizeof(last_message),
                    &reply_page,
                    status_line,
                    sizeof(status_line),
                    &request_rc
                )) {
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }
        } else {
            if ((kDown & KEY_B) != 0) {
                screen = APP_SCREEN_HOME;
                snprintf(status_line, sizeof(status_line), settings_dirty ? "Settings closed with unsaved changes." : "Returned home.");
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_UP) != 0) {
                if (selected_field == 0)
                    selected_field = SETTINGS_FIELD_COUNT - 1;
                else
                    selected_field = (SettingsField)(selected_field - 1);
                snprintf(status_line, sizeof(status_line), "Selected %s.", settings_field_label(selected_field));
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_DOWN) != 0) {
                selected_field = (SettingsField)((selected_field + 1) % SETTINGS_FIELD_COUNT);
                snprintf(status_line, sizeof(status_line), "Selected %s.", settings_field_label(selected_field));
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_Y) != 0) {
                hermes_app_config_set_defaults(&config);
                hermes_app_conversations_refresh_selection_from_active(&g_conversation_state, &config);
                settings_dirty = true;
                snprintf(status_line, sizeof(status_line), "Defaults restored. Save settings to keep them.");
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_A) != 0) {
                if (edit_selected_setting(&config, selected_field, status_line, sizeof(status_line)))
                    settings_dirty = true;
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_X) != 0) {
                if (hermes_app_config_save(&config)) {
                    settings_dirty = false;
                    snprintf(status_line, sizeof(status_line), "Settings saved to SD card.");
                } else {
                    snprintf(status_line, sizeof(status_line), "Could not save settings to SD card.");
                }
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
    }

    if (network_ready)
        bridge_health_network_exit();

    if (ac_ready)
        acExit();

    gfxExit();
    return 0;
}
