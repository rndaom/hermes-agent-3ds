#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "app_input.h"
#include "app_ui.h"
#include "bridge_chat.h"
#include "bridge_health.h"
#include "bridge_v2.h"
#include "voice_input.h"

static PrintConsole top_console;
static PrintConsole bottom_console;
static BridgeV2ConversationListResult g_conversation_list;
static size_t g_conversation_selection = 0;

static bool prompt_v2_approval_choice(char* out_choice, size_t out_size);

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

static void apply_v2_event_to_chat_result(const BridgeV2EventPollResult* event_result, BridgeChatResult* chat_result)
{
    if (event_result == NULL || chat_result == NULL)
        return;

    bridge_chat_result_reset(chat_result);

    if (!event_result->success) {
        snprintf(chat_result->error, sizeof(chat_result->error), "%s", event_result->error[0] != '\0' ? event_result->error : "V2 event polling failed.");
        return;
    }

    if (event_result->approval_required) {
        snprintf(chat_result->error, sizeof(chat_result->error), "Approval required. Use A/X/Y/B to respond.");
        return;
    }

    if (event_result->reply_text[0] == '\0') {
        snprintf(chat_result->error, sizeof(chat_result->error), "No assistant reply arrived yet.");
        return;
    }

    chat_result->success = true;
    snprintf(chat_result->reply, sizeof(chat_result->reply), "%s", event_result->reply_text);
}

static void set_chat_result_error(BridgeChatResult* chat_result, const char* error_message)
{
    if (chat_result == NULL)
        return;

    bridge_chat_result_reset(chat_result);
    snprintf(chat_result->error, sizeof(chat_result->error), "%s", error_message != NULL ? error_message : "Unknown error.");
}

static void reset_chat_request_state(BridgeChatResult* chat_result, BridgeV2MessageResult* message_result, Result* request_rc, size_t* reply_page)
{
    if (chat_result != NULL)
        bridge_chat_result_reset(chat_result);
    if (message_result != NULL)
        bridge_v2_message_result_reset(message_result);
    if (request_rc != NULL)
        *request_rc = 0;
    if (reply_page != NULL)
        *reply_page = 0;
}

static const char* conversation_id_for_message(const HermesAppConfig* config, const BridgeV2MessageResult* message_result)
{
    if (message_result != NULL && message_result->conversation_id[0] != '\0')
        return message_result->conversation_id;
    if (config != NULL && config->active_conversation_id[0] != '\0')
        return config->active_conversation_id;
    return DEFAULT_CONVERSATION_ID;
}

static Result poll_for_v2_reply(
    const HermesAppConfig* config,
    const char* events_url,
    const BridgeV2MessageResult* message_result,
    u32* event_cursor,
    BridgeV2EventPollResult* event_result,
    BridgeV2EventPollResult* matched_event_result,
    bool* matched_reply
)
{
    const char* conversation_id;
    int poll_attempt;
    Result request_rc = 0;

    if (config == NULL || events_url == NULL || message_result == NULL || event_cursor == NULL ||
        event_result == NULL || matched_event_result == NULL || matched_reply == NULL) {
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);
    }

    conversation_id = conversation_id_for_message(config, message_result);
    bridge_v2_event_poll_result_reset(event_result);
    bridge_v2_event_poll_result_reset(matched_event_result);
    *matched_reply = false;

    for (poll_attempt = 0; poll_attempt < 6; poll_attempt++) {
        request_rc = bridge_v2_poll_events(
            events_url,
            config->token,
            config->device_id,
            conversation_id,
            *event_cursor,
            5000,
            event_result
        );
        if (R_FAILED(request_rc))
            break;

        if (event_result->cursor > *event_cursor)
            *event_cursor = event_result->cursor;

        if (event_result->approval_required)
            break;

        if (event_result->reply_text[0] != '\0' &&
            strcmp(event_result->reply_to_message_id, message_result->message_id) == 0) {
            *matched_event_result = *event_result;
            *matched_reply = true;
            break;
        }
    }

    return request_rc;
}

static Result complete_v2_roundtrip(
    const HermesAppConfig* config,
    const char* events_url,
    const BridgeV2MessageResult* message_result,
    BridgeChatResult* chat_result,
    bool* missed_events_out
)
{
    char interaction_url[HERMES_APP_INTERACTION_URL_MAX];
    char approval_choice[16];
    BridgeV2EventPollResult event_result;
    BridgeV2EventPollResult matched_event_result;
    BridgeV2InteractionResult interaction_result;
    bool matched_reply = false;
    bool missed_events = false;
    u32 event_cursor;
    Result request_rc;

    if (missed_events_out != NULL)
        *missed_events_out = false;

    if (config == NULL || events_url == NULL || message_result == NULL || chat_result == NULL)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    event_cursor = message_result->cursor;
    request_rc = poll_for_v2_reply(
        config,
        events_url,
        message_result,
        &event_cursor,
        &event_result,
        &matched_event_result,
        &matched_reply
    );
    if (R_FAILED(request_rc)) {
        if (event_result.error[0] != '\0')
            set_chat_result_error(chat_result, event_result.error);
        return request_rc;
    }

    if (event_result.missed_events)
        missed_events = true;

    if (event_result.approval_required) {
        approval_choice[0] = '\0';
        bridge_v2_interaction_result_reset(&interaction_result);

        if (!hermes_app_config_build_interaction_url(config, event_result.request_id, interaction_url, sizeof(interaction_url))) {
            set_chat_result_error(chat_result, "Approval URL could not be built.");
            goto done;
        }
        if (!prompt_v2_approval_choice(approval_choice, sizeof(approval_choice))) {
            set_chat_result_error(chat_result, "Approval canceled.");
            goto done;
        }

        request_rc = bridge_v2_submit_interaction(interaction_url, config->token, approval_choice, &interaction_result);
        if (R_FAILED(request_rc) || !interaction_result.success) {
            set_chat_result_error(
                chat_result,
                interaction_result.error[0] != '\0' ? interaction_result.error : "Approval response failed."
            );
            goto done;
        }
        if (strcmp(approval_choice, "deny") == 0) {
            set_chat_result_error(chat_result, "Command denied.");
            request_rc = 0;
            goto done;
        }

        request_rc = poll_for_v2_reply(
            config,
            events_url,
            message_result,
            &event_cursor,
            &event_result,
            &matched_event_result,
            &matched_reply
        );
        if (R_FAILED(request_rc)) {
            if (event_result.error[0] != '\0')
                set_chat_result_error(chat_result, event_result.error);
            return request_rc;
        }

        if (event_result.missed_events)
            missed_events = true;
    }

    apply_v2_event_to_chat_result(matched_reply ? &matched_event_result : &event_result, chat_result);

done:
    if (missed_events_out != NULL)
        *missed_events_out = missed_events;
    return request_rc;
}

static void format_roundtrip_status(
    const BridgeChatResult* chat_result,
    bool missed_events,
    const char* success_message,
    char* status_line,
    size_t status_line_size
)
{
    if (status_line == NULL || status_line_size == 0)
        return;

    if (chat_result != NULL && chat_result->success) {
        if (missed_events)
            snprintf(status_line, status_line_size, "%s Some events were missed.", success_message);
        else
            snprintf(status_line, status_line_size, "%s", success_message);
    } else if (chat_result != NULL && chat_result->error[0] != '\0') {
        snprintf(status_line, status_line_size, "%s", chat_result->error);
    } else {
        snprintf(status_line, status_line_size, "No assistant reply arrived yet.");
    }
}

static bool prompt_v2_approval_choice(char* out_choice, size_t out_size)
{
    if (out_choice == NULL || out_size == 0)
        return false;

    out_choice[0] = '\0';

    while (aptMainLoop()) {
        u32 kDown;

        consoleSelect(&top_console);
        consoleClear();
        printf("Hermes Agent 3DS\n");
        printf("Approval required\n");
        printf("=================\n");
        printf("A: allow once\n");
        printf("X: allow session\n");
        printf("Y: allow always\n");
        printf("B: deny\n");

        consoleSelect(&bottom_console);
        consoleClear();
        printf("Approval controls\n");
        printf("================\n");
        printf("A once   X session\n");
        printf("Y always B deny\n");
        printf("START cancel\n");

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
        hidScanInput();
        kDown = hidKeysDown();

        if ((kDown & KEY_A) != 0) {
            snprintf(out_choice, out_size, "once");
            return true;
        }
        if ((kDown & KEY_X) != 0) {
            snprintf(out_choice, out_size, "session");
            return true;
        }
        if ((kDown & KEY_Y) != 0) {
            snprintf(out_choice, out_size, "always");
            return true;
        }
        if ((kDown & KEY_B) != 0) {
            snprintf(out_choice, out_size, "deny");
            return true;
        }
        if ((kDown & KEY_START) != 0)
            return false;
    }

    return false;
}

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
        &g_conversation_list,
        g_conversation_selection
    );
}

static size_t find_recent_conversation_index(const HermesAppConfig* config, const char* conversation_id)
{
    size_t index;

    if (config == NULL || conversation_id == NULL || conversation_id[0] == '\0')
        return 0;

    for (index = 0; index < config->recent_conversation_count; index++) {
        if (strcmp(config->recent_conversations[index], conversation_id) == 0)
            return index;
    }

    return 0;
}

static void clamp_conversation_selection(const HermesAppConfig* config)
{
    if (config == NULL || config->recent_conversation_count == 0) {
        g_conversation_selection = 0;
        return;
    }

    if (g_conversation_selection >= config->recent_conversation_count)
        g_conversation_selection = config->recent_conversation_count - 1;
}

static void merge_synced_conversations_into_config(HermesAppConfig* config)
{
    size_t index;

    if (config == NULL)
        return;

    for (index = g_conversation_list.count; index > 0; index--)
        hermes_app_config_touch_recent_conversation(config, g_conversation_list.conversations[index - 1].conversation_id);

    if (config->active_conversation_id[0] == '\0')
        hermes_app_config_set_active_conversation(config, DEFAULT_CONVERSATION_ID);

    hermes_app_config_touch_recent_conversation(config, config->active_conversation_id);
    clamp_conversation_selection(config);
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

static void handle_activate_conversation(
    HermesAppConfig* config,
    AppScreen* screen,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t* reply_page,
    char* status_line
)
{
    if (config == NULL || screen == NULL || chat_result == NULL || last_message == NULL || reply_page == NULL || status_line == NULL)
        return;

    if (config->recent_conversation_count == 0) {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "No saved conversations yet.");
        return;
    }
    if (!hermes_app_config_set_active_conversation(config, config->recent_conversations[g_conversation_selection])) {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Conversation selection was invalid.");
        return;
    }
    if (!hermes_app_config_save(config)) {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Could not save conversation selection.");
        return;
    }

    bridge_chat_result_reset(chat_result);
    last_message[0] = '\0';
    *reply_page = 0;
    *screen = APP_SCREEN_HOME;
    snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Conversation %s selected.", config->active_conversation_id);
}

static void handle_create_conversation(
    HermesAppConfig* config,
    AppScreen* screen,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t* reply_page,
    char* status_line
)
{
    char conversation_id[HERMES_APP_CONVERSATION_ID_MAX];

    if (config == NULL || screen == NULL || chat_result == NULL || last_message == NULL || reply_page == NULL || status_line == NULL)
        return;

    if (!prompt_conversation_input("", conversation_id, sizeof(conversation_id))) {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "New conversation canceled.");
        return;
    }
    if (!hermes_app_config_is_valid_conversation_id(conversation_id)) {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Conversation IDs only allow letters, numbers, - _ .");
        return;
    }
    if (!hermes_app_config_set_active_conversation(config, conversation_id)) {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Could not activate that conversation.");
        return;
    }
    if (!hermes_app_config_save(config)) {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Could not save new conversation.");
        return;
    }

    bridge_chat_result_reset(chat_result);
    last_message[0] = '\0';
    *reply_page = 0;
    g_conversation_selection = find_recent_conversation_index(config, config->active_conversation_id);
    *screen = APP_SCREEN_HOME;
    snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Conversation %s created.", config->active_conversation_id);
}

static void handle_conversation_sync(
    HermesAppConfig* config,
    bool network_ready,
    char* status_line,
    Result* request_rc
)
{
    char conversations_url[HERMES_APP_CONVERSATIONS_URL_MAX];
    BridgeV2ConversationListResult fetched_conversations;

    if (config == NULL || status_line == NULL || request_rc == NULL)
        return;

    bridge_v2_conversation_list_result_reset(&fetched_conversations);
    *request_rc = 0;
    if (!network_ready) {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Networking services failed to start.");
        return;
    }
    if (!hermes_app_config_build_conversations_url(config, conversations_url, sizeof(conversations_url)) ||
        config->device_id[0] == '\0') {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "V2 config is incomplete. Set device_id.");
        return;
    }

    *request_rc = bridge_v2_list_conversations(conversations_url, config->token, config->device_id, &fetched_conversations);
    if (R_SUCCEEDED(*request_rc) && fetched_conversations.success) {
        g_conversation_list = fetched_conversations;
        merge_synced_conversations_into_config(config);
        if (!hermes_app_config_save(config))
            snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Synced, but could not save conversations.");
        else
            snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Synced %lu conversations.", (unsigned long)g_conversation_list.count);
    } else if (fetched_conversations.error[0] != '\0') {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "%s", fetched_conversations.error);
    } else {
        snprintf(status_line, BRIDGE_CHAT_ERROR_MAX, "Conversation sync failed: 0x%08lX", (unsigned long)*request_rc);
    }
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
    bridge_v2_conversation_list_result_reset(&g_conversation_list);
    g_conversation_selection = 0;
    last_message[0] = '\0';

    load_status = hermes_app_config_load(&config);
    if (load_status == HERMES_APP_CONFIG_LOAD_OK)
        snprintf(status_line, sizeof(status_line), "Loaded saved config from SD card.");
    else if (load_status == HERMES_APP_CONFIG_LOAD_DEFAULTED)
        snprintf(status_line, sizeof(status_line), "No saved config found. Using defaults.");
    else
        snprintf(status_line, sizeof(status_line), "Config load failed. Using defaults.");

    g_conversation_selection = find_recent_conversation_index(&config, config.active_conversation_id);
    clamp_conversation_selection(&config);

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
                screen = APP_SCREEN_CONVERSATIONS;
                g_conversation_selection = find_recent_conversation_index(&config, config.active_conversation_id);
                clamp_conversation_selection(&config);
                snprintf(status_line, sizeof(status_line), "Conversation picker opened.");
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

            if ((kDown & KEY_B) != 0) {
                char message_buffer[BRIDGE_CHAT_MESSAGE_MAX];
                char messages_url[HERMES_APP_MESSAGES_URL_MAX];
                char events_url[HERMES_APP_EVENTS_URL_MAX];
                BridgeV2MessageResult message_result;
                bool missed_events = false;

                message_buffer[0] = '\0';
                if (!prompt_message_input(message_buffer, sizeof(message_buffer))) {
                    snprintf(status_line, sizeof(status_line), "Ask Hermes canceled.");
                    render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                    continue;
                }

                copy_bounded_string(last_message, sizeof(last_message), message_buffer);
                reset_chat_request_state(&chat_result, &message_result, &request_rc, &reply_page);

                if (!hermes_app_config_build_messages_url(&config, messages_url, sizeof(messages_url)) ||
                    !hermes_app_config_build_events_url(&config, events_url, sizeof(events_url)) ||
                    config.device_id[0] == '\0') {
                    snprintf(status_line, sizeof(status_line), "V2 config is incomplete. Set device_id.");
                    render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                    continue;
                }

                snprintf(status_line, sizeof(status_line), "Asking Hermes over v2...");
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                gfxFlushBuffers();
                gfxSwapBuffers();
                gspWaitForVBlank();

                if (network_ready) {
                    request_rc = bridge_v2_send_message(messages_url, config.token, config.device_id, config.active_conversation_id, message_buffer, &message_result);
                    if (R_SUCCEEDED(request_rc) && message_result.success) {
                        request_rc = complete_v2_roundtrip(&config, events_url, &message_result, &chat_result, &missed_events);
                        if (R_SUCCEEDED(request_rc))
                            format_roundtrip_status(&chat_result, missed_events, "Reply received over native v2.", status_line, sizeof(status_line));
                        else if (chat_result.error[0] != '\0')
                            snprintf(status_line, sizeof(status_line), "%s", chat_result.error);
                        else
                            snprintf(status_line, sizeof(status_line), "Chat failed: 0x%08lX", (unsigned long)request_rc);
                    } else if (message_result.error[0] != '\0')
                        snprintf(status_line, sizeof(status_line), "%s", message_result.error);
                    else
                        snprintf(status_line, sizeof(status_line), "Chat failed: 0x%08lX", (unsigned long)request_rc);
                } else {
                    snprintf(status_line, sizeof(status_line), "Networking services failed to start.");
                }

                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_UP) != 0) {
                char voice_url[HERMES_APP_VOICE_URL_MAX];
                char events_url[HERMES_APP_EVENTS_URL_MAX];
                u8* wav_data = NULL;
                size_t wav_size = 0;
                BridgeV2MessageResult message_result;
                bool missed_events = false;

                if (!network_ready) {
                    snprintf(status_line, sizeof(status_line), "Networking services failed to start.");
                    render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                    continue;
                }

                if (!hermes_app_config_build_voice_url(&config, voice_url, sizeof(voice_url)) ||
                    !hermes_app_config_build_events_url(&config, events_url, sizeof(events_url)) ||
                    config.device_id[0] == '\0') {
                    snprintf(status_line, sizeof(status_line), "V2 config is incomplete. Set device_id.");
                    render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                    continue;
                }

                if (!voice_input_record_prompt(&top_console, &bottom_console, &wav_data, &wav_size, status_line, sizeof(status_line))) {
                    render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                    continue;
                }

                copy_bounded_string(last_message, sizeof(last_message), "[mic] voice input");
                reset_chat_request_state(&chat_result, &message_result, &request_rc, &reply_page);

                snprintf(status_line, sizeof(status_line), "Sending mic input to Hermes...");
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                gfxFlushBuffers();
                gfxSwapBuffers();
                gspWaitForVBlank();

                request_rc = bridge_v2_send_voice_message(voice_url, config.token, config.device_id, config.active_conversation_id, wav_data, wav_size, &message_result);
                free(wav_data);
                wav_data = NULL;

                if (R_SUCCEEDED(request_rc) && message_result.success) {
                    request_rc = complete_v2_roundtrip(&config, events_url, &message_result, &chat_result, &missed_events);
                    if (R_SUCCEEDED(request_rc))
                        format_roundtrip_status(&chat_result, missed_events, "Voice reply received over native v2.", status_line, sizeof(status_line));
                    else if (chat_result.error[0] != '\0')
                        snprintf(status_line, sizeof(status_line), "%s", chat_result.error);
                    else
                        snprintf(status_line, sizeof(status_line), "Voice roundtrip failed: 0x%08lX", (unsigned long)request_rc);
                } else if (message_result.error[0] != '\0') {
                    snprintf(status_line, sizeof(status_line), "%s", message_result.error);
                } else {
                    snprintf(status_line, sizeof(status_line), "Voice upload failed: 0x%08lX", (unsigned long)request_rc);
                }

                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }
        } else if (screen == APP_SCREEN_CONVERSATIONS) {
            if ((kDown & KEY_B) != 0) {
                screen = APP_SCREEN_HOME;
                snprintf(status_line, sizeof(status_line), "Returned home.");
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_UP) != 0 && config.recent_conversation_count > 0) {
                if (g_conversation_selection == 0)
                    g_conversation_selection = config.recent_conversation_count - 1;
                else
                    g_conversation_selection--;
                snprintf(status_line, sizeof(status_line), "Selected %s.", config.recent_conversations[g_conversation_selection]);
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_DOWN) != 0 && config.recent_conversation_count > 0) {
                g_conversation_selection = (g_conversation_selection + 1) % config.recent_conversation_count;
                snprintf(status_line, sizeof(status_line), "Selected %s.", config.recent_conversations[g_conversation_selection]);
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_A) != 0) {
                handle_activate_conversation(&config, &screen, &chat_result, last_message, &reply_page, status_line);
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_X) != 0) {
                handle_create_conversation(&config, &screen, &chat_result, last_message, &reply_page, status_line);
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_Y) != 0) {
                handle_conversation_sync(&config, network_ready, status_line, &request_rc);
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
                g_conversation_selection = find_recent_conversation_index(&config, config.active_conversation_id);
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
