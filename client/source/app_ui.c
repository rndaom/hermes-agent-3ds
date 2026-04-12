#include "app_ui.h"
#include "app_gfx.h"

#include <stdio.h>
#include <string.h>

#define HOME_WRAP_WIDTH 46
#define HOME_WRAP_MAX_LINES 128
#define HOME_MESSAGE_LINES_PER_PAGE 3
#define HOME_REPLY_LINES_PER_PAGE 8

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

static void add_truncation_marker(char* line)
{
    size_t length;

    if (line == NULL)
        return;

    length = strlen(line);
    if (HOME_WRAP_WIDTH == 0)
        return;

    if (length == 0) {
        line[0] = '~';
        line[1] = '\0';
        return;
    }

    if (length >= HOME_WRAP_WIDTH)
        line[HOME_WRAP_WIDTH - 1] = '~';
    else {
        line[length] = '~';
        line[length + 1] = '\0';
    }
}

static size_t wrap_text_for_console(const char* input, char lines[][HOME_WRAP_WIDTH + 1], size_t max_lines)
{
    const char* cursor;
    size_t line_count = 0;

    if (lines == NULL || max_lines == 0)
        return 0;

    if (input == NULL || input[0] == '\0') {
        copy_bounded_string(lines[0], HOME_WRAP_WIDTH + 1, "<none>");
        return 1;
    }

    cursor = input;
    while (*cursor != '\0' && line_count < max_lines) {
        char line[HOME_WRAP_WIDTH + 1];
        size_t line_len = 0;

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
            size_t needed;

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

            if (line_len == 0 && word_len > HOME_WRAP_WIDTH) {
                memcpy(line, word_start, HOME_WRAP_WIDTH);
                line[HOME_WRAP_WIDTH] = '\0';
                cursor = word_start + HOME_WRAP_WIDTH;
                line_len = HOME_WRAP_WIDTH;
                break;
            }

            needed = word_len + (line_len > 0 ? 1 : 0);
            if (line_len + needed > HOME_WRAP_WIDTH)
                break;

            if (line_len > 0)
                line[line_len++] = ' ';

            memcpy(line + line_len, word_start, word_len);
            line_len += word_len;
            line[line_len] = '\0';
            cursor = word_start + word_len;
        }

        if (line_len == 0) {
            line[0] = '\0';
            if (*cursor == '\n')
                cursor++;
        }

        copy_bounded_string(lines[line_count], HOME_WRAP_WIDTH + 1, line);
        line_count++;

        if (*cursor == '\n')
            cursor++;
    }

    if (*cursor != '\0' && line_count > 0)
        add_truncation_marker(lines[line_count - 1]);

    if (line_count == 0) {
        copy_bounded_string(lines[0], HOME_WRAP_WIDTH + 1, "<none>");
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
    char wrapped_lines[HOME_WRAP_MAX_LINES][HOME_WRAP_WIDTH + 1];
    size_t total_lines = wrap_text_for_console(reply_text, wrapped_lines, HOME_WRAP_MAX_LINES);
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

#define UI_BG_TOP C2D_Color32(0x1C, 0x1D, 0x25, 0xFF)
#define UI_BG_BOTTOM C2D_Color32(0x22, 0x25, 0x31, 0xFF)
#define UI_PANEL C2D_Color32(0x2D, 0x33, 0x47, 0xFF)
#define UI_PANEL_ALT C2D_Color32(0x3C, 0x46, 0x5E, 0xFF)
#define UI_BORDER C2D_Color32(0xD9, 0xB4, 0x5F, 0xFF)
#define UI_ACCENT C2D_Color32(0x58, 0xC7, 0xC2, 0xFF)
#define UI_TEXT C2D_Color32(0xF4, 0xF1, 0xE6, 0xFF)
#define UI_MUTED C2D_Color32(0xC8, 0xD0, 0xD8, 0xFF)
#define UI_HILITE C2D_Color32(0x8A, 0xE7, 0xE3, 0xFF)

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
    app_gfx_text_right(382.0f, 16.0f, 0.42f, 0.42f, UI_HILITE, subtitle);
}

static void draw_bottom_header(const char* title)
{
    app_gfx_panel_inset(8.0f, 10.0f, 304.0f, 28.0f, UI_PANEL_ALT, UI_BORDER, UI_ACCENT);
    app_gfx_text(16.0f, 16.0f, 0.50f, 0.50f, UI_TEXT, title);
}

static void draw_text_lines(
    float x,
    float y,
    float line_height,
    float scale_x,
    float scale_y,
    u32 color,
    char lines[][HOME_WRAP_WIDTH + 1],
    size_t total_lines,
    size_t start,
    size_t max_lines
)
{
    size_t index;

    for (index = 0; index < max_lines && start + index < total_lines; index++)
        app_gfx_text(x, y + (float)index * line_height, scale_x, scale_y, color, lines[start + index]);
}

static void draw_menu_row(float x, float y, float w, const char* label, bool selected)
{
    if (selected)
        app_gfx_highlight_bar(x, y, w, 16.0f, UI_ACCENT);
    app_gfx_text(x + 8.0f, y + 3.0f, 0.43f, 0.43f, selected ? UI_BG_TOP : UI_TEXT, label);
}

static void draw_status_line(float x, float y, const char* label, const char* value)
{
    app_gfx_text(x, y, 0.38f, 0.38f, UI_HILITE, label);
    app_gfx_text(x, y + 10.0f, 0.36f, 0.36f, UI_TEXT, value != NULL ? value : "<none>");
}

static void render_home_graphical(
    const HermesAppConfig* config,
    const BridgeHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t reply_page,
    const char* status_line,
    const BridgeV2ConversationListResult* conversation_list
)
{
    char conversation_label[HERMES_APP_CONVERSATION_ID_MAX];
    char token_summary[48];
    char bridge_summary[80];
    char page_label[24];
    char message_lines[16][HOME_WRAP_WIDTH + 1];
    char reply_lines[HOME_WRAP_MAX_LINES][HOME_WRAP_WIDTH + 1];
    size_t message_line_count = wrap_text_for_console(last_message, message_lines, 16);
    size_t reply_line_count = 0;
    size_t page_count = 1;
    size_t reply_start = 0;
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

    reply_line_count = wrap_text_for_console(reply_source, reply_lines, HOME_WRAP_MAX_LINES);
    page_count = page_count_for_lines(reply_line_count, HOME_REPLY_LINES_PER_PAGE);
    if (reply_page >= page_count)
        reply_page = page_count - 1;
    reply_start = reply_page * HOME_REPLY_LINES_PER_PAGE;
    snprintf(page_label, sizeof(page_label), "%lu/%lu", (unsigned long)(reply_page + 1), (unsigned long)page_count);

    app_gfx_begin_top(UI_BG_TOP);
    draw_header("HERMES AGENT", "RELAY DECK");
    app_gfx_panel_inset(12.0f, 48.0f, 226.0f, 76.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_status_line(22.0f, 58.0f, "ACTIVE THREAD", conversation_label);
    draw_status_line(22.0f, 82.0f, "LINK STATUS", status_line);
    app_gfx_text(22.0f, 106.0f, 0.34f, 0.34f, UI_MUTED, health_result != NULL && health_result->success ? "bridge healthy" : "awaiting relay");

    app_gfx_panel_inset(248.0f, 48.0f, 140.0f, 76.0f, UI_PANEL_ALT, UI_BORDER, UI_ACCENT);
    app_gfx_panel(284.0f, 66.0f, 40.0f, 40.0f, UI_ACCENT, UI_BORDER);
    app_gfx_panel(296.0f, 58.0f, 16.0f, 8.0f, UI_HILITE, UI_BORDER);
    app_gfx_text(266.0f, 108.0f, 0.34f, 0.34f, UI_TEXT, "relay crest");

    app_gfx_panel_inset(12.0f, 132.0f, 376.0f, 96.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    app_gfx_text(20.0f, 140.0f, 0.46f, 0.46f, UI_TEXT, "HERMES REPLY");
    app_gfx_text_right(380.0f, 140.0f, 0.36f, 0.36f, UI_HILITE, page_label);
    app_gfx_text(20.0f, 156.0f, 0.32f, 0.32f, UI_MUTED, "Last message:");
    draw_text_lines(20.0f, 166.0f, 10.0f, 0.32f, 0.32f, UI_TEXT, message_lines, message_line_count, 0, 2);
    draw_text_lines(20.0f, 190.0f, 10.0f, 0.34f, 0.34f, UI_TEXT, reply_lines, reply_line_count, reply_start, HOME_REPLY_LINES_PER_PAGE > 3 ? 3 : HOME_REPLY_LINES_PER_PAGE);

    app_gfx_begin_bottom(UI_BG_BOTTOM);
    draw_bottom_header("COMMAND MENU");
    app_gfx_panel_inset(8.0f, 46.0f, 184.0f, 138.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_menu_row(16.0f, 56.0f, 168.0f, "B Ask Hermes", true);
    draw_menu_row(16.0f, 74.0f, 168.0f, "A Check Link", false);
    draw_menu_row(16.0f, 92.0f, 168.0f, "SELECT Threads", false);
    draw_menu_row(16.0f, 110.0f, 168.0f, "X Config", false);
    draw_menu_row(16.0f, 128.0f, 168.0f, "UP Mic Input", false);
    draw_menu_row(16.0f, 146.0f, 168.0f, "Y Clear Reply", false);
    draw_menu_row(16.0f, 164.0f, 168.0f, "START Exit", false);

    app_gfx_panel_inset(202.0f, 46.0f, 110.0f, 66.0f, UI_PANEL_ALT, UI_BORDER, UI_ACCENT);
    app_gfx_text(210.0f, 56.0f, 0.34f, 0.34f, UI_HILITE, "GATEWAY");
    app_gfx_text(210.0f, 70.0f, 0.32f, 0.32f, UI_TEXT, bridge_summary);
    app_gfx_text(210.0f, 88.0f, 0.34f, 0.34f, UI_HILITE, "TOKEN");
    app_gfx_text(210.0f, 102.0f, 0.30f, 0.30f, UI_TEXT, token_summary);

    app_gfx_panel_inset(202.0f, 120.0f, 110.0f, 64.0f, UI_PANEL_ALT, UI_BORDER, UI_ACCENT);
    app_gfx_text(210.0f, 130.0f, 0.33f, 0.33f, UI_HILITE, "HINT");
    app_gfx_text(210.0f, 144.0f, 0.30f, 0.30f, UI_TEXT, "L/R page reply");
    app_gfx_text(210.0f, 156.0f, 0.30f, 0.30f, UI_TEXT, "Old 3DS safe");
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
    app_gfx_text(20.0f, 58.0f, 0.42f, 0.42f, UI_TEXT, settings_dirty ? "LINK SETTINGS*" : "LINK SETTINGS");
    app_gfx_text_right(380.0f, 58.0f, 0.30f, 0.30f, UI_MUTED, rc_line);

    app_gfx_highlight_bar(18.0f, row_y - 2.0f + (float)selected_field * 34.0f, 170.0f, 28.0f, UI_ACCENT);
    app_gfx_text(24.0f, row_y + 2.0f, 0.38f, 0.38f, selected_field == SETTINGS_FIELD_HOST ? UI_BG_TOP : UI_TEXT, host_cursor);
    app_gfx_text(24.0f, row_y + 36.0f, 0.38f, 0.38f, selected_field == SETTINGS_FIELD_PORT ? UI_BG_TOP : UI_TEXT, port_cursor);
    app_gfx_text(24.0f, row_y + 70.0f, 0.38f, 0.38f, selected_field == SETTINGS_FIELD_TOKEN ? UI_BG_TOP : UI_TEXT, token_cursor);
    app_gfx_text(24.0f, row_y + 104.0f, 0.38f, 0.38f, selected_field == SETTINGS_FIELD_DEVICE_ID ? UI_BG_TOP : UI_TEXT, device_id_cursor);

    app_gfx_panel(198.0f, 72.0f, 178.0f, 28.0f, UI_PANEL_ALT, UI_BORDER);
    app_gfx_text(206.0f, 80.0f, 0.34f, 0.34f, UI_TEXT, config->host);
    app_gfx_panel(198.0f, 106.0f, 178.0f, 28.0f, UI_PANEL_ALT, UI_BORDER);
    snprintf(rc_line, sizeof(rc_line), "%u", (unsigned int)config->port);
    app_gfx_text(206.0f, 114.0f, 0.34f, 0.34f, UI_TEXT, rc_line);
    app_gfx_panel(198.0f, 140.0f, 178.0f, 28.0f, UI_PANEL_ALT, UI_BORDER);
    app_gfx_text(206.0f, 148.0f, 0.32f, 0.32f, UI_TEXT, token_summary);
    app_gfx_panel(198.0f, 174.0f, 178.0f, 28.0f, UI_PANEL_ALT, UI_BORDER);
    app_gfx_text(206.0f, 182.0f, 0.32f, 0.32f, UI_TEXT, config->device_id[0] != '\0' ? config->device_id : "<empty>");
    app_gfx_text(20.0f, 208.0f, 0.30f, 0.30f, UI_MUTED, status_line);

    app_gfx_begin_bottom(UI_BG_BOTTOM);
    draw_bottom_header("OPTIONS MENU");
    app_gfx_panel_inset(8.0f, 46.0f, 304.0f, 138.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_menu_row(16.0f, 58.0f, 288.0f, "A Edit value", true);
    draw_menu_row(16.0f, 78.0f, 288.0f, "X Save settings", false);
    draw_menu_row(16.0f, 98.0f, 288.0f, "Y Restore defaults", false);
    draw_menu_row(16.0f, 118.0f, 288.0f, "B Home", false);
    draw_menu_row(16.0f, 138.0f, 288.0f, "START Exit", false);
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

    app_gfx_begin_top(UI_BG_TOP);
    draw_header("HERMES AGENT", "THREAD LOG");
    app_gfx_panel_inset(12.0f, 48.0f, 376.0f, 176.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    app_gfx_text(20.0f, 58.0f, 0.42f, 0.42f, UI_TEXT, "THREAD ARCHIVE");
    snprintf(rc_line, sizeof(rc_line), "rc 0x%08lX", (unsigned long)last_rc);
    app_gfx_text_right(380.0f, 58.0f, 0.30f, 0.30f, UI_MUTED, rc_line);
    app_gfx_text(20.0f, 72.0f, 0.30f, 0.30f, UI_MUTED, status_line);

    if (config == NULL || config->recent_conversation_count == 0) {
        app_gfx_text(20.0f, 98.0f, 0.38f, 0.38f, UI_TEXT, "No saved conversations.");
        app_gfx_text(20.0f, 116.0f, 0.34f, 0.34f, UI_MUTED, "Press X to add one.");
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
            app_gfx_text(24.0f, row_y + 2.0f, 0.36f, 0.36f, selected ? UI_BG_TOP : UI_TEXT, title);
            app_gfx_text(24.0f, row_y + 14.0f, 0.28f, 0.28f, selected ? UI_BG_TOP : UI_MUTED,
                info != NULL && info->preview[0] != '\0' ? info->preview : config->recent_conversations[index]);
        }
    }

    app_gfx_begin_bottom(UI_BG_BOTTOM);
    draw_bottom_header("THREAD MENU");
    app_gfx_panel_inset(8.0f, 46.0f, 304.0f, 138.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_menu_row(16.0f, 58.0f, 288.0f, "A Use thread", true);
    draw_menu_row(16.0f, 78.0f, 288.0f, "X New thread", false);
    draw_menu_row(16.0f, 98.0f, 288.0f, "Y Sync Hermes", false);
    draw_menu_row(16.0f, 118.0f, 288.0f, "B Home", false);
    draw_menu_row(16.0f, 138.0f, 288.0f, "START Exit", false);
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
    app_gfx_text_fit(36.0f, 120.0f, 328.0f, 0.34f, 0.34f, UI_HILITE, "Hermes needs a response before it can continue.");
    app_gfx_text_fit(36.0f, 144.0f, 328.0f, 0.32f, 0.32f, UI_TEXT, "Choose how long to allow the action, or deny it.");

    app_gfx_begin_bottom(UI_BG_BOTTOM);
    draw_bottom_header("APPROVAL CONTROLS");
    app_gfx_panel_inset(8.0f, 46.0f, 304.0f, 138.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_menu_row(16.0f, 58.0f, 288.0f, "A Allow once", false);
    draw_menu_row(16.0f, 78.0f, 288.0f, "X Allow session", false);
    draw_menu_row(16.0f, 98.0f, 288.0f, "Y Allow always", false);
    draw_menu_row(16.0f, 118.0f, 288.0f, "B Deny", false);
    draw_menu_row(16.0f, 138.0f, 288.0f, "START Cancel", false);

    app_gfx_end_frame();
}

void hermes_app_ui_render_voice_recording(unsigned long tenths, size_t pcm_size, const char* status_line, bool waiting_for_up_release)
{
    char time_line[32];
    char audio_line[48];
    const char* capture_state = waiting_for_up_release ? "Release UP to arm stop." : "Press UP to stop and send.";

    snprintf(time_line, sizeof(time_line), "%lu.%lus", tenths / 10, tenths % 10);
    snprintf(audio_line, sizeof(audio_line), "%lu bytes", (unsigned long)pcm_size);

    app_gfx_begin_frame();

    app_gfx_begin_top(UI_BG_TOP);
    draw_header("HERMES AGENT", "MIC INPUT");
    app_gfx_panel_inset(20.0f, 54.0f, 360.0f, 160.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    app_gfx_text(32.0f, 70.0f, 0.46f, 0.46f, UI_TEXT, "Recording microphone");
    app_gfx_text_fit(32.0f, 92.0f, 316.0f, 0.32f, 0.32f, UI_MUTED, status_line != NULL && status_line[0] != '\0' ? status_line : "Mic is recording now.");
    app_gfx_text(32.0f, 122.0f, 0.34f, 0.34f, UI_HILITE, "TIME");
    app_gfx_panel(96.0f, 118.0f, 100.0f, 24.0f, UI_PANEL_ALT, UI_BORDER);
    app_gfx_text(106.0f, 124.0f, 0.34f, 0.34f, UI_TEXT, time_line);
    app_gfx_text(220.0f, 122.0f, 0.34f, 0.34f, UI_HILITE, "AUDIO");
    app_gfx_panel(286.0f, 118.0f, 74.0f, 24.0f, UI_PANEL_ALT, UI_BORDER);
    app_gfx_text_fit(292.0f, 124.0f, 62.0f, 0.30f, 0.30f, UI_TEXT, audio_line);
    app_gfx_text(32.0f, 162.0f, 0.34f, 0.34f, UI_TEXT, capture_state);

    app_gfx_begin_bottom(UI_BG_BOTTOM);
    draw_bottom_header("MIC CONTROLS");
    app_gfx_panel_inset(8.0f, 46.0f, 304.0f, 138.0f, UI_PANEL, UI_BORDER, UI_ACCENT);
    draw_menu_row(16.0f, 58.0f, 288.0f, "UP Stop + send", !waiting_for_up_release);
    draw_menu_row(16.0f, 78.0f, 288.0f, "B Cancel", false);
    draw_menu_row(16.0f, 98.0f, 288.0f, "START Cancel", false);
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
        render_home_graphical(config, health_result, chat_result, last_message, reply_page, status_line, conversation_list);
    }

    app_gfx_end_frame();
}
