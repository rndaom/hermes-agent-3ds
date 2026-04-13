#include "app_requests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_input.h"
#include "voice_input.h"

#define APP_REQUEST_EVENT_POLL_WAIT_MS 5000U
#define APP_REQUEST_REPLY_WAIT_TIMEOUT_MS (60ULL * 60ULL * 1000ULL)

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

static void set_chat_result_error(BridgeChatResult* chat_result, const char* error_message)
{
    if (chat_result == NULL)
        return;

    bridge_chat_result_reset(chat_result);
    snprintf(chat_result->error, sizeof(chat_result->error), "%s", error_message != NULL ? error_message : "Unknown error.");
}

static void render_home_request_ui(
    const HermesAppConfig* config,
    const AppRequestUiContext* ui,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t history_scroll,
    const char* status_line,
    Result request_rc
);

static void reset_chat_request_state(BridgeChatResult* chat_result, BridgeV2MessageResult* message_result, Result* request_rc, size_t* history_scroll)
{
    if (chat_result != NULL)
        bridge_chat_result_reset(chat_result);
    if (message_result != NULL)
        bridge_v2_message_result_reset(message_result);
    if (request_rc != NULL)
        *request_rc = 0;
    if (history_scroll != NULL)
        *history_scroll = 0;
}

static const char* conversation_id_for_message(const HermesAppConfig* config, const BridgeV2MessageResult* message_result)
{
    if (message_result != NULL && message_result->conversation_id[0] != '\0')
        return message_result->conversation_id;
    if (config != NULL && config->active_conversation_id[0] != '\0')
        return config->active_conversation_id;
    return DEFAULT_CONVERSATION_ID;
}

static bool event_is_matching_reply(const BridgeV2EventPollResult* event_result, const BridgeV2MessageResult* message_result)
{
    return event_result != NULL && message_result != NULL &&
        event_result->reply_text[0] != '\0' &&
        strcmp(event_result->reply_to_message_id, message_result->message_id) == 0;
}

static bool event_is_final_reply(const BridgeV2EventPollResult* event_result)
{
    return event_result != NULL && strcmp(event_result->event_type, "message.created") == 0;
}

static void set_chat_result_reply_text(BridgeChatResult* chat_result, const char* reply_text)
{
    if (chat_result == NULL)
        return;

    bridge_chat_result_reset(chat_result);
    if (reply_text == NULL || reply_text[0] == '\0')
        return;

    chat_result->success = true;
    snprintf(chat_result->reply, sizeof(chat_result->reply), "%s", reply_text);
}

static Result poll_for_v2_reply(
    const HermesAppConfig* config,
    const AppRequestUiContext* ui,
    const char* events_url,
    const BridgeV2MessageResult* message_result,
    u32* event_cursor,
    BridgeV2EventPollResult* event_result,
    BridgeV2EventPollResult* matched_event_result,
    bool* matched_reply,
    BridgeChatResult* chat_result,
    const char* last_message,
    char* status_line,
    size_t status_line_size
)
{
    const char* conversation_id;
    Result request_rc = 0;
    u64 started_at_ms;

    if (config == NULL || ui == NULL || events_url == NULL || message_result == NULL || event_cursor == NULL ||
        event_result == NULL || matched_event_result == NULL || matched_reply == NULL || chat_result == NULL) {
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);
    }

    conversation_id = conversation_id_for_message(config, message_result);
    bridge_v2_event_poll_result_reset(event_result);
    bridge_v2_event_poll_result_reset(matched_event_result);
    *matched_reply = false;
    started_at_ms = osGetTime();

    while (aptMainLoop() && (osGetTime() - started_at_ms) < APP_REQUEST_REPLY_WAIT_TIMEOUT_MS) {
        request_rc = bridge_v2_poll_events(
            events_url,
            config->token,
            config->device_id,
            conversation_id,
            *event_cursor,
            APP_REQUEST_EVENT_POLL_WAIT_MS,
            event_result
        );
        if (R_FAILED(request_rc))
            break;

        if (event_result->cursor > *event_cursor)
            *event_cursor = event_result->cursor;

        if (event_result->approval_required)
            break;

        if (event_is_matching_reply(event_result, message_result)) {
            *matched_event_result = *event_result;
            if (strcmp(event_result->event_type, "message.updated") == 0) {
                *matched_reply = false;
                set_chat_result_reply_text(chat_result, event_result->reply_text);
                hermes_app_ui_home_history_upsert(APP_UI_MESSAGE_HERMES, event_result->reply_text);
                if (status_line != NULL && status_line_size > 0)
                    snprintf(status_line, status_line_size, "Hermes is replying...");
                render_home_request_ui(config, ui, chat_result, last_message, 0, status_line, 0);
                continue;
            }

            *matched_reply = event_is_final_reply(event_result);
            if (*matched_reply)
                break;
        }
    }

    if (R_SUCCEEDED(request_rc) && !*matched_reply && !event_result->approval_required) {
        event_result->success = false;
        snprintf(event_result->error, sizeof(event_result->error), "Timed out waiting for Hermes reply.");
    }

    return request_rc;
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

static void render_home_request_ui(
    const HermesAppConfig* config,
    const AppRequestUiContext* ui,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t history_scroll,
    const char* status_line,
    Result request_rc
)
{
    if (ui == NULL || ui->render == NULL)
        return;

    ui->render(
        APP_SCREEN_HOME,
        config,
        ui->selected_field,
        ui->settings_dirty,
        ui->health_result,
        chat_result,
        last_message,
        history_scroll,
        ui->command_selection,
        status_line,
        request_rc
    );
}

static bool prompt_v2_approval_choice(const HermesAppConfig* config, const char* request_id, char* out_choice, size_t out_size)
{
    if (out_choice == NULL || out_size == 0)
        return false;

    out_choice[0] = '\0';

    while (aptMainLoop()) {
        u32 kDown;

        hermes_app_ui_render_approval_prompt(config, request_id);
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

static Result complete_v2_roundtrip(
    const HermesAppConfig* config,
    const AppRequestUiContext* ui,
    const char* events_url,
    const BridgeV2MessageResult* message_result,
    BridgeChatResult* chat_result,
    const char* last_message,
    char* status_line,
    size_t status_line_size,
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

    if (config == NULL || ui == NULL || events_url == NULL || message_result == NULL || chat_result == NULL)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    event_cursor = message_result->cursor;
    request_rc = poll_for_v2_reply(
        config,
        ui,
        events_url,
        message_result,
        &event_cursor,
        &event_result,
        &matched_event_result,
        &matched_reply,
        chat_result,
        last_message,
        status_line,
        status_line_size
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
        if (!prompt_v2_approval_choice(config, event_result.request_id, approval_choice, sizeof(approval_choice))) {
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
            ui,
            events_url,
            message_result,
            &event_cursor,
            &event_result,
            &matched_event_result,
            &matched_reply,
            chat_result,
            last_message,
            status_line,
            status_line_size
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

void hermes_app_requests_handle_text(
    const HermesAppConfig* config,
    bool network_ready,
    const AppRequestUiContext* ui,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* history_scroll,
    char* status_line,
    size_t status_line_size,
    Result* request_rc
)
{
    char message_buffer[BRIDGE_CHAT_MESSAGE_MAX];
    char messages_url[HERMES_APP_MESSAGES_URL_MAX];
    char events_url[HERMES_APP_EVENTS_URL_MAX];
    BridgeV2MessageResult message_result;
    bool missed_events = false;

    if (config == NULL || ui == NULL || chat_result == NULL || last_message == NULL || history_scroll == NULL ||
        status_line == NULL || status_line_size == 0 || request_rc == NULL) {
        return;
    }

    message_buffer[0] = '\0';
    if (!prompt_message_input(message_buffer, sizeof(message_buffer))) {
        snprintf(status_line, status_line_size, "Write note canceled.");
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    copy_bounded_string(last_message, last_message_size, message_buffer);
    hermes_app_ui_home_history_push(APP_UI_MESSAGE_USER, message_buffer);
    reset_chat_request_state(chat_result, &message_result, request_rc, history_scroll);

    if (!hermes_app_config_build_messages_url(config, messages_url, sizeof(messages_url)) ||
        !hermes_app_config_build_events_url(config, events_url, sizeof(events_url)) ||
        config->device_id[0] == '\0') {
        snprintf(status_line, status_line_size, "V2 config is incomplete. Set device_id.");
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    snprintf(status_line, status_line_size, "Sending note to Hermes...");
    render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
    gspWaitForVBlank();

    if (network_ready) {
        *request_rc = bridge_v2_send_message(messages_url, config->token, config->device_id, config->active_conversation_id, message_buffer, &message_result);
        if (R_SUCCEEDED(*request_rc) && message_result.success) {
            *request_rc = complete_v2_roundtrip(
                config,
                ui,
                events_url,
                &message_result,
                chat_result,
                last_message,
                status_line,
                status_line_size,
                &missed_events
            );
            if (R_SUCCEEDED(*request_rc)) {
                if (chat_result->success)
                    hermes_app_ui_home_history_push(APP_UI_MESSAGE_HERMES, chat_result->reply);
                format_roundtrip_status(chat_result, missed_events, "Reply note received over native v2.", status_line, status_line_size);
            }
            else if (chat_result->error[0] != '\0')
                snprintf(status_line, status_line_size, "%s", chat_result->error);
            else
                snprintf(status_line, status_line_size, "Chat failed: 0x%08lX", (unsigned long)*request_rc);
        } else if (message_result.error[0] != '\0') {
            snprintf(status_line, status_line_size, "%s", message_result.error);
        } else {
            snprintf(status_line, status_line_size, "Chat failed: 0x%08lX", (unsigned long)*request_rc);
        }
    } else {
        snprintf(status_line, status_line_size, "Networking services failed to start.");
    }

    render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
}

void hermes_app_requests_handle_voice(
    const HermesAppConfig* config,
    bool network_ready,
    const AppRequestUiContext* ui,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* history_scroll,
    char* status_line,
    size_t status_line_size,
    Result* request_rc
)
{
    char voice_url[HERMES_APP_VOICE_URL_MAX];
    char events_url[HERMES_APP_EVENTS_URL_MAX];
    u8* wav_data = NULL;
    size_t wav_size = 0;
    BridgeV2MessageResult message_result;
    bool missed_events = false;

    if (config == NULL || ui == NULL || chat_result == NULL || last_message == NULL || history_scroll == NULL ||
        status_line == NULL || status_line_size == 0 || request_rc == NULL) {
        return;
    }

    if (!network_ready) {
        snprintf(status_line, status_line_size, "Networking services failed to start.");
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    if (!hermes_app_config_build_voice_url(config, voice_url, sizeof(voice_url)) ||
        !hermes_app_config_build_events_url(config, events_url, sizeof(events_url)) ||
        config->device_id[0] == '\0') {
        snprintf(status_line, status_line_size, "V2 config is incomplete. Set device_id.");
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    if (!voice_input_record_prompt(config, &wav_data, &wav_size, status_line, status_line_size)) {
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    copy_bounded_string(last_message, last_message_size, "[mic] voice input");
    hermes_app_ui_home_history_push(APP_UI_MESSAGE_USER, "[mic] voice input");
    reset_chat_request_state(chat_result, &message_result, request_rc, history_scroll);

    snprintf(status_line, status_line_size, "Sending mic note to Hermes...");
    render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
    gspWaitForVBlank();

    *request_rc = bridge_v2_send_voice_message(voice_url, config->token, config->device_id, config->active_conversation_id, wav_data, wav_size, &message_result);
    free(wav_data);
    wav_data = NULL;

    if (R_SUCCEEDED(*request_rc) && message_result.success) {
        *request_rc = complete_v2_roundtrip(
            config,
            ui,
            events_url,
            &message_result,
            chat_result,
            last_message,
            status_line,
            status_line_size,
            &missed_events
        );
        if (R_SUCCEEDED(*request_rc)) {
            if (chat_result->success)
                hermes_app_ui_home_history_push(APP_UI_MESSAGE_HERMES, chat_result->reply);
            format_roundtrip_status(chat_result, missed_events, "Voice note reply received over native v2.", status_line, status_line_size);
        }
        else if (chat_result->error[0] != '\0')
            snprintf(status_line, status_line_size, "%s", chat_result->error);
        else
            snprintf(status_line, status_line_size, "Voice roundtrip failed: 0x%08lX", (unsigned long)*request_rc);
    } else if (message_result.error[0] != '\0') {
        snprintf(status_line, status_line_size, "%s", message_result.error);
    } else {
        snprintf(status_line, status_line_size, "Voice upload failed: 0x%08lX", (unsigned long)*request_rc);
    }

    render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
}
