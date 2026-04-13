#include "app_ui.h"
#include "app_gfx.h"
#include "app_home.h"

#include <stdio.h>
#include <string.h>

#define HOME_WRAP_MAX_LINES 128
#define HOME_WRAP_LINE_MAX 192
#define HOME_MESSAGE_WRAP_MAX_LINES 16
#define HOME_SUMMARY_WRAP_MAX_LINES 4
#define HOME_INFO_WRAP_MAX_LINES 8
#define HOME_MESSAGE_LINES_PER_PAGE 2
#define HOME_REPLY_LINES_PER_PAGE 6
#define HOME_MESSAGE_MAX_WIDTH 348.0f
#define HOME_REPLY_MAX_WIDTH 348.0f
#define HOME_MESSAGE_SCALE 0.30f
#define HOME_REPLY_SCALE 0.32f

static char g_reply_page_lines[HOME_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];
static char g_home_message_lines[HOME_MESSAGE_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];
static char g_home_reply_lines[HOME_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];
static char g_home_room_lines[HOME_SUMMARY_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];
static char g_home_status_lines[HOME_SUMMARY_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];
static char g_home_token_lines[HOME_SUMMARY_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];
static char g_home_detail_lines[HOME_INFO_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];

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

static void format_token_summary(const HermesAppConfig* config, char* out, size_t out_size)
{
    size_t token_length;

    if (config == NULL || out == NULL || out_size == 0)
        return;

    token_length = strlen(config->token);
    if (token_length == 0)
        snprintf(out, out_size, "<empty>");
    else
        snprintf(out, out_size, "configured (%lu chars)", (unsigned long)token_length);
}

static size_t utf8_prefix_boundary(const char* text, size_t max_bytes)
{
    size_t index = 0;
    size_t last_valid = 0;

    while (text != NULL && text[index] != '\0' && index < max_bytes) {
        unsigned char byte = (unsigned char)text[index];
        size_t codepoint_len = 1;
        size_t continuation_index;
        bool valid = true;

        if ((byte & 0x80U) == 0x00U) {
            codepoint_len = 1;
        } else if ((byte & 0xE0U) == 0xC0U) {
            codepoint_len = 2;
        } else if ((byte & 0xF0U) == 0xE0U) {
            codepoint_len = 3;
        } else if ((byte & 0xF8U) == 0xF0U) {
            codepoint_len = 4;
        } else {
            index++;
            last_valid = index;
            continue;
        }

        if (index + codepoint_len > max_bytes)
            break;

        for (continuation_index = 1; continuation_index < codepoint_len; continuation_index++) {
            unsigned char continuation = (unsigned char)text[index + continuation_index];
            if (continuation == '\0' || (continuation & 0xC0U) != 0x80U) {
                valid = false;
                break;
            }
        }

        if (!valid) {
            index++;
            last_valid = index;
            continue;
        }

        index += codepoint_len;
        last_valid = index;
    }

    return last_valid;
}

static size_t fit_prefix_to_width(const char* text, size_t max_bytes, float max_width, float scale_x, float scale_y)
{
    char candidate[HOME_WRAP_LINE_MAX];
    float width = 0.0f;
    float height = 0.0f;
    size_t low = 0;
    size_t high;
    size_t best = 0;

    if (text == NULL || text[0] == '\0' || max_bytes == 0)
        return 0;

    high = max_bytes;
    if (high >= sizeof(candidate))
        high = sizeof(candidate) - 1;

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        size_t keep = utf8_prefix_boundary(text, mid);

        if (keep == 0 && mid > 0)
            keep = utf8_prefix_boundary(text, 1);

        memcpy(candidate, text, keep);
        candidate[keep] = '\0';
        if (app_gfx_measure_text(candidate, scale_x, scale_y, &width, &height) && width <= max_width) {
            best = keep;
            low = mid + 1;
        } else {
            if (mid == 0)
                break;
            high = mid - 1;
        }
    }

    return best;
}

static void append_truncation_ellipsis(char* line, float max_width, float scale_x, float scale_y)
{
    char candidate[HOME_WRAP_LINE_MAX];
    size_t length;
    size_t keep;
    float width = 0.0f;
    float height = 0.0f;

    if (line == NULL)
        return;

    length = strlen(line);
    if (length == 0) {
        copy_bounded_string(line, HOME_WRAP_LINE_MAX, "...");
        return;
    }

    candidate[0] = '\0';
    copy_bounded_string(candidate, sizeof(candidate), line);
    strncat(candidate, "...", sizeof(candidate) - strlen(candidate) - 1);
    if (app_gfx_measure_text(candidate, scale_x, scale_y, &width, &height) && width <= max_width) {
        copy_bounded_string(line, HOME_WRAP_LINE_MAX, candidate);
        return;
    }

    keep = fit_prefix_to_width(line, length, max_width, scale_x, scale_y);
    while (keep > 0) {
        memcpy(candidate, line, keep);
        candidate[keep] = '\0';
        strncat(candidate, "...", sizeof(candidate) - strlen(candidate) - 1);
        if (app_gfx_measure_text(candidate, scale_x, scale_y, &width, &height) && width <= max_width) {
            copy_bounded_string(line, HOME_WRAP_LINE_MAX, candidate);
            return;
        }
        keep = utf8_prefix_boundary(line, keep - 1);
    }

    copy_bounded_string(line, HOME_WRAP_LINE_MAX, "...");
}

static size_t wrap_text_for_pixels(
    const char* input,
    float max_width,
    float scale_x,
    float scale_y,
    char lines[][HOME_WRAP_LINE_MAX],
    size_t max_lines
)
{
    const char* cursor;
    size_t line_count = 0;

    if (lines == NULL || max_lines == 0)
        return 0;

    if (input == NULL || input[0] == '\0') {
        copy_bounded_string(lines[0], HOME_WRAP_LINE_MAX, "<none>");
        return 1;
    }

    cursor = input;
    while (*cursor != '\0' && line_count < max_lines) {
        char line[HOME_WRAP_LINE_MAX];
        size_t line_len = 0;

        line[0] = '\0';
        if (*cursor == '\n') {
            lines[line_count][0] = '\0';
            line_count++;
            cursor++;
            continue;
        }

        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r')
            cursor++;

        while (*cursor != '\0' && *cursor != '\n') {
            const char* word_start;
            size_t word_len = 0;
            char candidate[HOME_WRAP_LINE_MAX];
            float width = 0.0f;
            float height = 0.0f;

            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r')
                cursor++;
            if (*cursor == '\0' || *cursor == '\n')
                break;

            word_start = cursor;
            while (word_start[word_len] != '\0' &&
                   word_start[word_len] != ' ' &&
                   word_start[word_len] != '\t' &&
                   word_start[word_len] != '\r' &&
                   word_start[word_len] != '\n')
                word_len++;

            if (line_len == 0) {
                size_t keep = fit_prefix_to_width(word_start, word_len, max_width, scale_x, scale_y);
                if (keep == 0)
                    keep = utf8_prefix_boundary(word_start, word_len > 0 ? 1 : 0);
                if (keep == 0)
                    break;
                memcpy(line, word_start, keep);
                line[keep] = '\0';
                line_len = keep;
                cursor = word_start + keep;
                if (keep < word_len)
                    break;
                continue;
            }

            snprintf(candidate, sizeof(candidate), "%s %.*s", line, (int)word_len, word_start);
            if (app_gfx_measure_text(candidate, scale_x, scale_y, &width, &height) && width <= max_width) {
                copy_bounded_string(line, sizeof(line), candidate);
                line_len = strlen(line);
                cursor = word_start + word_len;
                continue;
            }

            break;
        }

        if (line_len == 0) {
            line[0] = '\0';
            if (*cursor == '\n')
                cursor++;
        }

        copy_bounded_string(lines[line_count], HOME_WRAP_LINE_MAX, line);
        line_count++;

        if (*cursor == '\n')
            cursor++;
    }

    if (*cursor != '\0' && line_count > 0)
        append_truncation_ellipsis(lines[line_count - 1], max_width, scale_x, scale_y);

    if (line_count == 0) {
        copy_bounded_string(lines[0], HOME_WRAP_LINE_MAX, "<none>");
        return 1;
    }

    return line_count;
}

static size_t page_count_for_lines(size_t total_lines, size_t lines_per_page)
{
    if (total_lines == 0 || lines_per_page == 0)
        return 1;

    return (total_lines + lines_per_page - 1) / lines_per_page;
}

size_t hermes_app_ui_reply_page_count(const char* reply_text)
{
    size_t total_lines = wrap_text_for_pixels(
        reply_text,
        HOME_REPLY_MAX_WIDTH,
        HOME_REPLY_SCALE,
        HOME_REPLY_SCALE,
        g_reply_page_lines,
        HOME_WRAP_MAX_LINES
    );
    return page_count_for_lines(total_lines, HOME_REPLY_LINES_PER_PAGE);
}

static const BridgeV2ConversationInfo* find_synced_conversation(
    const BridgeV2ConversationListResult* conversation_list,
    const char* conversation_id
)
{
    size_t index;

    if (conversation_list == NULL || conversation_id == NULL || conversation_id[0] == '\0')
        return NULL;

    for (index = 0; index < conversation_list->count; index++) {
        if (strcmp(conversation_list->conversations[index].conversation_id, conversation_id) == 0)
            return &conversation_list->conversations[index];
    }

    return NULL;
}

static void format_active_conversation_label(
    const HermesAppConfig* config,
    const BridgeV2ConversationListResult* conversation_list,
    char* out_label,
    size_t out_size
)
{
    const BridgeV2ConversationInfo* info;

    if (out_label == NULL || out_size == 0)
        return;

    out_label[0] = '\0';
    if (config == NULL || config->active_conversation_id[0] == '\0') {
        snprintf(out_label, out_size, DEFAULT_CONVERSATION_ID);
        return;
    }

    info = find_synced_conversation(conversation_list, config->active_conversation_id);
    if (info != NULL && info->title[0] != '\0')
        snprintf(out_label, out_size, "%s", info->title);
    else
        snprintf(out_label, out_size, "%s", config->active_conversation_id);
}

#define UI_BG_TOP C2D_Color32(0x1B, 0x1F, 0x29, 0xFF)
#define UI_BG_BOTTOM C2D_Color32(0x20, 0x25, 0x32, 0xFF)
#define UI_PANEL C2D_Color32(0x24, 0x2A, 0x37, 0xFF)
#define UI_PANEL_ALT C2D_Color32(0x2D, 0x34, 0x45, 0xFF)
#define UI_REPLY_CARD C2D_Color32(0x1F, 0x24, 0x31, 0xFF)
#define UI_BORDER C2D_Color32(0x55, 0x61, 0x7A, 0xFF)
#define UI_BORDER_STRONG C2D_Color32(0x93, 0xA1, 0xBB, 0xFF)
#define UI_ACCENT C2D_Color32(0x5C, 0x83, 0xB1, 0xFF)
#define UI_ACCENT_HI C2D_Color32(0x82, 0xA8, 0xD6, 0xFF)
#define UI_SIGNAL C2D_Color32(0x62, 0xD1, 0xCF, 0xFF)
#define UI_SIGNAL_HI C2D_Color32(0x9D, 0xE8, 0xE4, 0xFF)
#define UI_TEXT C2D_Color32(0xF1, 0xF3, 0xF7, 0xFF)
#define UI_MUTED C2D_Color32(0xC5, 0xCF, 0xDB, 0xFF)
#define UI_META C2D_Color32(0x8B, 0x97, 0xAB, 0xFF)

bool hermes_app_ui_init(void)
{
    return app_gfx_init();
}

void hermes_app_ui_exit(void)
{
    app_gfx_fini();
}

static void draw_header(const char* title, const char* subtitle)
{
    app_gfx_panel_inset(10.0f, 10.0f, 380.0f, 28.0f, UI_PANEL_ALT, UI_BORDER, UI_ACCENT);
    app_gfx_text(18.0f, 16.0f, 0.55f, 0.55f, UI_TEXT, title);
    app_gfx_text_right(382.0f, 16.0f, 0.42f, 0.42f, UI_SIGNAL_HI, subtitle);
}

static void draw_bottom_header(const char* title)
{
    app_gfx_panel_inset(8.0f, 10.0f, 304.0f, 28.0f, UI_PANEL_ALT, UI_BORDER, UI_ACCENT);
    app_gfx_text(16.0f, 16.0f, 0.50f, 0.50f, UI_TEXT, title);
}

static void draw_chip(float x, float y, float w, const char* label, u32 fill, u32 text_color)
{
    app_gfx_panel(x, y, w, 16.0f, fill, UI_BORDER_STRONG);
    app_gfx_text_fit(x + 6.0f, y + 3.0f, w - 12.0f, 0.30f, 0.30f, text_color, label);
}

static void draw_note_lines(float x, float y, float w, size_t count)
{
    size_t index;

    for (index = 0; index < count; index++)
        app_gfx_highlight_bar(x, y + (float)index * 10.0f, w, 1.0f, UI_BORDER);
}

static void draw_text_lines(
    float x,
    float y,
    float line_height,
    float max_width,
    float scale_x,
    float scale_y,
    u32 color,
    char lines[][HOME_WRAP_LINE_MAX],
    size_t total_lines,
    size_t start,
    size_t max_lines
)
{
    size_t index;

    for (index = 0; index < max_lines && start + index < total_lines; index++)
        app_gfx_text_fit(x, y + (float)index * line_height, max_width, scale_x, scale_y, color, lines[start + index]);
}

static void draw_menu_row(float x, float y, float w, const char* label, bool selected)
{
    if (selected)
        app_gfx_highlight_bar(x, y, w, 16.0f, UI_ACCENT);
    app_gfx_text_fit(x + 8.0f, y + 3.0f, w - 16.0f, 0.43f, 0.43f, selected ? UI_BG_TOP : UI_TEXT, label);
}

static const char* home_command_label_for_selection(size_t command_selection)
{
    switch ((HomeCommand)command_selection) {
        case HOME_COMMAND_CHECK:
            return "Check Link";
        case HOME_COMMAND_THREADS:
            return "Conversations";
        case HOME_COMMAND_CONFIG:
            return "Settings";
        case HOME_COMMAND_MIC:
            return "Mic Input";
        case HOME_COMMAND_CLEAR:
            return "Clear Reply";
        case HOME_COMMAND_EXIT:
            return "Exit";
        case HOME_COMMAND_NONE:
        case HOME_COMMAND_ASK:
        default:
            return "Ask Hermes";
    }
}

static const char* home_command_detail_for_selection(size_t command_selection)
{
    switch ((HomeCommand)command_selection) {
        case HOME_COMMAND_CHECK:
            return "Run a health check against the configured Hermes gateway and refresh the link status.";
        case HOME_COMMAND_THREADS:
            return "Open the conversation list so you can switch threads, create one, or sync recent entries.";
        case HOME_COMMAND_CONFIG:
            return "Open the settings screen to edit host, port, token, and device ID.";
        case HOME_COMMAND_MIC:
            return "Record a short microphone message on-device and send it through Hermes speech-to-text.";
        case HOME_COMMAND_CLEAR:
            return "Clear the current reply, health state, and request status so the home screen starts clean again.";
        case HOME_COMMAND_EXIT:
            return "Close Hermes Agent 3DS and return to the Homebrew Launcher.";
        case HOME_COMMAND_NONE:
        case HOME_COMMAND_ASK:
        default:
            return "Open the software keyboard and send a text prompt in the active conversation.";
    }
}

static void render_home_graphical(
    const HermesAppConfig* config,
    const BridgeHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t reply_page,
    size_t command_selection,
    const char* status_line,
    const BridgeV2ConversationListResult* conversation_list
)
{
    char conversation_label[HERMES_APP_CONVERSATION_ID_MAX];
    char token_summary[48];
    char bridge_summary[80];
    char page_label[24];
    size_t room_line_count;
    size_t status_line_count;
    size_t token_line_count;
    size_t detail_line_count;
    size_t message_line_count;
    size_t reply_line_count = 0;
    size_t page_count = 1;
    size_t reply_start = 0;
    bool has_selection = command_selection <= (size_t)HOME_COMMAND_EXIT;
    size_t selected_command = has_selection ? command_selection : (size_t)HOME_COMMAND_ASK;
    const char* reply_source = NULL;

    format_active_conversation_label(config, conversation_list, conversation_label, sizeof(conversation_label));
    format_token_summary(config, token_summary, sizeof(token_summary));
    snprintf(bridge_summary, sizeof(bridge_summary), "%s:%u", config->host, (unsigned int)config->port);

    if (chat_result != NULL && chat_result->success)
        reply_source = chat_result->reply;
    else if (chat_result != NULL && chat_result->error[0] != '\0')
        reply_source = chat_result->error;
    else if (health_result != NULL && health_result->success)
        reply_source = "Hermes gateway OK.";
    else if (health_result != NULL && health_result->error[0] != '\0')
        reply_source = health_result->error;
    else
        reply_source = "Ready for a request.";

    room_line_count = wrap_text_for_pixels(
        conversation_label,
        210.0f,
        0.32f,
        0.32f,
        g_home_room_lines,
        2
    );
    status_line_count = wrap_text_for_pixels(
        status_line,
        124.0f,
        0.29f,
        0.29f,
        g_home_status_lines,
        2
    );
    token_line_count = wrap_text_for_pixels(
        token_summary,
        136.0f,
        0.28f,
        0.28f,
        g_home_token_lines,
        3
    );
    detail_line_count = wrap_text_for_pixels(
        home_command_detail_for_selection(selected_command),
        136.0f,
        0.29f,
        0.29f,
        g_home_detail_lines,
        5
    );
    message_line_count = wrap_text_for_pixels(
        last_message,
        HOME_MESSAGE_MAX_WIDTH,
        HOME_MESSAGE_SCALE,
        HOME_MESSAGE_SCALE,
        g_home_message_lines,
        HOME_MESSAGE_LINES_PER_PAGE
    );
    reply_line_count = wrap_text_for_pixels(
        reply_source,
        HOME_REPLY_MAX_WIDTH,
        HOME_REPLY_SCALE,
        HOME_REPLY_SCALE,
        g_home_reply_lines,
        HOME_WRAP_MAX_LINES
    );
    page_count = page_count_for_lines(reply_line_count, HOME_REPLY_LINES_PER_PAGE);
    if (reply_page >= page_count)
        reply_page = page_count - 1;
    reply_start = reply_page * HOME_REPLY_LINES_PER_PAGE;
    snprintf(page_label, sizeof(page_label), "%lu/%lu", (unsigned long)(reply_page + 1), (unsigned long)page_count);

    app_gfx_begin_top(UI_BG_TOP);
    app_gfx_panel_inset(10.0f, 10.0f, 380.0f, 64.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_chip(18.0f, 16.0f, 110.0f, "ACTIVE THREAD", UI_ACCENT, UI_TEXT);
    app_gfx_text(20.0f, 42.0f, 0.30f, 0.30f, UI_META, "ROOM");
    app_gfx_text(246.0f, 42.0f, 0.30f, 0.30f, UI_META, "LINK");
    draw_text_lines(20.0f, 54.0f, 10.0f, 210.0f, 0.32f, 0.32f, UI_TEXT, g_home_room_lines, room_line_count, 0, 2);
    draw_text_lines(
        246.0f,
        54.0f,
        10.0f,
        124.0f,
        0.29f,
        0.29f,
        health_result != NULL && health_result->success ? UI_SIGNAL_HI : UI_MUTED,
        g_home_status_lines,
        status_line_count,
        0,
        2
    );

    app_gfx_panel_inset(10.0f, 84.0f, 380.0f, 146.0f, UI_REPLY_CARD, UI_BORDER_STRONG, UI_SIGNAL);
    draw_chip(18.0f, 90.0f, 92.0f, "HERMES REPLY", UI_SIGNAL, UI_BG_TOP);
    app_gfx_text_right(380.0f, 92.0f, 0.32f, 0.32f, UI_SIGNAL_HI, page_label);
    app_gfx_text(20.0f, 114.0f, 0.28f, 0.28f, UI_META, "Last message");
    draw_text_lines(20.0f, 126.0f, 10.0f, 350.0f, HOME_MESSAGE_SCALE, HOME_MESSAGE_SCALE, UI_MUTED, g_home_message_lines, message_line_count, 0, HOME_MESSAGE_LINES_PER_PAGE);
    draw_note_lines(20.0f, 150.0f, 360.0f, HOME_REPLY_LINES_PER_PAGE);
    draw_text_lines(20.0f, 154.0f, 10.0f, HOME_REPLY_MAX_WIDTH, HOME_REPLY_SCALE, HOME_REPLY_SCALE, UI_TEXT, g_home_reply_lines, reply_line_count, reply_start, HOME_REPLY_LINES_PER_PAGE);

    app_gfx_begin_bottom(UI_BG_BOTTOM);
    app_gfx_panel_inset(8.0f, 8.0f, 140.0f, 224.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    app_gfx_text(18.0f, 18.0f, 0.36f, 0.36f, UI_SIGNAL_HI, "ACTIONS");
    draw_menu_row(16.0f, 40.0f, 124.0f, "Ask Hermes", selected_command == (size_t)HOME_COMMAND_ASK);
    draw_menu_row(16.0f, 64.0f, 124.0f, "Check Link", selected_command == (size_t)HOME_COMMAND_CHECK);
    draw_menu_row(16.0f, 88.0f, 124.0f, "Conversations", selected_command == (size_t)HOME_COMMAND_THREADS);
    draw_menu_row(16.0f, 112.0f, 124.0f, "Settings", selected_command == (size_t)HOME_COMMAND_CONFIG);
    draw_menu_row(16.0f, 136.0f, 124.0f, "Mic Input", selected_command == (size_t)HOME_COMMAND_MIC);
    draw_menu_row(16.0f, 160.0f, 124.0f, "Clear Reply", selected_command == (size_t)HOME_COMMAND_CLEAR);
    draw_menu_row(16.0f, 184.0f, 124.0f, "Exit", selected_command == (size_t)HOME_COMMAND_EXIT);

    app_gfx_panel_inset(156.0f, 8.0f, 156.0f, 96.0f, UI_PANEL_ALT, UI_BORDER, UI_ACCENT);
    app_gfx_text(166.0f, 18.0f, 0.34f, 0.34f, UI_SIGNAL_HI, "CONNECTION");
    app_gfx_text(166.0f, 38.0f, 0.28f, 0.28f, UI_META, "Gateway");
    app_gfx_text_fit(166.0f, 50.0f, 136.0f, 0.30f, 0.30f, UI_TEXT, bridge_summary);
    app_gfx_text(166.0f, 68.0f, 0.28f, 0.28f, UI_META, "Token");
    draw_text_lines(166.0f, 80.0f, 10.0f, 136.0f, 0.28f, 0.28f, UI_TEXT, g_home_token_lines, token_line_count, 0, 3);

    app_gfx_panel_inset(156.0f, 112.0f, 156.0f, 120.0f, UI_PANEL_ALT, UI_BORDER, UI_ACCENT);
    app_gfx_text(166.0f, 122.0f, 0.34f, 0.34f, UI_SIGNAL_HI, "CURRENT ACTION");
    app_gfx_text_fit(166.0f, 140.0f, 136.0f, 0.32f, 0.32f, UI_TEXT, home_command_label_for_selection(selected_command));
    draw_text_lines(166.0f, 156.0f, 10.0f, 136.0f, 0.29f, 0.29f, UI_MUTED, g_home_detail_lines, detail_line_count, 0, 5);
    app_gfx_text(166.0f, 214.0f, 0.28f, 0.28f, UI_META, "Reply page");
    app_gfx_text_right(302.0f, 214.0f, 0.30f, 0.30f, UI_TEXT, page_label);
}

static void render_settings_graphical(
    const HermesAppConfig* config,
    SettingsField selected_field,
    bool settings_dirty,
    const char* status_line,
    Result last_rc
)
{
    char token_summary[48];
    char rc_line[32];
    float row_y = 74.0f;
    const char* host_cursor = selected_field == SETTINGS_FIELD_HOST ? "HOST" : "Host";
    const char* port_cursor = selected_field == SETTINGS_FIELD_PORT ? "PORT" : "Port";
    const char* token_cursor = selected_field == SETTINGS_FIELD_TOKEN ? "TOKEN" : "Token";
    const char* device_id_cursor = selected_field == SETTINGS_FIELD_DEVICE_ID ? "DEVICE ID" : "Device ID";

    format_token_summary(config, token_summary, sizeof(token_summary));
    snprintf(rc_line, sizeof(rc_line), "rc 0x%08lX", (unsigned long)last_rc);

    app_gfx_begin_top(UI_BG_TOP);
    draw_header("HERMES AGENT", "SYSTEM CONFIG");
    app_gfx_panel_inset(12.0f, 48.0f, 376.0f, 176.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    app_gfx_text_fit(20.0f, 58.0f, 320.0f, 0.42f, 0.42f, UI_TEXT, settings_dirty ? "LINK SETTINGS*" : "LINK SETTINGS");
    app_gfx_text_right(380.0f, 58.0f, 0.30f, 0.30f, UI_MUTED, rc_line);

    app_gfx_highlight_bar(18.0f, row_y - 2.0f + (float)selected_field * 34.0f, 170.0f, 28.0f, UI_ACCENT);
    app_gfx_text_fit(24.0f, row_y + 2.0f, 154.0f, 0.38f, 0.38f, selected_field == SETTINGS_FIELD_HOST ? UI_BG_TOP : UI_TEXT, host_cursor);
    app_gfx_text_fit(24.0f, row_y + 36.0f, 154.0f, 0.38f, 0.38f, selected_field == SETTINGS_FIELD_PORT ? UI_BG_TOP : UI_TEXT, port_cursor);
    app_gfx_text_fit(24.0f, row_y + 70.0f, 154.0f, 0.38f, 0.38f, selected_field == SETTINGS_FIELD_TOKEN ? UI_BG_TOP : UI_TEXT, token_cursor);
    app_gfx_text_fit(24.0f, row_y + 104.0f, 154.0f, 0.38f, 0.38f, selected_field == SETTINGS_FIELD_DEVICE_ID ? UI_BG_TOP : UI_TEXT, device_id_cursor);

    app_gfx_panel(198.0f, 72.0f, 178.0f, 28.0f, UI_PANEL_ALT, UI_BORDER);
    app_gfx_text_fit(206.0f, 80.0f, 162.0f, 0.34f, 0.34f, UI_TEXT, config->host);
    app_gfx_panel(198.0f, 106.0f, 178.0f, 28.0f, UI_PANEL_ALT, UI_BORDER);
    snprintf(rc_line, sizeof(rc_line), "%u", (unsigned int)config->port);
    app_gfx_text_fit(206.0f, 114.0f, 162.0f, 0.34f, 0.34f, UI_TEXT, rc_line);
    app_gfx_panel(198.0f, 140.0f, 178.0f, 28.0f, UI_PANEL_ALT, UI_BORDER);
    app_gfx_text_fit(206.0f, 148.0f, 162.0f, 0.32f, 0.32f, UI_TEXT, token_summary);
    app_gfx_panel(198.0f, 174.0f, 178.0f, 28.0f, UI_PANEL_ALT, UI_BORDER);
    app_gfx_text_fit(206.0f, 182.0f, 162.0f, 0.32f, 0.32f, UI_TEXT, config->device_id[0] != '\0' ? config->device_id : "<empty>");
    app_gfx_text_fit(20.0f, 208.0f, 356.0f, 0.30f, 0.30f, UI_MUTED, status_line);

    app_gfx_begin_bottom(UI_BG_BOTTOM);
    draw_bottom_header("SETTINGS ACTIONS");
    app_gfx_panel_inset(8.0f, 46.0f, 304.0f, 138.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_menu_row(16.0f, 58.0f, 288.0f, "Edit value", false);
    draw_menu_row(16.0f, 78.0f, 288.0f, "Save settings", false);
    draw_menu_row(16.0f, 98.0f, 288.0f, "Restore defaults", false);
    draw_menu_row(16.0f, 118.0f, 288.0f, "Return home", false);
    draw_menu_row(16.0f, 138.0f, 288.0f, "Exit app", false);
}

static void render_conversations_graphical(
    const HermesAppConfig* config,
    const char* status_line,
    Result last_rc,
    const BridgeV2ConversationListResult* conversation_list,
    size_t conversation_selection
)
{
    size_t visible = 4;
    size_t start = 0;
    size_t end;
    size_t index;
    char rc_line[32];

    snprintf(rc_line, sizeof(rc_line), "rc 0x%08lX", (unsigned long)last_rc);
    app_gfx_begin_top(UI_BG_TOP);
    draw_header("HERMES AGENT", "THREAD LOG");
    app_gfx_panel_inset(12.0f, 48.0f, 376.0f, 176.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_chip(20.0f, 54.0f, 92.0f, "THREAD ARCHIVE", UI_ACCENT, UI_TEXT);
    app_gfx_text_right(380.0f, 58.0f, 0.30f, 0.30f, UI_MUTED, rc_line);
    app_gfx_text_fit(20.0f, 76.0f, 356.0f, 0.30f, 0.30f, UI_MUTED, status_line);

    if (config == NULL || config->recent_conversation_count == 0) {
        app_gfx_text(20.0f, 98.0f, 0.38f, 0.38f, UI_TEXT, "No saved conversations.");
        app_gfx_text(20.0f, 116.0f, 0.34f, 0.34f, UI_MUTED, "Create one from the lower action list.");
    } else {
        if (conversation_selection >= visible)
            start = conversation_selection - (visible - 1);
        end = start + visible;
        if (end > config->recent_conversation_count)
            end = config->recent_conversation_count;

        for (index = start; index < end; index++) {
            const BridgeV2ConversationInfo* info = find_synced_conversation(conversation_list, config->recent_conversations[index]);
            const char* title = (info != NULL && info->title[0] != '\0') ? info->title : config->recent_conversations[index];
            float row_y = 92.0f + (float)(index - start) * 32.0f;
            bool selected = index == conversation_selection;

            if (selected)
                app_gfx_highlight_bar(18.0f, row_y - 2.0f, 364.0f, 26.0f, UI_ACCENT);
            app_gfx_text_fit(24.0f, row_y + 2.0f, 350.0f, 0.36f, 0.36f, selected ? UI_BG_TOP : UI_TEXT, title);
            app_gfx_text_fit(24.0f, row_y + 14.0f, 350.0f, 0.28f, 0.28f, selected ? UI_BG_TOP : UI_MUTED,
                info != NULL && info->preview[0] != '\0' ? info->preview : config->recent_conversations[index]);
        }
    }

    app_gfx_begin_bottom(UI_BG_BOTTOM);
    draw_bottom_header("THREAD ACTIONS");
    app_gfx_panel_inset(8.0f, 46.0f, 304.0f, 138.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_menu_row(16.0f, 58.0f, 288.0f, "Use thread", false);
    draw_menu_row(16.0f, 78.0f, 288.0f, "New thread", false);
    draw_menu_row(16.0f, 98.0f, 288.0f, "Sync Hermes", false);
    draw_menu_row(16.0f, 118.0f, 288.0f, "Return home", false);
    draw_menu_row(16.0f, 138.0f, 288.0f, "Exit app", false);
}

void hermes_app_ui_render_approval_prompt(const char* request_id)
{
    char request_line[96];

    if (request_id != NULL && request_id[0] != '\0')
        snprintf(request_line, sizeof(request_line), "Request %s", request_id);
    else
        snprintf(request_line, sizeof(request_line), "Approval request pending");

    app_gfx_begin_frame();

    app_gfx_begin_top(UI_BG_TOP);
    draw_header("HERMES AGENT", "APPROVAL");
    app_gfx_panel_inset(24.0f, 56.0f, 352.0f, 156.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    app_gfx_text(36.0f, 72.0f, 0.46f, 0.46f, UI_TEXT, "Approval required");
    app_gfx_text_fit(36.0f, 92.0f, 328.0f, 0.32f, 0.32f, UI_MUTED, request_line);
    app_gfx_text_fit(36.0f, 120.0f, 328.0f, 0.34f, 0.34f, UI_SIGNAL_HI, "Hermes needs a response before it can continue.");
    app_gfx_text_fit(36.0f, 144.0f, 328.0f, 0.32f, 0.32f, UI_TEXT, "Choose how long to allow the action, or deny it.");

    app_gfx_begin_bottom(UI_BG_BOTTOM);
    draw_bottom_header("APPROVAL OPTIONS");
    app_gfx_panel_inset(8.0f, 46.0f, 304.0f, 138.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_menu_row(16.0f, 58.0f, 288.0f, "Allow once", false);
    draw_menu_row(16.0f, 78.0f, 288.0f, "Allow for session", false);
    draw_menu_row(16.0f, 98.0f, 288.0f, "Always allow", false);
    draw_menu_row(16.0f, 118.0f, 288.0f, "Deny request", false);
    draw_menu_row(16.0f, 138.0f, 288.0f, "Cancel", false);

    app_gfx_end_frame();
}

void hermes_app_ui_render_voice_recording(unsigned long tenths, size_t pcm_size, const char* status_line, bool waiting_for_up_release)
{
    char time_line[32];
    char audio_line[48];
    const char* capture_state = waiting_for_up_release ? "Release upward input to arm stop." : "Move upward to stop and send.";

    snprintf(time_line, sizeof(time_line), "%lu.%lus", tenths / 10, tenths % 10);
    snprintf(audio_line, sizeof(audio_line), "%lu bytes", (unsigned long)pcm_size);

    app_gfx_begin_frame();

    app_gfx_begin_top(UI_BG_TOP);
    draw_header("HERMES AGENT", "MIC INPUT");
    app_gfx_panel_inset(20.0f, 54.0f, 360.0f, 160.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    app_gfx_text(32.0f, 70.0f, 0.46f, 0.46f, UI_TEXT, "Recording microphone");
    app_gfx_text_fit(32.0f, 92.0f, 316.0f, 0.32f, 0.32f, UI_MUTED, status_line != NULL && status_line[0] != '\0' ? status_line : "Mic is recording now.");
    app_gfx_text(32.0f, 122.0f, 0.34f, 0.34f, UI_SIGNAL_HI, "TIME");
    app_gfx_panel(96.0f, 118.0f, 100.0f, 24.0f, UI_PANEL_ALT, UI_BORDER);
    app_gfx_text(106.0f, 124.0f, 0.34f, 0.34f, UI_TEXT, time_line);
    app_gfx_text(220.0f, 122.0f, 0.34f, 0.34f, UI_SIGNAL_HI, "AUDIO");
    app_gfx_panel(286.0f, 118.0f, 74.0f, 24.0f, UI_PANEL_ALT, UI_BORDER);
    app_gfx_text_fit(292.0f, 124.0f, 62.0f, 0.30f, 0.30f, UI_TEXT, audio_line);
    app_gfx_text(32.0f, 162.0f, 0.34f, 0.34f, UI_TEXT, capture_state);

    app_gfx_begin_bottom(UI_BG_BOTTOM);
    draw_bottom_header("MIC SESSION");
    app_gfx_panel_inset(8.0f, 46.0f, 304.0f, 138.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_menu_row(16.0f, 58.0f, 288.0f, "Stop and send", !waiting_for_up_release);
    draw_menu_row(16.0f, 78.0f, 288.0f, "Cancel recording", false);
    draw_menu_row(16.0f, 98.0f, 288.0f, "Exit app", false);
    draw_menu_row(16.0f, 118.0f, 288.0f, "5 min safety timeout", false);
    draw_menu_row(16.0f, 138.0f, 288.0f, capture_state, false);

    app_gfx_end_frame();
}

void hermes_app_ui_render(
    AppScreen screen,
    const HermesAppConfig* config,
    SettingsField selected_field,
    bool settings_dirty,
    const BridgeHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t reply_page,
    size_t command_selection,
    const char* status_line,
    Result last_rc,
    const BridgeV2ConversationListResult* conversation_list,
    size_t conversation_selection
)
{
    app_gfx_begin_frame();

    if (screen == APP_SCREEN_SETTINGS) {
        render_settings_graphical(config, selected_field, settings_dirty, status_line, last_rc);
    } else if (screen == APP_SCREEN_CONVERSATIONS) {
        render_conversations_graphical(config, status_line, last_rc, conversation_list, conversation_selection);
    } else {
        render_home_graphical(config, health_result, chat_result, last_message, reply_page, command_selection, status_line, conversation_list);
    }

    app_gfx_end_frame();
}
