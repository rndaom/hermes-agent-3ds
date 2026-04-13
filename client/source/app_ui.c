#include "app_ui.h"
#include "app_gfx.h"
#include "app_home.h"

#include <stdio.h>
#include <string.h>

#define HOME_WRAP_MAX_LINES 128
#define HOME_WRAP_LINE_MAX 192
#define HOME_SUMMARY_WRAP_MAX_LINES 4
#define HOME_INFO_WRAP_MAX_LINES 8
#define HOME_HISTORY_MAX 6
#define HOME_HISTORY_CARD_LINES 8
#define HOME_REPLY_LINES_PER_PAGE 6
#define HOME_REPLY_MAX_WIDTH 348.0f
#define HOME_REPLY_SCALE 0.32f

typedef struct AppUiHomeHistoryEntry {
    AppUiMessageAuthor author;
    char text[BRIDGE_CHAT_REPLY_MAX];
} AppUiHomeHistoryEntry;

static char g_reply_page_lines[HOME_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];
static char g_home_room_lines[HOME_SUMMARY_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];
static char g_home_status_lines[HOME_SUMMARY_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];
static char g_home_detail_lines[HOME_INFO_WRAP_MAX_LINES][HOME_WRAP_LINE_MAX];
static char g_home_card_lines[HOME_HISTORY_CARD_LINES][HOME_WRAP_LINE_MAX];
static AppUiHomeHistoryEntry g_home_history[HOME_HISTORY_MAX];
static size_t g_home_history_count = 0;

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

void hermes_app_ui_home_history_reset(void)
{
    memset(g_home_history, 0, sizeof(g_home_history));
    g_home_history_count = 0;
}

void hermes_app_ui_home_history_push(AppUiMessageAuthor author, const char* text)
{
    size_t index;

    if (text == NULL || text[0] == '\0')
        return;

    if (g_home_history_count > 0) {
        AppUiHomeHistoryEntry* last = &g_home_history[g_home_history_count - 1];
        if (last->author == author && strcmp(last->text, text) == 0)
            return;
    }

    if (g_home_history_count < HOME_HISTORY_MAX) {
        index = g_home_history_count++;
    } else {
        memmove(&g_home_history[0], &g_home_history[1], sizeof(g_home_history[0]) * (HOME_HISTORY_MAX - 1));
        index = HOME_HISTORY_MAX - 1;
    }

    g_home_history[index].author = author;
    copy_bounded_string(g_home_history[index].text, sizeof(g_home_history[index].text), text);
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

#undef HOME_REPLY_MAX_WIDTH
#undef HOME_REPLY_SCALE
#define HOME_REPLY_MAX_WIDTH 326.0f
#define HOME_REPLY_SCALE 0.28f

#define UI_COLOR32(r, g, b, a) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | ((u32)(a) << 24))
#define UI_BG UI_COLOR32(0xEF, 0xEF, 0xE9, 0xFF)
#define UI_PAPER UI_COLOR32(0xFB, 0xFB, 0xF6, 0xFF)
#define UI_PAPER_ALT UI_COLOR32(0xF3, 0xF3, 0xED, 0xFF)
#define UI_PAPER_SHADOW UI_COLOR32(0xE2, 0xE2, 0xDA, 0xFF)
#define UI_RULE UI_COLOR32(0xD4, 0xD4, 0xCC, 0xFF)
#define UI_RULE_STRONG UI_COLOR32(0xC2, 0xC2, 0xBA, 0xFF)
#define UI_BORDER UI_COLOR32(0x90, 0x91, 0x8B, 0xFF)
#define UI_BORDER_DARK UI_COLOR32(0x62, 0x63, 0x60, 0xFF)
#define UI_TEXT UI_COLOR32(0x28, 0x2A, 0x2E, 0xFF)
#define UI_MUTED UI_COLOR32(0x55, 0x57, 0x5E, 0xFF)
#define UI_META UI_COLOR32(0x77, 0x79, 0x80, 0xFF)
#define UI_ALERT_BG UI_COLOR32(0x1B, 0x1B, 0x1D, 0xFF)
#define UI_ALERT_LINE UI_COLOR32(0x3A, 0x3A, 0x3D, 0xFF)
#define UI_ALERT_BORDER UI_COLOR32(0xFF, 0x8C, 0x1C, 0xFF)

typedef struct AppUiTheme {
    u32 rail_top;
    u32 rail_mid;
    u32 rail_bottom;
    u32 accent_light;
    u32 accent;
    u32 accent_dark;
    u32 accent_text;
} AppUiTheme;

static const AppUiTheme g_ui_room_themes[] = {
    {UI_COLOR32(0xB7, 0xEA, 0xFF, 0xFF), UI_COLOR32(0x72, 0xD5, 0xFF, 0xFF), UI_COLOR32(0x39, 0xBD, 0xFF, 0xFF), UI_COLOR32(0xD8, 0xF4, 0xFF, 0xFF), UI_COLOR32(0x60, 0xCB, 0xFF, 0xFF), UI_COLOR32(0x11, 0x74, 0xCB, 0xFF), UI_TEXT},
    {UI_COLOR32(0xFF, 0xD0, 0x92, 0xFF), UI_COLOR32(0xFF, 0xA9, 0x4D, 0xFF), UI_COLOR32(0xF1, 0x7E, 0x0C, 0xFF), UI_COLOR32(0xFF, 0xE4, 0xBF, 0xFF), UI_COLOR32(0xFF, 0xA0, 0x31, 0xFF), UI_COLOR32(0xC8, 0x5E, 0x00, 0xFF), UI_TEXT},
    {UI_COLOR32(0xE0, 0xFB, 0xA8, 0xFF), UI_COLOR32(0xC5, 0xF2, 0x57, 0xFF), UI_COLOR32(0xA0, 0xD6, 0x11, 0xFF), UI_COLOR32(0xED, 0xFD, 0xC5, 0xFF), UI_COLOR32(0xBE, 0xEE, 0x4E, 0xFF), UI_COLOR32(0x6F, 0xA9, 0x00, 0xFF), UI_TEXT},
    {UI_COLOR32(0xC4, 0xF4, 0xBF, 0xFF), UI_COLOR32(0x86, 0xE8, 0x7E, 0xFF), UI_COLOR32(0x33, 0xC4, 0x49, 0xFF), UI_COLOR32(0xD8, 0xF9, 0xD3, 0xFF), UI_COLOR32(0x69, 0xDA, 0x6F, 0xFF), UI_COLOR32(0x0D, 0x99, 0x30, 0xFF), UI_TEXT},
    {UI_COLOR32(0xC6, 0xD9, 0xFF, 0xFF), UI_COLOR32(0x83, 0xB7, 0xFF, 0xFF), UI_COLOR32(0x35, 0x7C, 0xF5, 0xFF), UI_COLOR32(0xDE, 0xE8, 0xFF, 0xFF), UI_COLOR32(0x61, 0x9C, 0xFF, 0xFF), UI_COLOR32(0x1A, 0x52, 0xD8, 0xFF), UI_TEXT},
    {UI_COLOR32(0xF2, 0xC4, 0xFF, 0xFF), UI_COLOR32(0xE7, 0x87, 0xFF, 0xFF), UI_COLOR32(0xD8, 0x3A, 0xFA, 0xFF), UI_COLOR32(0xF7, 0xD8, 0xFF, 0xFF), UI_COLOR32(0xEC, 0x73, 0xFF, 0xFF), UI_COLOR32(0xB0, 0x1C, 0xDA, 0xFF), UI_TEXT},
};

#define UI_THEME_COUNT (sizeof(g_ui_room_themes) / sizeof(g_ui_room_themes[0]))

static size_t theme_index_for_text(const char* text)
{
    unsigned long hash = 5381UL;

    if (text == NULL || text[0] == '\0')
        return 0;

    while (*text != '\0') {
        hash = ((hash << 5) + hash) + (unsigned char)*text;
        text++;
    }

    return (size_t)(hash % UI_THEME_COUNT);
}

static const AppUiTheme* theme_for_conversation_id(const char* conversation_id)
{
    return &g_ui_room_themes[theme_index_for_text(conversation_id)];
}

static const AppUiTheme* active_room_theme(const HermesAppConfig* config)
{
    if (config == NULL || config->active_conversation_id[0] == '\0')
        return &g_ui_room_themes[0];

    return theme_for_conversation_id(config->active_conversation_id);
}

static const AppUiTheme* author_theme(AppUiMessageAuthor author)
{
    return author == APP_UI_MESSAGE_USER ? &g_ui_room_themes[1] : &g_ui_room_themes[4];
}

static const AppUiTheme* relay_theme(const BridgeHealthResult* health_result, const BridgeChatResult* chat_result)
{
    if (health_result != NULL && health_result->success)
        return &g_ui_room_themes[3];
    if (chat_result != NULL && chat_result->success)
        return &g_ui_room_themes[4];
    return &g_ui_room_themes[0];
}

bool hermes_app_ui_init(void)
{
    return app_gfx_init();
}

void hermes_app_ui_exit(void)
{
    app_gfx_fini();
}

static void draw_system_bar(float x, float y, float w, float h, const AppUiTheme* theme)
{
    float band_height = (h - 4.0f) / 3.0f;

    if (theme == NULL)
        theme = &g_ui_room_themes[0];

    app_gfx_panel(x, y, w, h, UI_PAPER, UI_BORDER_DARK);
    app_gfx_highlight_bar(x + 2.0f, y + 2.0f, w - 4.0f, band_height, theme->rail_top);
    app_gfx_highlight_bar(x + 2.0f, y + 2.0f + band_height, w - 4.0f, band_height, theme->rail_mid);
    app_gfx_highlight_bar(x + 2.0f, y + 2.0f + band_height * 2.0f, w - 4.0f, h - 4.0f - band_height * 2.0f, theme->rail_bottom);
    app_gfx_highlight_bar(x + 2.0f, y + h - 3.0f, w - 4.0f, 1.0f, theme->accent_dark);
}

static void draw_top_header(const AppUiTheme* theme, const char* subtitle)
{
    draw_system_bar(8.0f, 8.0f, 384.0f, 20.0f, theme);
    app_gfx_text(16.0f, 12.0f, 0.50f, 0.50f, UI_TEXT, "PICTOCHAT H");
    app_gfx_text_right(384.0f, 13.0f, 0.30f, 0.30f, UI_TEXT, subtitle);
}

static void draw_bottom_header(const AppUiTheme* theme, const char* title)
{
    draw_system_bar(8.0f, 10.0f, 304.0f, 18.0f, theme);
    app_gfx_text(16.0f, 13.0f, 0.40f, 0.40f, UI_TEXT, title);
}

static void draw_ruled_paper(float x, float y, float w, float h, u32 fill)
{
    int row;
    int column;

    app_gfx_panel(x, y, w, h, fill, UI_BORDER);
    for (row = 10; y + (float)row < y + h - 4.0f; row += 8)
        app_gfx_highlight_bar(x + 4.0f, y + (float)row, w - 8.0f, 1.0f, UI_RULE);
    for (column = 44; x + (float)column < x + w - 4.0f; column += 48)
        app_gfx_highlight_bar(x + (float)column, y + 4.0f, 1.0f, h - 8.0f, UI_RULE_STRONG);
}

static void draw_alert_banner(float x, float y, float w, float h, const char* text)
{
    int row;

    app_gfx_panel(x, y, w, h, UI_ALERT_BG, UI_ALERT_BORDER);
    for (row = 4; y + (float)row < y + h - 3.0f; row += 4)
        app_gfx_highlight_bar(x + 4.0f, y + (float)row, w - 8.0f, 1.0f, UI_ALERT_LINE);
    app_gfx_text_fit(x + 8.0f, y + 7.0f, w - 16.0f, 0.28f, 0.28f, UI_PAPER, text);
}

static void draw_chip(float x, float y, float w, const char* label, u32 fill, u32 text_color, u32 border)
{
    app_gfx_panel(x, y, w, 16.0f, fill, border);
    app_gfx_text_fit(x + 6.0f, y + 3.0f, w - 12.0f, 0.28f, 0.28f, text_color, label);
}

static float chip_width_for_label(const char* label)
{
    size_t length;
    float width;

    if (label == NULL || label[0] == '\0')
        return 44.0f;

    length = strlen(label);
    width = 24.0f + (float)length * 5.0f;
    if (width < 44.0f)
        width = 44.0f;
    if (width > 84.0f)
        width = 84.0f;
    return width;
}

static void draw_note_lines(float x, float y, float w, size_t count)
{
    size_t index;

    for (index = 0; index < count; index++)
        app_gfx_highlight_bar(x, y + (float)index * 9.0f, w, 1.0f, UI_RULE);
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

static void draw_menu_row(float x, float y, float w, const char* label, const AppUiTheme* theme)
{
    if (theme == NULL)
        theme = &g_ui_room_themes[0];

    app_gfx_panel(x, y, w, 18.0f, UI_PAPER, UI_BORDER);
    app_gfx_highlight_bar(x + 2.0f, y + 2.0f, 16.0f, 14.0f, theme->accent_light);
    app_gfx_highlight_bar(x + 2.0f, y + 2.0f, 4.0f, 14.0f, theme->accent);
    app_gfx_text_fit(x + 24.0f, y + 4.0f, w - 30.0f, 0.30f, 0.30f, UI_TEXT, label);
}

static void draw_action_button(float x, float y, float w, float h, const char* label, const AppUiTheme* theme, bool selected)
{
    if (theme == NULL)
        theme = &g_ui_room_themes[0];

    app_gfx_panel(x, y, w, h, selected ? UI_PAPER_ALT : UI_PAPER, selected ? theme->accent_dark : UI_BORDER_DARK);
    app_gfx_highlight_bar(x + 2.0f, y + 2.0f, w - 4.0f, 3.0f, selected ? theme->accent_light : UI_PAPER_SHADOW);
    app_gfx_highlight_bar(x + 7.0f, y + 6.0f, 18.0f, h - 12.0f, selected ? theme->accent : theme->accent_light);
    app_gfx_highlight_bar(x + 10.0f, y + 9.0f, 12.0f, 2.0f, theme->accent_dark);
    app_gfx_highlight_bar(x + 10.0f, y + 14.0f, 12.0f, 2.0f, theme->accent_dark);
    app_gfx_text_fit(x + 32.0f, y + 10.0f, w - 42.0f, 0.32f, 0.32f, UI_TEXT, label);
}

static void draw_hint_button(float x, float y, float w, const char* label, const AppUiTheme* theme)
{
    if (theme == NULL)
        theme = &g_ui_room_themes[0];

    app_gfx_panel(x, y, w, 16.0f, UI_PAPER, UI_BORDER);
    app_gfx_highlight_bar(x + 2.0f, y + 2.0f, 14.0f, 12.0f, theme->accent_light);
    app_gfx_text_fit(x + 20.0f, y + 4.0f, w - 24.0f, 0.26f, 0.26f, UI_TEXT, label);
}

static void draw_info_card(
    float x,
    float y,
    float w,
    float h,
    const char* label,
    const AppUiTheme* theme,
    char lines[][HOME_WRAP_LINE_MAX],
    size_t line_count,
    float max_width,
    size_t max_lines
)
{
    if (theme == NULL)
        theme = &g_ui_room_themes[0];

    draw_ruled_paper(x, y, w, h, UI_PAPER);
    draw_chip(x + 8.0f, y + 6.0f, chip_width_for_label(label), label, theme->accent, theme->accent_text, theme->accent_dark);
    draw_text_lines(x + 70.0f, y + 8.0f, 10.0f, max_width, 0.28f, 0.28f, UI_TEXT, lines, line_count, 0, max_lines);
}

static size_t wrap_message_card_text(const char* text, float max_width, float scale)
{
    return wrap_text_for_pixels(text, max_width, scale, scale, g_home_card_lines, HOME_HISTORY_CARD_LINES);
}

static void draw_message_card(
    float x,
    float y,
    float w,
    float h,
    const char* label,
    const char* text,
    size_t start_line,
    size_t max_lines,
    float text_scale,
    const char* page_label,
    const AppUiTheme* theme
)
{
    size_t total_lines;
    float chip_width;

    if (theme == NULL)
        theme = &g_ui_room_themes[0];

    total_lines = wrap_message_card_text(text, w - 42.0f, text_scale);
    if (start_line >= total_lines)
        start_line = 0;

    app_gfx_panel(x, y, w, h, UI_PAPER, theme->accent_dark);
    app_gfx_highlight_bar(x + 2.0f, y + 2.0f, w - 4.0f, 4.0f, theme->accent_light);
    app_gfx_highlight_bar(x + 24.0f, y + 28.0f, 1.0f, h - 36.0f, theme->accent_light);
    draw_note_lines(x + 10.0f, y + 32.0f, w - 20.0f, max_lines + 1);

    chip_width = chip_width_for_label(label);
    draw_chip(x + 10.0f, y + 8.0f, chip_width, label, theme->accent, theme->accent_text, theme->accent_dark);
    if (page_label != NULL && page_label[0] != '\0')
        draw_chip(x + w - 44.0f, y + 8.0f, 34.0f, page_label, UI_PAPER_ALT, UI_TEXT, theme->accent_dark);
    draw_text_lines(x + 30.0f, y + 34.0f, 9.0f, w - 40.0f, text_scale, text_scale, UI_TEXT, g_home_card_lines, total_lines, start_line, max_lines);
}

static const char* home_command_label_for_selection(size_t command_selection)
{
    switch ((HomeCommand)command_selection) {
        case HOME_COMMAND_CHECK:
            return "Check Relay";
        case HOME_COMMAND_THREADS:
            return "Rooms";
        case HOME_COMMAND_CONFIG:
            return "Setup";
        case HOME_COMMAND_MIC:
            return "Mic Note";
        case HOME_COMMAND_CLEAR:
            return "Clear Board";
        case HOME_COMMAND_NONE:
        case HOME_COMMAND_ASK:
        default:
            return "Write Note";
    }
}

static const char* home_command_detail_for_selection(size_t command_selection)
{
    switch ((HomeCommand)command_selection) {
        case HOME_COMMAND_CHECK:
            return "Check the Hermes relay and refresh the current room status.";
        case HOME_COMMAND_THREADS:
            return "Open the room book to switch rooms, create one, or sync recent entries.";
        case HOME_COMMAND_CONFIG:
            return "Open the setup sheet to edit host, port, token, and device ID.";
        case HOME_COMMAND_MIC:
            return "Record a short microphone note and send it through Hermes speech-to-text.";
        case HOME_COMMAND_CLEAR:
            return "Clear the visible board, relay state, and request status so the room starts clean again.";
        case HOME_COMMAND_NONE:
        case HOME_COMMAND_ASK:
        default:
            return "Open the keyboard and send a text note into the active Hermes room.";
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
    const AppUiTheme* room_theme;
    const AppUiTheme* status_theme;
    char conversation_label[HERMES_APP_CONVERSATION_ID_MAX];
    char page_label[24];
    size_t room_line_count;
    size_t status_line_count;
    size_t detail_line_count;
    size_t page_count = 1;
    size_t reply_start = 0;
    bool has_selection = command_selection <= (size_t)HOME_COMMAND_CLEAR;
    size_t selected_command = has_selection ? command_selection : (size_t)HOME_COMMAND_ASK;
    const char* latest_reply_text = NULL;
    size_t history_start;
    size_t index;
    float card_y = 72.0f;

    (void)last_message;

    room_theme = active_room_theme(config);
    status_theme = relay_theme(health_result, chat_result);
    format_active_conversation_label(config, conversation_list, conversation_label, sizeof(conversation_label));
    if (g_home_history_count > 0 && g_home_history[g_home_history_count - 1].author == APP_UI_MESSAGE_HERMES)
        latest_reply_text = g_home_history[g_home_history_count - 1].text;
    else if (chat_result != NULL && chat_result->success)
        latest_reply_text = chat_result->reply;

    if (latest_reply_text != NULL && latest_reply_text[0] != '\0')
        page_count = hermes_app_ui_reply_page_count(latest_reply_text);
    if (reply_page >= page_count)
        reply_page = page_count - 1;
    reply_start = reply_page * HOME_REPLY_LINES_PER_PAGE;
    snprintf(page_label, sizeof(page_label), "%lu/%lu", (unsigned long)(reply_page + 1), (unsigned long)page_count);

    room_line_count = wrap_text_for_pixels(
        conversation_label,
        110.0f,
        0.28f,
        0.28f,
        g_home_room_lines,
        2
    );
    status_line_count = wrap_text_for_pixels(
        status_line,
        104.0f,
        0.27f,
        0.27f,
        g_home_status_lines,
        2
    );
    detail_line_count = wrap_text_for_pixels(
        home_command_detail_for_selection(selected_command),
        176.0f,
        0.27f,
        0.27f,
        g_home_detail_lines,
        2
    );

    app_gfx_begin_top(UI_BG);
    app_gfx_panel(4.0f, 4.0f, 392.0f, 232.0f, UI_PAPER_SHADOW, UI_BORDER_DARK);
    draw_ruled_paper(8.0f, 8.0f, 384.0f, 224.0f, UI_PAPER);
    draw_top_header(room_theme, "ROOM BOARD");
    draw_info_card(16.0f, 36.0f, 176.0f, 28.0f, "ROOM", room_theme, g_home_room_lines, room_line_count, 110.0f, 2);
    draw_info_card(208.0f, 36.0f, 176.0f, 28.0f, "RELAY", status_theme, g_home_status_lines, status_line_count, 104.0f, 2);

    if (g_home_history_count == 0) {
        draw_message_card(
            16.0f,
            76.0f,
            368.0f,
            88.0f,
            "HERMES",
            "Press Write Note below to start a relay room.",
            0,
            5,
            0.28f,
            "",
            &g_ui_room_themes[4]
        );
    } else {
        history_start = g_home_history_count > 3 ? g_home_history_count - 3 : 0;
        for (index = history_start; index < g_home_history_count; index++) {
            const AppUiHomeHistoryEntry* entry = &g_home_history[index];
            bool latest = index == g_home_history_count - 1;
            bool latest_reply = latest && entry->author == APP_UI_MESSAGE_HERMES;
            float card_height = latest_reply ? 90.0f : 32.0f;
            size_t max_lines = latest_reply ? HOME_REPLY_LINES_PER_PAGE : 1;
            float text_scale = latest_reply ? HOME_REPLY_SCALE : 0.25f;

            if (card_y + card_height > 224.0f)
                break;

            draw_message_card(
                16.0f,
                card_y,
                368.0f,
                card_height,
                entry->author == APP_UI_MESSAGE_USER ? "YOU" : "HERMES",
                entry->text,
                latest_reply ? reply_start : 0,
                max_lines,
                text_scale,
                latest_reply ? page_label : "",
                author_theme(entry->author)
            );
            card_y += card_height + 6.0f;
        }

        if (g_home_history[g_home_history_count - 1].author != APP_UI_MESSAGE_HERMES && card_y + 46.0f <= 224.0f) {
            draw_message_card(
                16.0f,
                card_y,
                368.0f,
                46.0f,
                "NOTICE",
                status_line != NULL && status_line[0] != '\0' ? status_line : "Waiting for Hermes...",
                0,
                2,
                0.25f,
                "",
                status_theme
            );
        }
    }

    app_gfx_begin_bottom(UI_BG);
    app_gfx_panel(4.0f, 6.0f, 312.0f, 228.0f, UI_PAPER_SHADOW, UI_BORDER_DARK);
    draw_ruled_paper(8.0f, 10.0f, 304.0f, 220.0f, UI_PAPER);
    draw_bottom_header(room_theme, "TOOL TRAY");
    draw_action_button(16.0f, 44.0f, 136.0f, 28.0f, "Write Note", room_theme, selected_command == (size_t)HOME_COMMAND_ASK);
    draw_action_button(168.0f, 44.0f, 136.0f, 28.0f, "Check Relay", room_theme, selected_command == (size_t)HOME_COMMAND_CHECK);
    draw_action_button(16.0f, 80.0f, 136.0f, 28.0f, "Rooms", room_theme, selected_command == (size_t)HOME_COMMAND_THREADS);
    draw_action_button(168.0f, 80.0f, 136.0f, 28.0f, "Setup", room_theme, selected_command == (size_t)HOME_COMMAND_CONFIG);
    draw_action_button(16.0f, 116.0f, 136.0f, 28.0f, "Mic Note", room_theme, selected_command == (size_t)HOME_COMMAND_MIC);
    draw_action_button(168.0f, 116.0f, 136.0f, 28.0f, "Clear Board", room_theme, selected_command == (size_t)HOME_COMMAND_CLEAR);

    draw_ruled_paper(8.0f, 156.0f, 304.0f, 52.0f, UI_PAPER);
    app_gfx_text(18.0f, 164.0f, 0.28f, 0.28f, room_theme->accent_dark, "TOOL");
    app_gfx_text_fit(56.0f, 164.0f, 152.0f, 0.30f, 0.30f, UI_TEXT, home_command_label_for_selection(selected_command));
    draw_text_lines(18.0f, 178.0f, 10.0f, 184.0f, 0.27f, 0.27f, UI_MUTED, g_home_detail_lines, detail_line_count, 0, 2);
    app_gfx_text(224.0f, 164.0f, 0.28f, 0.28f, room_theme->accent_dark, "PAGE");
    app_gfx_text_right(300.0f, 164.0f, 0.30f, 0.30f, UI_TEXT, page_label);
    app_gfx_text_fit(224.0f, 178.0f, 76.0f, 0.27f, 0.27f, UI_MUTED, page_count > 1 ? "L/R flips notes" : "Ready to send");
    draw_hint_button(16.0f, 214.0f, 90.0f, "A Select", room_theme);
    draw_hint_button(112.0f, 214.0f, 90.0f, "Touch OK", room_theme);
    draw_hint_button(208.0f, 214.0f, 96.0f, "START Exit", room_theme);
}


static void render_settings_graphical(
    const HermesAppConfig* config,
    SettingsField selected_field,
    bool settings_dirty,
    const char* status_line,
    Result last_rc
)
{
    const AppUiTheme* theme = active_room_theme(config);
    char token_summary[48];
    float row_y = 74.0f;
    char port_line[16];

    (void)last_rc;

    format_token_summary(config, token_summary, sizeof(token_summary));
    snprintf(port_line, sizeof(port_line), "%u", (unsigned int)config->port);

    app_gfx_begin_top(UI_BG);
    app_gfx_panel(4.0f, 4.0f, 392.0f, 232.0f, UI_PAPER_SHADOW, UI_BORDER_DARK);
    draw_ruled_paper(8.0f, 8.0f, 384.0f, 224.0f, UI_PAPER);
    draw_top_header(theme, "SETUP SHEET");
    draw_chip(24.0f, 42.0f, 60.0f, "SETUP", theme->accent, theme->accent_text, theme->accent_dark);
    draw_chip(306.0f, 42.0f, 70.0f, settings_dirty ? "UNSAVED" : "SAVED", theme->accent_light, UI_TEXT, theme->accent_dark);

    app_gfx_panel(20.0f, row_y, 164.0f, 28.0f, selected_field == SETTINGS_FIELD_HOST ? UI_PAPER_ALT : UI_PAPER, selected_field == SETTINGS_FIELD_HOST ? theme->accent_dark : UI_BORDER);
    app_gfx_highlight_bar(22.0f, row_y + 2.0f, 18.0f, 24.0f, selected_field == SETTINGS_FIELD_HOST ? theme->accent : theme->accent_light);
    app_gfx_text_fit(48.0f, row_y + 8.0f, 130.0f, 0.34f, 0.34f, UI_TEXT, "Host");
    app_gfx_panel(196.0f, row_y, 180.0f, 28.0f, selected_field == SETTINGS_FIELD_HOST ? UI_PAPER_ALT : UI_PAPER, selected_field == SETTINGS_FIELD_HOST ? theme->accent_dark : UI_BORDER);
    app_gfx_text_fit(204.0f, row_y + 8.0f, 164.0f, 0.32f, 0.32f, UI_TEXT, config->host);

    row_y += 34.0f;
    app_gfx_panel(20.0f, row_y, 164.0f, 28.0f, selected_field == SETTINGS_FIELD_PORT ? UI_PAPER_ALT : UI_PAPER, selected_field == SETTINGS_FIELD_PORT ? theme->accent_dark : UI_BORDER);
    app_gfx_highlight_bar(22.0f, row_y + 2.0f, 18.0f, 24.0f, selected_field == SETTINGS_FIELD_PORT ? theme->accent : theme->accent_light);
    app_gfx_text_fit(48.0f, row_y + 8.0f, 130.0f, 0.34f, 0.34f, UI_TEXT, "Port");
    app_gfx_panel(196.0f, row_y, 180.0f, 28.0f, selected_field == SETTINGS_FIELD_PORT ? UI_PAPER_ALT : UI_PAPER, selected_field == SETTINGS_FIELD_PORT ? theme->accent_dark : UI_BORDER);
    app_gfx_text_fit(204.0f, row_y + 8.0f, 164.0f, 0.32f, 0.32f, UI_TEXT, port_line);

    row_y += 34.0f;
    app_gfx_panel(20.0f, row_y, 164.0f, 28.0f, selected_field == SETTINGS_FIELD_TOKEN ? UI_PAPER_ALT : UI_PAPER, selected_field == SETTINGS_FIELD_TOKEN ? theme->accent_dark : UI_BORDER);
    app_gfx_highlight_bar(22.0f, row_y + 2.0f, 18.0f, 24.0f, selected_field == SETTINGS_FIELD_TOKEN ? theme->accent : theme->accent_light);
    app_gfx_text_fit(48.0f, row_y + 8.0f, 130.0f, 0.34f, 0.34f, UI_TEXT, "Token");
    app_gfx_panel(196.0f, row_y, 180.0f, 28.0f, selected_field == SETTINGS_FIELD_TOKEN ? UI_PAPER_ALT : UI_PAPER, selected_field == SETTINGS_FIELD_TOKEN ? theme->accent_dark : UI_BORDER);
    app_gfx_text_fit(204.0f, row_y + 8.0f, 164.0f, 0.30f, 0.30f, UI_TEXT, token_summary);

    row_y += 34.0f;
    app_gfx_panel(20.0f, row_y, 164.0f, 28.0f, selected_field == SETTINGS_FIELD_DEVICE_ID ? UI_PAPER_ALT : UI_PAPER, selected_field == SETTINGS_FIELD_DEVICE_ID ? theme->accent_dark : UI_BORDER);
    app_gfx_highlight_bar(22.0f, row_y + 2.0f, 18.0f, 24.0f, selected_field == SETTINGS_FIELD_DEVICE_ID ? theme->accent : theme->accent_light);
    app_gfx_text_fit(48.0f, row_y + 8.0f, 130.0f, 0.34f, 0.34f, UI_TEXT, "Device ID");
    app_gfx_panel(196.0f, row_y, 180.0f, 28.0f, selected_field == SETTINGS_FIELD_DEVICE_ID ? UI_PAPER_ALT : UI_PAPER, selected_field == SETTINGS_FIELD_DEVICE_ID ? theme->accent_dark : UI_BORDER);
    app_gfx_text_fit(204.0f, row_y + 8.0f, 164.0f, 0.30f, 0.30f, UI_TEXT, config->device_id[0] != '\0' ? config->device_id : "<empty>");

    draw_ruled_paper(20.0f, 210.0f, 356.0f, 14.0f, UI_PAPER_ALT);
    app_gfx_text_fit(26.0f, 212.0f, 344.0f, 0.24f, 0.24f, UI_MUTED, status_line);

    app_gfx_begin_bottom(UI_BG);
    app_gfx_panel(4.0f, 6.0f, 312.0f, 228.0f, UI_PAPER_SHADOW, UI_BORDER_DARK);
    draw_ruled_paper(8.0f, 10.0f, 304.0f, 220.0f, UI_PAPER);
    draw_bottom_header(theme, "SETUP KEYS");
    draw_ruled_paper(8.0f, 44.0f, 304.0f, 138.0f, UI_PAPER);
    draw_menu_row(16.0f, 56.0f, 288.0f, "A Edit field", theme);
    draw_menu_row(16.0f, 78.0f, 288.0f, "X Save card", theme);
    draw_menu_row(16.0f, 100.0f, 288.0f, "Y Restore defaults", theme);
    draw_menu_row(16.0f, 122.0f, 288.0f, "B Return to board", theme);
    draw_menu_row(16.0f, 144.0f, 288.0f, "START Exit app", theme);
    draw_ruled_paper(8.0f, 190.0f, 304.0f, 30.0f, UI_PAPER_ALT);
    app_gfx_text_fit(16.0f, 198.0f, 288.0f, 0.24f, 0.24f, UI_MUTED, "Setup is stored on sdmc:/3ds/hermes-agent-3ds/config.ini");
}

static void render_conversations_graphical(
    const HermesAppConfig* config,
    const char* status_line,
    Result last_rc,
    const BridgeV2ConversationListResult* conversation_list,
    size_t conversation_selection
)
{
    const AppUiTheme* theme = active_room_theme(config);
    size_t visible = 4;
    size_t start = 0;
    size_t end;
    size_t index;

    (void)last_rc;

    app_gfx_begin_top(UI_BG);
    app_gfx_panel(4.0f, 4.0f, 392.0f, 232.0f, UI_PAPER_SHADOW, UI_BORDER_DARK);
    draw_ruled_paper(8.0f, 8.0f, 384.0f, 224.0f, UI_PAPER);
    draw_top_header(theme, "ROOM BOOK");
    draw_info_card(16.0f, 36.0f, 368.0f, 28.0f, "STATUS", theme, g_home_status_lines,
        wrap_text_for_pixels(status_line, 300.0f, 0.27f, 0.27f, g_home_status_lines, 2), 298.0f, 2);

    if (config == NULL || config->recent_conversation_count == 0) {
        draw_message_card(16.0f, 78.0f, 368.0f, 72.0f, "NOTICE", "No rooms are saved yet. Create one from the lower keys.", 0, 4, 0.27f, "", theme);
    } else {
        if (conversation_selection >= visible)
            start = conversation_selection - (visible - 1);
        end = start + visible;
        if (end > config->recent_conversation_count)
            end = config->recent_conversation_count;

        for (index = start; index < end; index++) {
            const BridgeV2ConversationInfo* info = find_synced_conversation(conversation_list, config->recent_conversations[index]);
            const char* title = (info != NULL && info->title[0] != '\0') ? info->title : config->recent_conversations[index];
            const AppUiTheme* row_theme = theme_for_conversation_id(config->recent_conversations[index]);
            float row_y = 78.0f + (float)(index - start) * 34.0f;
            bool selected = index == conversation_selection;

            app_gfx_panel(16.0f, row_y, 368.0f, 28.0f, selected ? UI_PAPER_ALT : UI_PAPER, selected ? row_theme->accent_dark : UI_BORDER);
            app_gfx_highlight_bar(18.0f, row_y + 2.0f, 18.0f, 24.0f, selected ? row_theme->accent : row_theme->accent_light);
            draw_chip(42.0f, row_y + 6.0f, 48.0f, "ROOM", row_theme->accent, row_theme->accent_text, row_theme->accent_dark);
            app_gfx_text_fit(96.0f, row_y + 5.0f, 280.0f, 0.30f, 0.30f, UI_TEXT, title);
            app_gfx_text_fit(96.0f, row_y + 15.0f, 280.0f, 0.24f, 0.24f, UI_MUTED,
                info != NULL && info->preview[0] != '\0' ? info->preview : config->recent_conversations[index]);
        }
    }

    app_gfx_begin_bottom(UI_BG);
    app_gfx_panel(4.0f, 6.0f, 312.0f, 228.0f, UI_PAPER_SHADOW, UI_BORDER_DARK);
    draw_ruled_paper(8.0f, 10.0f, 304.0f, 220.0f, UI_PAPER);
    draw_bottom_header(theme, "ROOM KEYS");
    draw_ruled_paper(8.0f, 44.0f, 304.0f, 138.0f, UI_PAPER);
    draw_menu_row(16.0f, 56.0f, 288.0f, "A Use room", theme);
    draw_menu_row(16.0f, 78.0f, 288.0f, "X New room", theme);
    draw_menu_row(16.0f, 100.0f, 288.0f, "Y Sync rooms", theme);
    draw_menu_row(16.0f, 122.0f, 288.0f, "B Return to board", theme);
    draw_menu_row(16.0f, 144.0f, 288.0f, "START Exit app", theme);
    draw_ruled_paper(8.0f, 190.0f, 304.0f, 30.0f, UI_PAPER_ALT);
    app_gfx_text_fit(16.0f, 198.0f, 288.0f, 0.24f, 0.24f, UI_MUTED, "Rooms are Hermes conversation IDs with a PictoChat shell.");
}

void hermes_app_ui_render_approval_prompt(const char* request_id)
{
    const AppUiTheme* theme = &g_ui_room_themes[0];
    char request_line[96];

    if (request_id != NULL && request_id[0] != '\0')
        snprintf(request_line, sizeof(request_line), "Request %s", request_id);
    else
        snprintf(request_line, sizeof(request_line), "Approval request pending");

    app_gfx_begin_frame();

    app_gfx_begin_top(UI_BG);
    app_gfx_panel(4.0f, 4.0f, 392.0f, 232.0f, UI_PAPER_SHADOW, UI_BORDER_DARK);
    draw_ruled_paper(8.0f, 8.0f, 384.0f, 224.0f, UI_PAPER);
    draw_top_header(theme, "APPROVAL");
    draw_alert_banner(34.0f, 46.0f, 332.0f, 24.0f, "Relay approval needed");
    draw_ruled_paper(24.0f, 80.0f, 352.0f, 132.0f, UI_PAPER);
    app_gfx_text(36.0f, 94.0f, 0.34f, 0.34f, UI_TEXT, "Hermes is waiting for permission.");
    app_gfx_text_fit(36.0f, 112.0f, 328.0f, 0.28f, 0.28f, UI_MUTED, request_line);
    app_gfx_text_fit(36.0f, 140.0f, 328.0f, 0.28f, 0.28f, UI_TEXT, "Choose how long to allow this action, or deny it.");
    app_gfx_text_fit(36.0f, 166.0f, 328.0f, 0.26f, 0.26f, UI_MUTED, "A and X allow the action. Y keeps allowing it. B denies it.");

    app_gfx_begin_bottom(UI_BG);
    app_gfx_panel(4.0f, 6.0f, 312.0f, 228.0f, UI_PAPER_SHADOW, UI_BORDER_DARK);
    draw_ruled_paper(8.0f, 10.0f, 304.0f, 220.0f, UI_PAPER);
    draw_bottom_header(theme, "APPROVAL KEYS");
    draw_action_button(16.0f, 48.0f, 136.0f, 28.0f, "A Once", &g_ui_room_themes[0], false);
    draw_action_button(168.0f, 48.0f, 136.0f, 28.0f, "X Session", &g_ui_room_themes[4], false);
    draw_action_button(16.0f, 84.0f, 136.0f, 28.0f, "Y Always", &g_ui_room_themes[3], false);
    draw_action_button(168.0f, 84.0f, 136.0f, 28.0f, "B Deny", &g_ui_room_themes[1], false);
    draw_ruled_paper(8.0f, 128.0f, 304.0f, 54.0f, UI_PAPER_ALT);
    app_gfx_text_fit(16.0f, 138.0f, 288.0f, 0.26f, 0.26f, UI_TEXT, "This is a Hermes gateway approval, styled like a DS system alert.");
    app_gfx_text_fit(16.0f, 156.0f, 288.0f, 0.24f, 0.24f, UI_MUTED, "Press START to cancel without answering.");
    draw_hint_button(82.0f, 208.0f, 156.0f, "START Cancel", theme);

    app_gfx_end_frame();
}

void hermes_app_ui_render_voice_recording(unsigned long tenths, size_t pcm_size, const char* status_line, bool waiting_for_up_release)
{
    const AppUiTheme* theme = &g_ui_room_themes[1];
    char time_line[32];
    char audio_line[48];
    const char* capture_state = waiting_for_up_release ? "Release upward input to arm stop." : "Move upward to stop and send.";

    snprintf(time_line, sizeof(time_line), "%lu.%lus", tenths / 10, tenths % 10);
    snprintf(audio_line, sizeof(audio_line), "%lu bytes", (unsigned long)pcm_size);

    app_gfx_begin_frame();

    app_gfx_begin_top(UI_BG);
    app_gfx_panel(4.0f, 4.0f, 392.0f, 232.0f, UI_PAPER_SHADOW, UI_BORDER_DARK);
    draw_ruled_paper(8.0f, 8.0f, 384.0f, 224.0f, UI_PAPER);
    draw_top_header(theme, "MIC NOTE");
    draw_alert_banner(32.0f, 44.0f, 336.0f, 24.0f, waiting_for_up_release ? "Release UP to arm send" : "Recording microphone note");
    draw_ruled_paper(20.0f, 76.0f, 360.0f, 136.0f, UI_PAPER);
    draw_chip(32.0f, 88.0f, 44.0f, "TIME", theme->accent, theme->accent_text, theme->accent_dark);
    app_gfx_panel(86.0f, 84.0f, 110.0f, 24.0f, UI_PAPER_ALT, UI_BORDER);
    app_gfx_text_fit(96.0f, 92.0f, 90.0f, 0.30f, 0.30f, UI_TEXT, time_line);
    draw_chip(224.0f, 88.0f, 52.0f, "AUDIO", theme->accent, theme->accent_text, theme->accent_dark);
    app_gfx_panel(284.0f, 84.0f, 76.0f, 24.0f, UI_PAPER_ALT, UI_BORDER);
    app_gfx_text_fit(292.0f, 92.0f, 60.0f, 0.26f, 0.26f, UI_TEXT, audio_line);
    app_gfx_text_fit(32.0f, 128.0f, 316.0f, 0.28f, 0.28f, UI_TEXT, status_line != NULL && status_line[0] != '\0' ? status_line : "Mic is recording now.");
    app_gfx_text_fit(32.0f, 154.0f, 316.0f, 0.26f, 0.26f, UI_MUTED, capture_state);
    app_gfx_text_fit(32.0f, 176.0f, 316.0f, 0.24f, 0.24f, UI_MUTED, "Voice notes keep the PictoChat shell and route through Hermes speech-to-text.");

    app_gfx_begin_bottom(UI_BG);
    app_gfx_panel(4.0f, 6.0f, 312.0f, 228.0f, UI_PAPER_SHADOW, UI_BORDER_DARK);
    draw_ruled_paper(8.0f, 10.0f, 304.0f, 220.0f, UI_PAPER);
    draw_bottom_header(theme, "MIC KEYS");
    draw_action_button(16.0f, 48.0f, 88.0f, 28.0f, "UP Send", theme, !waiting_for_up_release);
    draw_action_button(116.0f, 48.0f, 88.0f, 28.0f, "B Cancel", &g_ui_room_themes[5], false);
    draw_action_button(216.0f, 48.0f, 88.0f, 28.0f, "START Abort", &g_ui_room_themes[4], false);
    draw_ruled_paper(8.0f, 92.0f, 304.0f, 116.0f, UI_PAPER_ALT);
    app_gfx_text_fit(16.0f, 104.0f, 288.0f, 0.28f, 0.28f, UI_TEXT, capture_state);
    app_gfx_text_fit(16.0f, 126.0f, 288.0f, 0.24f, 0.24f, UI_MUTED, "The session stops after five minutes or when the audio buffer fills.");
    app_gfx_text_fit(16.0f, 148.0f, 288.0f, 0.24f, 0.24f, UI_MUTED, "If UP is already held, release it once before trying to send.");
    draw_hint_button(82.0f, 214.0f, 156.0f, "Return after send", theme);

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
