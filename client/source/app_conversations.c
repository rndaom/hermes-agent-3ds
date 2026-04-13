#include "app_conversations.h"

#include <stdio.h>
#include <string.h>

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

static void clamp_conversation_selection(AppConversationState* state, const HermesAppConfig* config)
{
    if (state == NULL)
        return;

    if (config == NULL || config->recent_conversation_count == 0) {
        state->selection = 0;
        return;
    }

    if (state->selection >= config->recent_conversation_count)
        state->selection = config->recent_conversation_count - 1;
}

static void merge_synced_conversations_into_config(AppConversationState* state, HermesAppConfig* config)
{
    size_t index;

    if (state == NULL || config == NULL)
        return;

    for (index = state->list.count; index > 0; index--)
        hermes_app_config_touch_recent_conversation(config, state->list.conversations[index - 1].conversation_id);

    if (config->active_conversation_id[0] == '\0')
        hermes_app_config_set_active_conversation(config, DEFAULT_CONVERSATION_ID);

    hermes_app_config_touch_recent_conversation(config, config->active_conversation_id);
    clamp_conversation_selection(state, config);
}

static void clear_active_conversation_view(BridgeChatResult* chat_result, char* last_message, size_t last_message_size, size_t* history_scroll)
{
    hermes_app_ui_home_history_reset();
    if (chat_result != NULL)
        bridge_chat_result_reset(chat_result);
    if (last_message != NULL && last_message_size > 0)
        last_message[0] = '\0';
    if (history_scroll != NULL)
        *history_scroll = 0;
}

static void handle_activate_conversation(
    AppConversationState* state,
    HermesAppConfig* config,
    AppScreen* screen,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* history_scroll,
    char* status_line,
    size_t status_line_size
)
{
    if (state == NULL || config == NULL || screen == NULL || chat_result == NULL || last_message == NULL ||
        history_scroll == NULL || status_line == NULL || status_line_size == 0) {
        return;
    }

    if (config->recent_conversation_count == 0) {
        snprintf(status_line, status_line_size, "No saved sessions yet.");
        return;
    }
    if (!hermes_app_config_set_active_conversation(config, config->recent_conversations[state->selection])) {
        snprintf(status_line, status_line_size, "Session selection was invalid.");
        return;
    }
    if (!hermes_app_config_save(config)) {
        snprintf(status_line, status_line_size, "Could not save session selection.");
        return;
    }

    clear_active_conversation_view(chat_result, last_message, last_message_size, history_scroll);
    *screen = APP_SCREEN_HOME;
    snprintf(status_line, status_line_size, "Session %s selected.", config->active_conversation_id);
}

static void handle_create_conversation(
    AppConversationState* state,
    HermesAppConfig* config,
    AppScreen* screen,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* history_scroll,
    char* status_line,
    size_t status_line_size
)
{
    char conversation_id[HERMES_APP_CONVERSATION_ID_MAX];

    if (state == NULL || config == NULL || screen == NULL || chat_result == NULL || last_message == NULL ||
        history_scroll == NULL || status_line == NULL || status_line_size == 0) {
        return;
    }

    if (!prompt_conversation_input("", conversation_id, sizeof(conversation_id))) {
        snprintf(status_line, status_line_size, "New session canceled.");
        return;
    }
    if (!hermes_app_config_is_valid_conversation_id(conversation_id)) {
        snprintf(status_line, status_line_size, "Session IDs only allow letters, numbers, - _ .");
        return;
    }
    if (!hermes_app_config_set_active_conversation(config, conversation_id)) {
        snprintf(status_line, status_line_size, "Could not activate that session.");
        return;
    }
    if (!hermes_app_config_save(config)) {
        snprintf(status_line, status_line_size, "Could not save the new session.");
        return;
    }

    clear_active_conversation_view(chat_result, last_message, last_message_size, history_scroll);
    state->selection = find_recent_conversation_index(config, config->active_conversation_id);
    *screen = APP_SCREEN_HOME;
    snprintf(status_line, status_line_size, "Session %s created.", config->active_conversation_id);
}

static void handle_conversation_sync(
    AppConversationState* state,
    HermesAppConfig* config,
    bool network_ready,
    char* status_line,
    size_t status_line_size,
    Result* request_rc
)
{
    char conversations_url[HERMES_APP_CONVERSATIONS_URL_MAX];
    BridgeV2ConversationListResult fetched_conversations;

    if (state == NULL || config == NULL || status_line == NULL || status_line_size == 0 || request_rc == NULL)
        return;

    bridge_v2_conversation_list_result_reset(&fetched_conversations);
    *request_rc = 0;
    if (!network_ready) {
        snprintf(status_line, status_line_size, "Networking services failed to start.");
        return;
    }
    if (!hermes_app_config_build_conversations_url(config, conversations_url, sizeof(conversations_url)) ||
        config->device_id[0] == '\0') {
        snprintf(status_line, status_line_size, "V2 config is incomplete. Set device_id.");
        return;
    }

    *request_rc = bridge_v2_list_conversations(conversations_url, config->token, config->device_id, &fetched_conversations);
    if (R_SUCCEEDED(*request_rc) && fetched_conversations.success) {
        state->list = fetched_conversations;
        merge_synced_conversations_into_config(state, config);
        if (!hermes_app_config_save(config))
            snprintf(status_line, status_line_size, "Synced sessions, but could not save them.");
        else
            snprintf(status_line, status_line_size, "Synced %lu sessions.", (unsigned long)state->list.count);
    } else if (fetched_conversations.error[0] != '\0') {
        snprintf(status_line, status_line_size, "%s", fetched_conversations.error);
    } else {
        snprintf(status_line, status_line_size, "Session sync failed: 0x%08lX", (unsigned long)*request_rc);
    }
}

void hermes_app_conversations_init(AppConversationState* state)
{
    if (state == NULL)
        return;

    bridge_v2_conversation_list_result_reset(&state->list);
    state->selection = 0;
}

void hermes_app_conversations_refresh_selection_from_active(AppConversationState* state, const HermesAppConfig* config)
{
    if (state == NULL)
        return;

    state->selection = find_recent_conversation_index(config, config != NULL ? config->active_conversation_id : NULL);
    clamp_conversation_selection(state, config);
}

void hermes_app_conversations_open_picker(
    AppConversationState* state,
    const HermesAppConfig* config,
    AppScreen* screen,
    char* status_line,
    size_t status_line_size
)
{
    if (state == NULL || screen == NULL || status_line == NULL || status_line_size == 0)
        return;

    *screen = APP_SCREEN_CONVERSATIONS;
    hermes_app_conversations_refresh_selection_from_active(state, config);
    snprintf(status_line, status_line_size, "Session book opened.");
}

bool hermes_app_conversations_handle_picker_input(
    AppConversationState* state,
    HermesAppConfig* config,
    bool network_ready,
    u32 kDown,
    AppScreen* screen,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* history_scroll,
    char* status_line,
    size_t status_line_size,
    Result* request_rc
)
{
    if (state == NULL || config == NULL || screen == NULL || history_scroll == NULL || status_line == NULL ||
        status_line_size == 0 || request_rc == NULL) {
        return false;
    }

    if ((kDown & KEY_B) != 0) {
        *screen = APP_SCREEN_HOME;
        snprintf(status_line, status_line_size, "Returned home.");
        return true;
    }

    if ((kDown & (KEY_UP | KEY_CPAD_UP)) != 0 && config->recent_conversation_count > 0) {
        if (state->selection == 0)
            state->selection = config->recent_conversation_count - 1;
        else
            state->selection--;
        snprintf(status_line, status_line_size, "Selected %s.", config->recent_conversations[state->selection]);
        return true;
    }

    if ((kDown & (KEY_DOWN | KEY_CPAD_DOWN)) != 0 && config->recent_conversation_count > 0) {
        state->selection = (state->selection + 1) % config->recent_conversation_count;
        snprintf(status_line, status_line_size, "Selected %s.", config->recent_conversations[state->selection]);
        return true;
    }

    if ((kDown & KEY_A) != 0) {
        handle_activate_conversation(
            state,
            config,
            screen,
                chat_result,
                last_message,
                last_message_size,
                history_scroll,
                status_line,
                status_line_size
            );
        return true;
    }

    if ((kDown & KEY_X) != 0) {
        handle_create_conversation(
            state,
            config,
            screen,
                chat_result,
                last_message,
                last_message_size,
                history_scroll,
                status_line,
                status_line_size
            );
        return true;
    }

    if ((kDown & KEY_Y) != 0) {
        handle_conversation_sync(state, config, network_ready, status_line, status_line_size, request_rc);
        return true;
    }

    return false;
}
