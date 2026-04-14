#include "app_requests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_input.h"
#include "picture_input.h"
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

#define INTERACTION_VISIBLE_OPTION_COUNT 8U

static bool event_has_image_media(const BridgeV2EventPollResult* event_result)
{
    return event_result != NULL &&
        event_result->media_id[0] != '\0' &&
        strncmp(event_result->media_type, "image/", 6) == 0;
}

static bool event_has_visible_reply(const BridgeV2EventPollResult* event_result)
{
    return event_result != NULL && (event_result->reply_text[0] != '\0' || event_has_image_media(event_result));
}

static const char* event_history_text(const BridgeV2EventPollResult* event_result)
{
    if (event_result == NULL)
        return "";
    if (event_result->reply_text[0] != '\0')
        return event_result->reply_text;
    if (event_has_image_media(event_result))
        return "[img] Picture note";
    return "";
}

static void apply_event_to_home_history(
    const HermesAppConfig* config,
    const BridgeV2EventPollResult* event_result,
    bool upsert,
    char* status_line,
    size_t status_line_size
)
{
    if (event_result == NULL)
        return;

    if (event_has_image_media(event_result) && config != NULL) {
        char media_url[HERMES_APP_MEDIA_URL_MAX];
        void* media_data = NULL;
        size_t media_size = 0;
        PictureInputPreview preview;

        picture_input_preview_reset(&preview);
        if (hermes_app_config_build_media_url(config, event_result->media_id, media_url, sizeof(media_url)) &&
            R_SUCCEEDED(bridge_v2_download_media(
                media_url,
                config->token,
                &media_data,
                &media_size,
                NULL,
                0,
                status_line,
                status_line_size
            )) && picture_input_decode_bmp_preview(media_data, media_size, &preview, status_line, status_line_size)) {
            if (upsert) {
                hermes_app_ui_home_history_upsert_image(
                    APP_UI_MESSAGE_HERMES,
                    event_history_text(event_result),
                    preview.rgba8_data,
                    preview.width,
                    preview.height
                );
            } else {
                hermes_app_ui_home_history_push_image(
                    APP_UI_MESSAGE_HERMES,
                    event_history_text(event_result),
                    preview.rgba8_data,
                    preview.width,
                    preview.height
                );
            }
            picture_input_preview_free(&preview);
            free(media_data);
            return;
        }

        picture_input_preview_free(&preview);
        if (media_data != NULL)
            free(media_data);
        if (status_line != NULL && status_line_size > 0)
            snprintf(status_line, status_line_size, "Picture preview was unavailable. Showing note text instead.");
    }

    if (event_history_text(event_result)[0] != '\0') {
        if (upsert)
            hermes_app_ui_home_history_upsert(APP_UI_MESSAGE_HERMES, event_history_text(event_result));
        else
            hermes_app_ui_home_history_push(APP_UI_MESSAGE_HERMES, event_history_text(event_result));
    }
}

static bool interaction_choice_is_cancel_like(const char* choice)
{
    return choice != NULL && (
        strcmp(choice, "cancel") == 0 ||
        strcmp(choice, "deny") == 0
    );
}

static int interaction_cancel_choice_index(const BridgeV2InteractionOption* options, size_t option_count)
{
    size_t index;

    if (options == NULL)
        return -1;

    for (index = 0; index < option_count; index++) {
        if (interaction_choice_is_cancel_like(options[index].choice))
            return (int)index;
    }

    return -1;
}

static int interaction_touch_option_index(int px, int py, size_t option_count)
{
    size_t index;

    for (index = 0; index < option_count && index < 8; index++) {
        int x = index % 2 == 0 ? 16 : 168;
        int y = 48 + (int)(index / 2) * 36;

        if (px >= x && px < x + 136 && py >= y && py < y + 28)
            return (int)index;
    }

    return -1;
}

static size_t interaction_page_count(size_t option_count)
{
    return option_count == 0 ? 1 : ((option_count - 1) / INTERACTION_VISIBLE_OPTION_COUNT) + 1;
}

static size_t interaction_page_start(size_t selection)
{
    return (selection / INTERACTION_VISIBLE_OPTION_COUNT) * INTERACTION_VISIBLE_OPTION_COUNT;
}

static size_t move_interaction_selection(size_t current, size_t option_count, int delta_x, int delta_y)
{
    size_t row;
    size_t column;
    size_t next;

    if (option_count == 0)
        return 0;

    row = current / 2;
    column = current % 2;

    if (delta_x < 0 && column > 0)
        column--;
    else if (delta_x > 0 && column < 1)
        column++;

    if (delta_y < 0 && row > 0)
        row--;
    else if (delta_y > 0)
        row++;

    next = row * 2 + column;
    if (next >= option_count) {
        if (delta_y > 0)
            return current;
        if (option_count > 0)
            return option_count - 1;
    }

    return next < option_count ? next : current;
}

static bool prompt_picker_choice(
    const HermesAppConfig* config,
    const char* header,
    const char* title,
    const char* body,
    const BridgeV2InteractionOption* options,
    size_t option_count,
    char* out_choice,
    size_t out_size
)
{
    char hint_line[96];
    size_t selection = 0;
    size_t page_count;
    size_t page_start;
    size_t visible_count;
    int cancel_index;

    if (out_choice == NULL || out_size == 0 || options == NULL || option_count == 0)
        return false;

    out_choice[0] = '\0';
    cancel_index = interaction_cancel_choice_index(options, option_count);
    page_count = interaction_page_count(option_count);

    while (aptMainLoop()) {
        u32 kDown;
        touchPosition touch = {0};
        int touched_index = -1;

        page_start = interaction_page_start(selection);
        visible_count = option_count - page_start;
        if (visible_count > INTERACTION_VISIBLE_OPTION_COUNT)
            visible_count = INTERACTION_VISIBLE_OPTION_COUNT;

        if (page_count > 1) {
            snprintf(
                hint_line,
                sizeof(hint_line),
                "Use Pad/touch. L/R page %lu/%lu. A choose, B cancel.",
                (unsigned long)((page_start / INTERACTION_VISIBLE_OPTION_COUNT) + 1),
                (unsigned long)page_count
            );
        } else {
            snprintf(hint_line, sizeof(hint_line), "Use the D-pad, touch, or A/B.");
        }

        hermes_app_ui_render_interaction_prompt(
            config,
            header,
            title,
            body,
            options + page_start,
            visible_count,
            selection - page_start,
            hint_line
        );
        gspWaitForVBlank();
        hidScanInput();
        kDown = hidKeysDown();

        if ((kDown & KEY_L) != 0 && page_start >= INTERACTION_VISIBLE_OPTION_COUNT) {
            selection = page_start - INTERACTION_VISIBLE_OPTION_COUNT;
        } else if ((kDown & KEY_R) != 0 && page_start + visible_count < option_count) {
            selection = page_start + INTERACTION_VISIBLE_OPTION_COUNT;
            if (selection >= option_count)
                selection = option_count - 1;
        } else if ((kDown & (KEY_UP | KEY_CPAD_UP)) != 0) {
            selection = move_interaction_selection(selection, option_count, 0, -1);
        } else if ((kDown & (KEY_DOWN | KEY_CPAD_DOWN)) != 0) {
            selection = move_interaction_selection(selection, option_count, 0, 1);
        } else if ((kDown & KEY_LEFT) != 0) {
            selection = move_interaction_selection(selection, option_count, -1, 0);
        } else if ((kDown & KEY_RIGHT) != 0) {
            selection = move_interaction_selection(selection, option_count, 1, 0);
        } else if ((kDown & KEY_TOUCH) != 0) {
            hidTouchRead(&touch);
            touched_index = interaction_touch_option_index((int)touch.px, (int)touch.py, visible_count);
            if (touched_index >= 0)
                selection = page_start + (size_t)touched_index;
        }

        if ((kDown & KEY_A) != 0 || ((kDown & KEY_TOUCH) != 0 && touched_index >= 0)) {
            snprintf(out_choice, out_size, "%s", options[selection].choice);
            return true;
        }

        if (((kDown & KEY_B) != 0 || (kDown & KEY_START) != 0) && cancel_index >= 0) {
            snprintf(out_choice, out_size, "%s", options[cancel_index].choice);
            return true;
        }
        if ((kDown & KEY_B) != 0)
            return false;
        if ((kDown & KEY_START) != 0)
            return false;
    }

    return false;
}

static bool prompt_v2_interaction_choice(
    const HermesAppConfig* config,
    const BridgeV2EventPollResult* event_result,
    char* out_choice,
    size_t out_size
)
{
    if (event_result == NULL)
        return false;

    return prompt_picker_choice(
        config,
        "CHOICE",
        event_result->interaction_title,
        event_result->interaction_text,
        event_result->interaction_options,
        event_result->interaction_option_count,
        out_choice,
        out_size
    );
}

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
        event_has_visible_reply(event_result) &&
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

        if (event_result->approval_required || event_result->interaction_required)
            break;

        if (strcmp(event_result->event_type, "status.updated") == 0 && event_result->reply_text[0] != '\0') {
            if (status_line != NULL && status_line_size > 0)
                snprintf(status_line, status_line_size, "%s", event_result->reply_text);
            render_home_request_ui(config, ui, chat_result, last_message, 0, status_line, 0);
            continue;
        }

        if (event_is_matching_reply(event_result, message_result)) {
            *matched_event_result = *event_result;
            if (strcmp(event_result->event_type, "message.updated") == 0) {
                *matched_reply = false;
                set_chat_result_reply_text(chat_result, event_result->reply_text);
                apply_event_to_home_history(config, event_result, true, status_line, status_line_size);
                if (status_line != NULL && status_line_size > 0)
                    snprintf(status_line, status_line_size, "(^.^) Hermes is replying...");
                render_home_request_ui(config, ui, chat_result, last_message, 0, status_line, 0);
                continue;
            }

            *matched_reply = event_is_final_reply(event_result);
            if (*matched_reply)
                break;
        }
    }

    if (R_SUCCEEDED(request_rc) && !*matched_reply && !event_result->approval_required && !event_result->interaction_required) {
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

    if (event_result->interaction_required) {
        snprintf(chat_result->error, sizeof(chat_result->error), "Interaction required. Use the bottom screen picker.");
        return;
    }

    if (!event_has_visible_reply(event_result)) {
        snprintf(chat_result->error, sizeof(chat_result->error), "No assistant reply arrived yet.");
        return;
    }

    chat_result->success = true;
    if (event_result->reply_text[0] != '\0')
        snprintf(chat_result->reply, sizeof(chat_result->reply), "%s", event_result->reply_text);
    else
        snprintf(chat_result->reply, sizeof(chat_result->reply), "%s", "[img] Hermes sent a picture.");
}

static void drain_matching_reply_events(
    const HermesAppConfig* config,
    const AppRequestUiContext* ui,
    const char* events_url,
    const BridgeV2MessageResult* message_result,
    u32* event_cursor,
    BridgeChatResult* chat_result,
    const char* last_message,
    char* status_line,
    size_t status_line_size,
    bool* missed_events_out
)
{
    u64 deadline_ms;

    if (config == NULL || ui == NULL || events_url == NULL || message_result == NULL || event_cursor == NULL)
        return;

    deadline_ms = osGetTime() + 600ULL;
    while (aptMainLoop() && osGetTime() < deadline_ms) {
        BridgeV2EventPollResult extra_event;
        Result extra_rc;

        bridge_v2_event_poll_result_reset(&extra_event);
        extra_rc = bridge_v2_poll_events(
            events_url,
            config->token,
            config->device_id,
            config->active_conversation_id,
            *event_cursor,
            150,
            &extra_event
        );
        if (R_FAILED(extra_rc))
            return;
        if (extra_event.cursor > *event_cursor)
            *event_cursor = extra_event.cursor;
        if (extra_event.missed_events && missed_events_out != NULL)
            *missed_events_out = true;
        if (extra_event.event_type[0] == '\0' || !event_is_matching_reply(&extra_event, message_result))
            return;

        apply_event_to_home_history(
            config,
            &extra_event,
            strcmp(extra_event.event_type, "message.updated") == 0,
            status_line,
            status_line_size
        );
        render_home_request_ui(config, ui, chat_result, last_message, 0, status_line, 0);
    }
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
    char interaction_choice[BRIDGE_V2_INTERACTION_CHOICE_MAX];
    BridgeV2EventPollResult* event_result = NULL;
    BridgeV2EventPollResult* matched_event_result = NULL;
    BridgeV2InteractionResult interaction_result;
    bool matched_reply = false;
    bool missed_events = false;
    u32 event_cursor;
    Result request_rc = 0;

    if (missed_events_out != NULL)
        *missed_events_out = false;

    if (config == NULL || ui == NULL || events_url == NULL || message_result == NULL || chat_result == NULL)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    event_result = (BridgeV2EventPollResult*)malloc(sizeof(*event_result));
    matched_event_result = (BridgeV2EventPollResult*)malloc(sizeof(*matched_event_result));
    if (event_result == NULL || matched_event_result == NULL) {
        free(event_result);
        free(matched_event_result);
        set_chat_result_error(chat_result, "Could not allocate reply buffers.");
        return MAKERESULT(RL_FATAL, RS_OUTOFRESOURCE, RM_APPLICATION, RD_OUT_OF_MEMORY);
    }

    bridge_v2_event_poll_result_reset(event_result);
    bridge_v2_event_poll_result_reset(matched_event_result);

    event_cursor = message_result->cursor;
    while (aptMainLoop()) {
        request_rc = poll_for_v2_reply(
            config,
            ui,
            events_url,
            message_result,
            &event_cursor,
            event_result,
            matched_event_result,
            &matched_reply,
            chat_result,
            last_message,
            status_line,
            status_line_size
        );
        if (R_FAILED(request_rc)) {
            if (event_result->error[0] != '\0')
                set_chat_result_error(chat_result, event_result->error);
            goto done;
        }

        if (event_result->missed_events)
            missed_events = true;

        if (matched_reply)
            break;

        if (!event_result->approval_required && !event_result->interaction_required)
            break;

        interaction_choice[0] = '\0';
        bridge_v2_interaction_result_reset(&interaction_result);

        if (!hermes_app_config_build_interaction_url(config, event_result->request_id, interaction_url, sizeof(interaction_url))) {
            set_chat_result_error(chat_result, event_result->approval_required ? "Approval URL could not be built." : "Interaction URL could not be built.");
            goto done;
        }

        if (event_result->approval_required) {
            if (!prompt_v2_approval_choice(config, event_result->request_id, interaction_choice, sizeof(interaction_choice))) {
                set_chat_result_error(chat_result, "Approval canceled.");
                goto done;
            }
        } else {
            if (!prompt_v2_interaction_choice(config, event_result, interaction_choice, sizeof(interaction_choice))) {
                set_chat_result_error(chat_result, "Interaction canceled.");
                goto done;
            }
        }

        request_rc = bridge_v2_submit_interaction(interaction_url, config->token, interaction_choice, &interaction_result);
        if (R_FAILED(request_rc) || !interaction_result.success) {
            set_chat_result_error(
                chat_result,
                interaction_result.error[0] != '\0' ? interaction_result.error : "Interaction response failed."
            );
            goto done;
        }
        if (strcmp(interaction_choice, "deny") == 0) {
            set_chat_result_error(chat_result, "Command denied.");
            request_rc = 0;
            goto done;
        }
        if (strcmp(interaction_choice, "cancel") == 0) {
            set_chat_result_error(chat_result, "Command canceled.");
            request_rc = 0;
            goto done;
        }
    }

    apply_v2_event_to_chat_result(matched_reply ? matched_event_result : event_result, chat_result);
    apply_event_to_home_history(config, matched_reply ? matched_event_result : event_result, true, status_line, status_line_size);
    drain_matching_reply_events(
        config,
        ui,
        events_url,
        message_result,
        &event_cursor,
        chat_result,
        last_message,
        status_line,
        status_line_size,
        &missed_events
    );

done:
    free(matched_event_result);
    free(event_result);
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

static bool text_is_fresh_session_command(const char* text)
{
    size_t prefix_length;

    if (text == NULL || text[0] != '/')
        return false;

    if (strncmp(text, "/reset", 6) == 0) {
        prefix_length = 6;
    } else if (strncmp(text, "/new", 4) == 0) {
        prefix_length = 4;
    } else {
        return false;
    }

    return text[prefix_length] == '\0' || text[prefix_length] == ' ';
}

static void submit_hidden_reset_command(
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
    char messages_url[HERMES_APP_MESSAGES_URL_MAX];
    char events_url[HERMES_APP_EVENTS_URL_MAX];
    BridgeV2MessageResult message_result;
    bool missed_events = false;

    if (config == NULL || ui == NULL || chat_result == NULL || last_message == NULL || history_scroll == NULL ||
        status_line == NULL || status_line_size == 0 || request_rc == NULL) {
        return;
    }

    hermes_app_ui_home_history_reset();
    bridge_chat_result_reset(chat_result);
    last_message[0] = '\0';
    *history_scroll = 0;
    *request_rc = 0;

    if (!hermes_app_config_build_messages_url(config, messages_url, sizeof(messages_url)) ||
        !hermes_app_config_build_events_url(config, events_url, sizeof(events_url)) ||
        config->device_id[0] == '\0') {
        snprintf(status_line, status_line_size, "Session cleared locally. Device ID is still missing.");
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    snprintf(status_line, status_line_size, "(^_^) Clearing the board...");
    render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
    gspWaitForVBlank();

    if (!network_ready) {
        snprintf(status_line, status_line_size, "Board cleared locally. Network is offline, so Hermes could not reset the session.");
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    *request_rc = bridge_v2_send_message(messages_url, config->token, config->device_id, config->active_conversation_id, "/reset", &message_result);
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
        bridge_chat_result_reset(chat_result);
        last_message[0] = '\0';
        if (R_SUCCEEDED(*request_rc)) {
            if (missed_events)
                snprintf(status_line, status_line_size, "Board cleared. Fresh session ready (events trimmed).");
            else
                snprintf(status_line, status_line_size, "Board cleared. Fresh session ready.");
        } else if (chat_result->error[0] != '\0') {
            snprintf(status_line, status_line_size, "%s", chat_result->error);
        } else {
            snprintf(status_line, status_line_size, "Session clear failed: 0x%08lX", (unsigned long)*request_rc);
        }
    } else if (message_result.error[0] != '\0') {
        snprintf(status_line, status_line_size, "%s", message_result.error);
    } else {
        snprintf(status_line, status_line_size, "Session clear failed: 0x%08lX", (unsigned long)*request_rc);
    }

    render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
}

static void submit_text_message(
    const HermesAppConfig* config,
    bool network_ready,
    const AppRequestUiContext* ui,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* history_scroll,
    char* status_line,
    size_t status_line_size,
    Result* request_rc,
    const char* message_buffer
)
{
    char messages_url[HERMES_APP_MESSAGES_URL_MAX];
    char events_url[HERMES_APP_EVENTS_URL_MAX];
    BridgeV2MessageResult message_result;
    bool missed_events = false;
    bool fresh_session_command;

    if (config == NULL || ui == NULL || chat_result == NULL || last_message == NULL || history_scroll == NULL ||
        status_line == NULL || status_line_size == 0 || request_rc == NULL || message_buffer == NULL || message_buffer[0] == '\0') {
        return;
    }

    fresh_session_command = text_is_fresh_session_command(message_buffer);
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

    snprintf(status_line, status_line_size, "(^_^) Sending note to Hermes...");
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
                if (fresh_session_command) {
                    hermes_app_ui_home_history_reset();
                    *history_scroll = 0;
                    hermes_app_ui_home_history_push(APP_UI_MESSAGE_USER, message_buffer);
                    if (chat_result->success)
                        hermes_app_ui_home_history_push(APP_UI_MESSAGE_HERMES, chat_result->reply);
                }
                if (chat_result->success && message_buffer[0] == '/')
                    *history_scroll = hermes_app_ui_home_history_max_scroll(status_line);
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

static void submit_picked_command(
    const HermesAppConfig* config,
    bool network_ready,
    const AppRequestUiContext* ui,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* history_scroll,
    char* status_line,
    size_t status_line_size,
    Result* request_rc,
    const char* command_name,
    const char* command_choice
)
{
    char command_buffer[BRIDGE_CHAT_MESSAGE_MAX];

    if (command_name == NULL || command_name[0] == '\0' || command_choice == NULL || command_choice[0] == '\0')
        return;

    snprintf(command_buffer, sizeof(command_buffer), "/%s %s", command_name, command_choice);
    submit_text_message(
        config,
        network_ready,
        ui,
        chat_result,
        last_message,
        last_message_size,
        history_scroll,
        status_line,
        status_line_size,
        request_rc,
        command_buffer
    );
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

    submit_text_message(
        config,
        network_ready,
        ui,
        chat_result,
        last_message,
        last_message_size,
        history_scroll,
        status_line,
        status_line_size,
        request_rc,
        message_buffer
    );
}

void hermes_app_requests_handle_command(
    const HermesAppConfig* config,
    bool network_ready,
    const AppRequestUiContext* ui,
    BridgeChatResult* chat_result,
    char* last_message,
    size_t last_message_size,
    size_t* history_scroll,
    char* status_line,
    size_t status_line_size,
    Result* request_rc,
    const char* command_text
)
{
    submit_text_message(
        config,
        network_ready,
        ui,
        chat_result,
        last_message,
        last_message_size,
        history_scroll,
        status_line,
        status_line_size,
        request_rc,
        command_text
    );
}

void hermes_app_requests_handle_reasoning_command(
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
    static const BridgeV2InteractionOption reasoning_options[] = {
        {"none", "None"},
        {"minimal", "Minimal"},
        {"low", "Low"},
        {"medium", "Medium"},
        {"high", "High"},
        {"xhigh", "XHigh"},
        {"show", "Show"},
        {"hide", "Hide"},
    };
    char choice[BRIDGE_V2_INTERACTION_CHOICE_MAX];

    if (!prompt_picker_choice(
            config,
            "REASONING",
            "Reasoning settings",
            "Choose a reasoning effort level, or toggle whether Hermes shows its thinking before replies.",
            reasoning_options,
            sizeof(reasoning_options) / sizeof(reasoning_options[0]),
            choice,
            sizeof(choice)
        )) {
        snprintf(status_line, status_line_size, "Reasoning change canceled.");
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    submit_picked_command(
        config,
        network_ready,
        ui,
        chat_result,
        last_message,
        last_message_size,
        history_scroll,
        status_line,
        status_line_size,
        request_rc,
        "reasoning",
        choice
    );
}

void hermes_app_requests_handle_fast_command(
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
    static const BridgeV2InteractionOption fast_options[] = {
        {"fast", "Fast"},
        {"normal", "Normal"},
        {"status", "Status"},
    };
    char choice[BRIDGE_V2_INTERACTION_CHOICE_MAX];

    if (!prompt_picker_choice(
            config,
            "PRIORITY",
            "Priority processing",
            "Choose fast mode, return to normal mode, or just ask Hermes for the current Priority Processing state.",
            fast_options,
            sizeof(fast_options) / sizeof(fast_options[0]),
            choice,
            sizeof(choice)
        )) {
        snprintf(status_line, status_line_size, "Priority Processing change canceled.");
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    submit_picked_command(
        config,
        network_ready,
        ui,
        chat_result,
        last_message,
        last_message_size,
        history_scroll,
        status_line,
        status_line_size,
        request_rc,
        "fast",
        choice
    );
}

void hermes_app_requests_handle_clear_command(
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
    submit_hidden_reset_command(
        config,
        network_ready,
        ui,
        chat_result,
        last_message,
        last_message_size,
        history_scroll,
        status_line,
        status_line_size,
        request_rc
    );
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

    snprintf(status_line, status_line_size, "(^_^) Sending mic note to Hermes...");
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

void hermes_app_requests_handle_picture(
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
    char image_url[HERMES_APP_IMAGE_URL_MAX];
    char events_url[HERMES_APP_EVENTS_URL_MAX];
    u8* bmp_data = NULL;
    size_t bmp_size = 0;
    PictureInputPreview preview;
    BridgeV2MessageResult message_result;
    bool missed_events = false;

    picture_input_preview_reset(&preview);
    if (config == NULL || ui == NULL || chat_result == NULL || last_message == NULL || history_scroll == NULL ||
        status_line == NULL || status_line_size == 0 || request_rc == NULL) {
        return;
    }

    if (!network_ready) {
        snprintf(status_line, status_line_size, "Networking services failed to start.");
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    if (!hermes_app_config_build_image_url(config, image_url, sizeof(image_url)) ||
        !hermes_app_config_build_events_url(config, events_url, sizeof(events_url)) ||
        config->device_id[0] == '\0') {
        snprintf(status_line, status_line_size, "V2 config is incomplete. Set device_id.");
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    if (!picture_input_capture_prompt(config, &bmp_data, &bmp_size, &preview, status_line, status_line_size)) {
        render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
        return;
    }

    copy_bounded_string(last_message, last_message_size, "[cam] picture input");
    hermes_app_ui_home_history_push_image(APP_UI_MESSAGE_USER, "[cam] picture input", preview.rgba8_data, preview.width, preview.height);
    picture_input_preview_free(&preview);
    reset_chat_request_state(chat_result, &message_result, request_rc, history_scroll);

    snprintf(status_line, status_line_size, "(^_^) Sending picture note to Hermes...");
    render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
    gspWaitForVBlank();

    *request_rc = bridge_v2_send_image_message(
        image_url,
        config->token,
        config->device_id,
        config->active_conversation_id,
        bmp_data,
        bmp_size,
        "image/bmp",
        &message_result
    );
    free(bmp_data);
    bmp_data = NULL;

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
        if (R_SUCCEEDED(*request_rc))
            format_roundtrip_status(chat_result, missed_events, "Picture reply received over native v2.", status_line, status_line_size);
        else if (chat_result->error[0] != '\0')
            snprintf(status_line, status_line_size, "%s", chat_result->error);
        else
            snprintf(status_line, status_line_size, "Picture roundtrip failed: 0x%08lX", (unsigned long)*request_rc);
    } else if (message_result.error[0] != '\0') {
        snprintf(status_line, status_line_size, "%s", message_result.error);
    } else {
        snprintf(status_line, status_line_size, "Picture upload failed: 0x%08lX", (unsigned long)*request_rc);
    }

    render_home_request_ui(config, ui, chat_result, last_message, *history_scroll, status_line, *request_rc);
}
