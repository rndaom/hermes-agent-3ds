#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "app_gfx.h"
#include "app_home.h"
#include "app_conversations.h"
#include "app_input.h"
#include "app_settings.h"
#include "app_ui.h"
#include "bridge_chat.h"
#include "bridge_health.h"

static AppConversationState g_conversation_state;

static void render_ui(
    AppScreen screen,
    const HermesAppConfig* config,
    SettingsField selected_field,
    bool settings_dirty,
    const GatewayHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t reply_page,
    size_t command_selection,
    const char* status_line,
    Result last_rc
)
{
    hermes_app_ui_render(
        screen,
        config,
        selected_field,
        settings_dirty,
        health_result,
        chat_result,
        last_message,
        reply_page,
        command_selection,
        status_line,
        last_rc,
        &g_conversation_state.list,
        g_conversation_state.selection
    );
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
    HomeCommand command_selection = HOME_COMMAND_ASK;
    HermesAppConfig config;
    HermesAppConfigLoadStatus load_status;
    GatewayHealthResult health_result;
    BridgeChatResult chat_result;
    char last_message[BRIDGE_CHAT_MESSAGE_MAX];
    char status_line[BRIDGE_CHAT_ERROR_MAX];
    size_t reply_page = 0;

    (void)argc;
    (void)argv;

    gateway_health_result_reset(&health_result);
    bridge_chat_result_reset(&chat_result);
    hermes_app_ui_home_history_reset();
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

    /*
     * Graphical UI bootstrap moved behind hermes_app_ui_init() / app_gfx_init().
     * That path now owns gfxInitDefault() and gfxSet3D(false) for the Old 3DS GUI.
     */
    if (!hermes_app_ui_init()) {
        return 1;
    }

    init_rc = acInit();
    if (R_FAILED(init_rc)) {
        snprintf(status_line, sizeof(status_line), "acInit failed: 0x%08lX", (unsigned long)init_rc);
    } else {
        ac_ready = true;
        init_rc = gateway_health_network_init();
        if (R_FAILED(init_rc)) {
            snprintf(status_line, sizeof(status_line), "socInit failed: 0x%08lX", (unsigned long)init_rc);
        } else {
            network_ready = true;
        }
    }

    render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, command_selection, status_line, init_rc);

    while (aptMainLoop())
    {
        u32 kDown;

        gspWaitForVBlank();
        hidScanInput();
        kDown = hidKeysDown();

        if (kDown & KEY_START)
            break;

        if (screen == APP_SCREEN_HOME) {
            AppHomeContext home_context = {
                &g_conversation_state,
                selected_field,
                settings_dirty,
                &health_result,
                &chat_result,
                last_message,
                sizeof(last_message),
                &reply_page,
                &command_selection,
                status_line,
                sizeof(status_line),
                &request_rc,
                render_ui,
            };

            hermes_app_home_handle_input(kDown, &config, network_ready, &screen, &home_context);
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
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, command_selection, status_line, request_rc);
            }
        } else {
            AppSettingsContext settings_context = {&g_conversation_state};

            if (hermes_app_settings_handle_input(
                    kDown,
                    &config,
                    &settings_context,
                    &screen,
                    &selected_field,
                    &settings_dirty,
                    status_line,
                    sizeof(status_line)
                )) {
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, command_selection, status_line, request_rc);
            }
        }

        render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, command_selection, status_line, request_rc);
    }

    if (network_ready)
        gateway_health_network_exit();

    if (ac_ready)
        acExit();

    hermes_app_ui_exit();
    return 0;
}
