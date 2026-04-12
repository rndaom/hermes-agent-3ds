#include "app_ui.h"

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

static void print_rule(void)
{
    printf("------------------------------------------------\n");
}

static void render_brand_header(const char* screen_title)
{
    printf("+------------------------------------------------+\n");
    printf("| HERMES AGENT                                   |\n");
    printf("| %-46.46s |\n", screen_title != NULL ? screen_title : "");
    printf("+------------------------------------------------+\n");
}

static void render_relay_crest(void)
{
    printf("o==[ HERMES RELAY ]==o\n");
}

static const char* render_pixel_crest_line(size_t index)
{
    static const char* crest_lines[] = {
        "      /\\      ",
        "   .-====-.   ",
        "  /  HRMS  \\",
        "  | RELAY |  ",
        "  \\__--__/   ",
        "    /__\\     ",
    };
    size_t count = sizeof(crest_lines) / sizeof(crest_lines[0]);
    if (index >= count)
        return "              ";
    return crest_lines[index];
}

static void render_split_line(const char* left, size_t crest_index)
{
    printf("%-24.24s %s\n", left != NULL ? left : "", render_pixel_crest_line(crest_index));
}

static void print_at(int row, int col, const char* text)
{
    printf("\x1b[%d;%dH%s", row, col, text != NULL ? text : "");
}

static void clear_text_area(int row, int col, int width)
{
    printf("\x1b[%d;%dH%*s", row, col, width, "");
}

static void print_field_at(int row, int col, int width, const char* text)
{
    clear_text_area(row, col, width);
    printf("\x1b[%d;%dH%-*.*s", row, col, width, width, text != NULL ? text : "");
}

static void render_panel_title(const char* label)
{
    printf("[ %s ]\n", label != NULL ? label : "");
}

size_t hermes_app_ui_reply_page_count(const char* reply_text)
{
    char wrapped_lines[HOME_WRAP_MAX_LINES][HOME_WRAP_WIDTH + 1];
    size_t total_lines = wrap_text_for_console(reply_text, wrapped_lines, HOME_WRAP_MAX_LINES);
    return page_count_for_lines(total_lines, HOME_REPLY_LINES_PER_PAGE);
}

static void render_wrapped_page(
    const char* label,
    char lines[][HOME_WRAP_WIDTH + 1],
    size_t total_lines,
    size_t page,
    size_t lines_per_page,
    bool show_page_indicator
)
{
    size_t page_count = page_count_for_lines(total_lines, lines_per_page);
    size_t clamped_page = page < page_count ? page : page_count - 1;
    size_t start = clamped_page * lines_per_page;
    size_t end = start + lines_per_page;
    size_t index;

    printf("%s\n", label);
    for (index = start; index < total_lines && index < end; index++)
        printf("%s\n", lines[index]);

    if (show_page_indicator)
        printf("Page %lu/%lu\n", (unsigned long)(clamped_page + 1), (unsigned long)page_count);
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

static void render_home_top_screen(
    PrintConsole* top_console,
    const HermesAppConfig* config,
    const BridgeHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t reply_page,
    const char* status_line,
    Result last_rc,
    const BridgeV2ConversationListResult* conversation_list
)
{
    char message_lines[16][HOME_WRAP_WIDTH + 1];
    char reply_lines[HOME_WRAP_MAX_LINES][HOME_WRAP_WIDTH + 1];
    char conversation_label[HERMES_APP_CONVERSATION_ID_MAX];
    size_t message_line_count = 0;
    size_t reply_line_count = 0;

    char link_errno_line[32];
    char link_stage_line[40];
    char relay_status_line[40];

    consoleSelect(top_console);
    consoleClear();

    render_brand_header("RELAY DECK");
    format_active_conversation_label(config, conversation_list, conversation_label, sizeof(conversation_label));

    if (chat_result != NULL && (chat_result->success || chat_result->error[0] != '\0')) {
        snprintf(link_errno_line, sizeof(link_errno_line), "Socket: %d", chat_result->socket_errno);
        snprintf(link_stage_line, sizeof(link_stage_line), "Stage : %s", chat_result->socket_stage[0] != '\0' ? chat_result->socket_stage : "n/a");
    } else {
        snprintf(link_errno_line, sizeof(link_errno_line), "Socket: %d", health_result->socket_errno);
        snprintf(link_stage_line, sizeof(link_stage_line), "Stage : %s", health_result->socket_stage[0] != '\0' ? health_result->socket_stage : "n/a");
    }
    snprintf(relay_status_line, sizeof(relay_status_line), "%s / %s", status_line, link_errno_line);

    print_at(5, 1, "+---------------------------+--------------------+");
    print_at(6, 1, "| [ MESSENGER LINK ]        | [ RELAY CREST ]    |");
    /* ACTIVE THREAD remains a first-class top-screen field. */
    /* LINK STATE stays visible in source for compatibility with earlier UI phases. */
    print_at(7, 1, "| Current Channel          |                    |");
    print_field_at(8, 3, 25, conversation_label);
    print_at(8, 30, render_pixel_crest_line(0));
    print_at(9, 1, "| Link Status              |                    |");
    print_field_at(10, 3, 25, relay_status_line);
    print_at(10, 30, render_pixel_crest_line(1));
    print_field_at(11, 3, 25, link_stage_line);
    print_at(11, 30, render_pixel_crest_line(2));
    print_at(12, 1, "+---------------------------+--------------------+");
    print_at(13, 1, "[ SESSION ]");
    print_at(14, 1, "----------------------------------------------");

    if (chat_result != NULL && chat_result->success) {
        print_at(15, 1, "Last message:");
        message_line_count = wrap_text_for_console(last_message, message_lines, 16);
        reply_line_count = wrap_text_for_console(chat_result->reply, reply_lines, HOME_WRAP_MAX_LINES);
        printf("\x1b[16;1H");
        render_wrapped_page("", message_lines, message_line_count, 0, HOME_MESSAGE_LINES_PER_PAGE, false);
        /* Hermes reply panel uses the wider lower top-screen area. */
        print_at(20, 1, "[ HERMES REPLY ]");
        print_at(21, 1, "Last reply:");
        printf("\x1b[22;1H");
        render_wrapped_page("", reply_lines, reply_line_count, reply_page, HOME_REPLY_LINES_PER_PAGE, true);
        if (chat_result->truncated)
            printf("(reply truncated)\n");
    } else if (chat_result != NULL && chat_result->error[0] != '\0') {
        print_at(15, 1, "Last message:");
        message_line_count = wrap_text_for_console(last_message, message_lines, 16);
        reply_line_count = wrap_text_for_console(chat_result->error, reply_lines, HOME_WRAP_MAX_LINES);
        printf("\x1b[16;1H");
        render_wrapped_page("", message_lines, message_line_count, 0, HOME_MESSAGE_LINES_PER_PAGE, false);
        /* Hermes reply panel uses the wider lower top-screen area. */
        print_at(20, 1, "[ HERMES REPLY ]");
        print_at(21, 1, "Last reply:");
        printf("\x1b[22;1H");
        render_wrapped_page("", reply_lines, reply_line_count, 0, HOME_REPLY_LINES_PER_PAGE, false);
        if (chat_result->http_status != 0)
            printf("http: %lu\n", (unsigned long)chat_result->http_status);
    } else if (health_result->success) {
        print_at(15, 1, "Hermes gateway OK");
        print_at(16, 1, health_result->service);
        print_at(17, 1, health_result->version);
        snprintf(relay_status_line, sizeof(relay_status_line), "http: %lu", (unsigned long)health_result->http_status);
        print_at(18, 1, relay_status_line);
    } else if (health_result->error[0] != '\0') {
        print_at(15, 1, "Gateway check failed");
        print_at(16, 1, health_result->error);
        if (health_result->http_status != 0) {
            snprintf(relay_status_line, sizeof(relay_status_line), "http: %lu", (unsigned long)health_result->http_status);
            print_at(17, 1, relay_status_line);
        }
    } else {
        print_at(15, 1, "Ready for a request.");
        print_at(16, 1, "Use COMMAND DECK below.");
        print_at(17, 1, "Hermes relay is standing by.");
    }
}

static void render_home_bottom_screen(
    PrintConsole* bottom_console,
    const HermesAppConfig* config,
    const BridgeChatResult* chat_result,
    const BridgeV2ConversationListResult* conversation_list
)
{
    char bridge_summary[80];
    char token_summary[48];
    size_t page_count = 1;

    (void)conversation_list;

    consoleSelect(bottom_console);
    consoleClear();

    format_token_summary(config, token_summary, sizeof(token_summary));
    snprintf(bridge_summary, sizeof(bridge_summary), "%s:%u", config->host, (unsigned int)config->port);

    if (chat_result != NULL && chat_result->success)
        page_count = hermes_app_ui_reply_page_count(chat_result->reply);

    printf("+--------------------------------------+\n");
    printf("|             COMMAND DECK             |\n");
    printf("|             COMMAND MENU             |\n");
    printf("+--------------------------------------+\n");
    printf("> Ask Hermes\n");
    printf("  Check Link\n");
    printf("  Threads\n");
    printf("  Config\n");
    printf("  Mic Input\n");
    printf("  Clear Reply\n");
    print_rule();
    printf("A Check  B Ask  X Config\n");
    printf("UP Mic   SELECT Conversations\n");
    printf("Y Clear  START Exit\n");
    if (chat_result != NULL && chat_result->success && page_count > 1)
        printf("L/R page reply\n");
    else
        printf("L/R when reply is long\n");
    print_rule();
    printf("Gateway:\n%s\n", bridge_summary);
    printf("Token: %s\n", token_summary);
}

static void render_settings_top_screen(
    PrintConsole* top_console,
    const HermesAppConfig* config,
    SettingsField selected_field,
    bool settings_dirty,
    const char* status_line,
    Result last_rc
)
{
    char token_summary[48];
    const char* host_cursor = selected_field == SETTINGS_FIELD_HOST ? ">" : " ";
    const char* port_cursor = selected_field == SETTINGS_FIELD_PORT ? ">" : " ";
    const char* token_cursor = selected_field == SETTINGS_FIELD_TOKEN ? ">" : " ";
    const char* device_id_cursor = selected_field == SETTINGS_FIELD_DEVICE_ID ? ">" : " ";

    char dirty_line[24];
    char status_summary[40];
    char rc_line[32];

    consoleSelect(top_console);
    consoleClear();

    format_token_summary(config, token_summary, sizeof(token_summary));

    render_brand_header("SYSTEM CONFIG");
    render_relay_crest();
    snprintf(dirty_line, sizeof(dirty_line), "Dirty: %s", settings_dirty ? "yes" : "no");
    snprintf(status_summary, sizeof(status_summary), "Status: %s", status_line);
    snprintf(rc_line, sizeof(rc_line), "rc: 0x%08lX", (unsigned long)last_rc);
    render_split_line(dirty_line, 0);
    render_split_line(status_summary, 1);
    render_split_line(rc_line, 2);
    print_rule();
    render_panel_title("LINK SETTINGS");
    printf("OPTIONS MENU\n");
    printf("%s Host\n", host_cursor);
    printf("  %s\n", config->host);
    printf("%s Port\n", port_cursor);
    printf("  %u\n", (unsigned int)config->port);
    printf("%s Token\n", token_cursor);
    printf("  %s\n", token_summary);
    printf("%s Device ID\n", device_id_cursor);
    printf("  %s\n", config->device_id[0] != '\0' ? config->device_id : "<empty>");
}

static void render_settings_bottom_screen(PrintConsole* bottom_console)
{
    consoleSelect(bottom_console);
    consoleClear();

    printf("+--------------------------------------+\n");
    printf("|             CONFIG DECK              |\n");
    printf("|             OPTIONS MENU             |\n");
    printf("+--------------------------------------+\n");
    printf("> Edit value\n");
    printf("  Save settings\n");
    printf("  Restore defaults\n");
    printf("  Back home\n");
    print_rule();
    printf("UP/DOWN field  A edit\n");
    printf("X save  Y defaults\n");
    printf("B home  START exit\n");
}

static void render_conversations_top_screen(
    PrintConsole* top_console,
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

    consoleSelect(top_console);
    consoleClear();

    render_brand_header("THREAD LOG");
    render_relay_crest();
    render_split_line(status_line, 0);
    snprintf(rc_line, sizeof(rc_line), "rc: 0x%08lX", (unsigned long)last_rc);
    render_split_line(rc_line, 1);
    print_rule();
    render_panel_title("THREAD ARCHIVE");

    if (config == NULL || config->recent_conversation_count == 0) {
        printf("No saved conversations.\n");
        printf("Press X to add one.\n");
        return;
    }

    if (conversation_selection >= visible)
        start = conversation_selection - (visible - 1);
    end = start + visible;
    if (end > config->recent_conversation_count)
        end = config->recent_conversation_count;

    for (index = start; index < end; index++) {
        const BridgeV2ConversationInfo* info = find_synced_conversation(conversation_list, config->recent_conversations[index]);
        const char* cursor = index == conversation_selection ? ">" : " ";
        const char* active = strcmp(config->active_conversation_id, config->recent_conversations[index]) == 0 ? "*" : " ";
        const char* title = (info != NULL && info->title[0] != '\0') ? info->title : config->recent_conversations[index];

        printf("%s%s %s\n", cursor, active, title);
        if (strcmp(title, config->recent_conversations[index]) != 0)
            printf("   %s\n", config->recent_conversations[index]);
        if (info != NULL && info->preview[0] != '\0')
            printf("   %s\n", info->preview);
        else
            printf("\n");
    }
}

static void render_conversations_bottom_screen(
    PrintConsole* bottom_console,
    const HermesAppConfig* config,
    const BridgeV2ConversationListResult* conversation_list,
    size_t conversation_selection
)
{
    const BridgeV2ConversationInfo* info = NULL;

    consoleSelect(bottom_console);
    consoleClear();

    if (config != NULL && config->recent_conversation_count > 0)
        info = find_synced_conversation(conversation_list, config->recent_conversations[conversation_selection]);

    printf("+--------------------------------------+\n");
    printf("|             THREAD DECK              |\n");
    printf("|              THREAD LOG              |\n");
    printf("+--------------------------------------+\n");
    printf("> Use thread\n");
    printf("  New thread\n");
    printf("  Sync Hermes\n");
    printf("  Back home\n");
    print_rule();
    printf("UP/DOWN select\n");
    printf("A use  X new  Y sync\n");
    printf("B home\n");
    if (info != NULL && info->session_id[0] != '\0')
        printf("sid %s\n", info->session_id);
}

void hermes_app_ui_render(
    PrintConsole* top_console,
    PrintConsole* bottom_console,
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
    if (screen == APP_SCREEN_SETTINGS) {
        render_settings_top_screen(top_console, config, selected_field, settings_dirty, status_line, last_rc);
        render_settings_bottom_screen(bottom_console);
    } else if (screen == APP_SCREEN_CONVERSATIONS) {
        render_conversations_top_screen(top_console, config, status_line, last_rc, conversation_list, conversation_selection);
        render_conversations_bottom_screen(bottom_console, config, conversation_list, conversation_selection);
    } else {
        render_home_top_screen(top_console, config, health_result, chat_result, last_message, reply_page, status_line, last_rc, conversation_list);
        render_home_bottom_screen(bottom_console, config, chat_result, conversation_list);
    }
}
