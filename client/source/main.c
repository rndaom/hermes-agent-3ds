#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "bridge_chat.h"
#include "bridge_health.h"

#define HOME_WRAP_WIDTH 38
#define HOME_WRAP_MAX_LINES 128
#define HOME_MESSAGE_LINES_PER_PAGE 3
#define HOME_REPLY_LINES_PER_PAGE 10

static PrintConsole top_console;
static PrintConsole bottom_console;

typedef enum AppScreen {
    APP_SCREEN_HOME = 0,
    APP_SCREEN_SETTINGS = 1,
} AppScreen;

typedef enum SettingsField {
    SETTINGS_FIELD_HOST = 0,
    SETTINGS_FIELD_PORT = 1,
    SETTINGS_FIELD_TOKEN = 2,
    SETTINGS_FIELD_COUNT = 3,
} SettingsField;

static const char* settings_field_label(SettingsField field)
{
    switch (field) {
        case SETTINGS_FIELD_HOST:
            return "Host";
        case SETTINGS_FIELD_PORT:
            return "Port";
        case SETTINGS_FIELD_TOKEN:
            return "Token";
        default:
            return "Unknown";
    }
}

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

static size_t wrapped_page_count(const char* input, size_t lines_per_page)
{
    char wrapped_lines[HOME_WRAP_MAX_LINES][HOME_WRAP_WIDTH + 1];
    size_t total_lines = wrap_text_for_console(input, wrapped_lines, HOME_WRAP_MAX_LINES);
    return page_count_for_lines(total_lines, lines_per_page);
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

static bool prompt_text_input(SwkbdType type, const char* hint, const char* initial_text, char* out_text, size_t out_size, bool password_mode)
{
    SwkbdState swkbd;
    SwkbdButton button;

    if (out_text == NULL || out_size == 0)
        return false;

    out_text[0] = '\0';
    swkbdInit(&swkbd, type, 1, (int)out_size - 1);
    if (initial_text != NULL && initial_text[0] != '\0')
        swkbdSetInitialText(&swkbd, initial_text);
    if (hint != NULL)
        swkbdSetHintText(&swkbd, hint);

    if (password_mode)
        swkbdSetPasswordMode(&swkbd, SWKBD_PASSWORD_HIDE_DELAY);

    if (type == SWKBD_TYPE_NUMPAD) {
        swkbdSetValidation(&swkbd, SWKBD_ANYTHING, 0, 0);
        swkbdSetFeatures(&swkbd, SWKBD_FIXED_WIDTH);
    } else {
        swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    }

    button = swkbdInputText(&swkbd, out_text, out_size);
    return button != SWKBD_BUTTON_NONE;
}

static bool edit_selected_setting(HermesAppConfig* config, SettingsField field, char* status_line, size_t status_line_size)
{
    char edit_buffer[HERMES_APP_CONFIG_TOKEN_MAX];

    if (config == NULL || status_line == NULL || status_line_size == 0)
        return false;

    switch (field) {
        case SETTINGS_FIELD_HOST:
            if (!prompt_text_input(SWKBD_TYPE_WESTERN, "Bridge host or IPv4", config->host, edit_buffer, sizeof(config->host), false)) {
                snprintf(status_line, status_line_size, "%s edit canceled.", settings_field_label(field));
                return false;
            }

            copy_bounded_string(config->host, sizeof(config->host), edit_buffer);
            snprintf(status_line, status_line_size, "Host updated.");
            return true;

        case SETTINGS_FIELD_PORT: {
            char port_text[8];
            char* end_ptr = NULL;
            unsigned long parsed_port;

            snprintf(port_text, sizeof(port_text), "%u", (unsigned int)config->port);
            if (!prompt_text_input(SWKBD_TYPE_NUMPAD, "Bridge port", port_text, edit_buffer, sizeof(port_text), false)) {
                snprintf(status_line, status_line_size, "%s edit canceled.", settings_field_label(field));
                return false;
            }

            parsed_port = strtoul(edit_buffer, &end_ptr, 10);
            if (end_ptr == edit_buffer || *end_ptr != '\0' || parsed_port == 0 || parsed_port > 65535) {
                snprintf(status_line, status_line_size, "Port must be between 1 and 65535.");
                return false;
            }

            config->port = (u16)parsed_port;
            snprintf(status_line, status_line_size, "Port updated.");
            return true;
        }

        case SETTINGS_FIELD_TOKEN:
            if (!prompt_text_input(SWKBD_TYPE_WESTERN, "Bridge token (optional for now)", config->token, edit_buffer, sizeof(config->token), true)) {
                snprintf(status_line, status_line_size, "%s edit canceled.", settings_field_label(field));
                return false;
            }

            copy_bounded_string(config->token, sizeof(config->token), edit_buffer);
            snprintf(status_line, status_line_size, "Token updated.");
            return true;

        default:
            snprintf(status_line, status_line_size, "Unknown settings field.");
            return false;
    }
}

static bool prompt_message_input(char* out_message, size_t out_size)
{
    return prompt_text_input(
        SWKBD_TYPE_WESTERN,
        "Write a message for Hermes",
        "",
        out_message,
        out_size,
        false
    );
}

static void render_home_top_screen(
    const BridgeHealthResult* health_result,
    const BridgeChatResult* chat_result,
    const char* last_message,
    size_t reply_page,
    const char* status_line,
    Result last_rc
)
{
    char message_lines[16][HOME_WRAP_WIDTH + 1];
    char reply_lines[HOME_WRAP_MAX_LINES][HOME_WRAP_WIDTH + 1];
    size_t message_line_count = 0;
    size_t reply_line_count = 0;

    consoleSelect(&top_console);
    consoleClear();

    printf("Hermes Agent 3DS\n");
    printf("=================\n");
    printf("%s\n", status_line);
    printf("last rc: 0x%08lX\n", (unsigned long)last_rc);

    if (chat_result != NULL && (chat_result->success || chat_result->error[0] != '\0')) {
        printf("socket errno: %d\n", chat_result->socket_errno);
        printf("stage: %s\n", chat_result->socket_stage[0] != '\0' ? chat_result->socket_stage : "n/a");
    } else {
        printf("socket errno: %d\n", health_result->socket_errno);
        printf("stage: %s\n", health_result->socket_stage[0] != '\0' ? health_result->socket_stage : "n/a");
    }
    printf("\n");

    if (chat_result != NULL && chat_result->success) {
        message_line_count = wrap_text_for_console(last_message, message_lines, 16);
        reply_line_count = wrap_text_for_console(chat_result->reply, reply_lines, HOME_WRAP_MAX_LINES);
        render_wrapped_page("Last message:", message_lines, message_line_count, 0, HOME_MESSAGE_LINES_PER_PAGE, false);
        printf("\n");
        render_wrapped_page("Last reply:", reply_lines, reply_line_count, reply_page, HOME_REPLY_LINES_PER_PAGE, true);
        if (chat_result->truncated)
            printf("(reply truncated)\n");
    } else if (chat_result != NULL && chat_result->error[0] != '\0') {
        message_line_count = wrap_text_for_console(last_message, message_lines, 16);
        reply_line_count = wrap_text_for_console(chat_result->error, reply_lines, HOME_WRAP_MAX_LINES);
        render_wrapped_page("Last message:", message_lines, message_line_count, 0, HOME_MESSAGE_LINES_PER_PAGE, false);
        printf("\n");
        render_wrapped_page("Chat failed:", reply_lines, reply_line_count, 0, HOME_REPLY_LINES_PER_PAGE, false);
        if (chat_result->http_status != 0)
            printf("http: %lu\n", (unsigned long)chat_result->http_status);
    } else if (health_result->success) {
        printf("Bridge OK\n");
        printf("service: %s\n", health_result->service);
        printf("version: %s\n", health_result->version);
        printf("http: %lu\n", (unsigned long)health_result->http_status);
    } else if (health_result->error[0] != '\0') {
        printf("Bridge check failed\n");
        printf("%s\n", health_result->error);
        if (health_result->http_status != 0)
            printf("http: %lu\n", (unsigned long)health_result->http_status);
    } else {
        printf("Press A to check bridge health.\n");
        printf("Press B to Ask Hermes.\n");
    }
}

static void render_home_bottom_screen(
    const HermesAppConfig* config,
    const BridgeChatResult* chat_result
)
{
    char bridge_summary[80];
    char token_summary[48];
    size_t page_count = 1;

    consoleSelect(&bottom_console);
    consoleClear();

    format_token_summary(config, token_summary, sizeof(token_summary));
    snprintf(bridge_summary, sizeof(bridge_summary), "%s:%u", config->host, (unsigned int)config->port);

    if (chat_result != NULL && chat_result->success)
        page_count = wrapped_page_count(chat_result->reply, HOME_REPLY_LINES_PER_PAGE);

    printf("Bridge:\n%s\n", bridge_summary);
    printf("Token: %s\n", token_summary);
    printf("\n");
    printf("A check   B ask   X settings\n");
    printf("Y clear   START exit\n");
    if (chat_result != NULL && chat_result->success && page_count > 1)
        printf("L/R page reply\n");
    else
        printf("L/R when reply is long\n");
}

static void render_settings_top_screen(const HermesAppConfig* config, SettingsField selected_field, bool settings_dirty, const char* status_line, Result last_rc)
{
    char token_summary[48];
    const char* host_cursor = selected_field == SETTINGS_FIELD_HOST ? ">" : " ";
    const char* port_cursor = selected_field == SETTINGS_FIELD_PORT ? ">" : " ";
    const char* token_cursor = selected_field == SETTINGS_FIELD_TOKEN ? ">" : " ";

    consoleSelect(&top_console);
    consoleClear();

    format_token_summary(config, token_summary, sizeof(token_summary));

    printf("Hermes Agent 3DS\n");
    printf("Settings\n");
    printf("========\n");
    printf("Dirty: %s\n", settings_dirty ? "yes" : "no");
    printf("Status: %s\n", status_line);
    printf("rc: 0x%08lX\n", (unsigned long)last_rc);
    printf("\n");
    printf("%s Host\n", host_cursor);
    printf("  %s\n", config->host);
    printf("%s Port\n", port_cursor);
    printf("  %u\n", (unsigned int)config->port);
    printf("%s Token\n", token_cursor);
    printf("  %s\n", token_summary);
}

static void render_settings_bottom_screen(void)
{
    consoleSelect(&bottom_console);
    consoleClear();

    printf("Settings controls\n");
    printf("================\n");
    printf("UP/DOWN: select field\n");
    printf("A: edit field\n");
    printf("X: save settings\n");
    printf("Y: restore defaults\n");
    printf("B: back home\n");
    printf("START: exit\n");
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
    if (screen == APP_SCREEN_SETTINGS) {
        render_settings_top_screen(config, selected_field, settings_dirty, status_line, last_rc);
        render_settings_bottom_screen();
    } else {
        render_home_top_screen(health_result, chat_result, last_message, reply_page, status_line, last_rc);
        render_home_bottom_screen(config, chat_result);
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
    last_message[0] = '\0';

    load_status = hermes_app_config_load(&config);
    if (load_status == HERMES_APP_CONFIG_LOAD_OK)
        snprintf(status_line, sizeof(status_line), "Loaded saved config from SD card.");
    else if (load_status == HERMES_APP_CONFIG_LOAD_DEFAULTED)
        snprintf(status_line, sizeof(status_line), "No saved config found. Using defaults.");
    else
        snprintf(status_line, sizeof(status_line), "Config load failed. Using defaults.");

    // Old 3DS-friendly UI: text-first rendering split across top and bottom screens.
    gfxInitDefault();
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

            if ((kDown & KEY_L) != 0 && chat_result.success && reply_page > 0) {
                reply_page--;
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_R) != 0 && chat_result.success) {
                size_t page_count = wrapped_page_count(chat_result.reply, HOME_REPLY_LINES_PER_PAGE);
                if (reply_page + 1 < page_count)
                    reply_page++;
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_A) != 0) {
                char health_url[HERMES_APP_HEALTH_URL_MAX];

                bridge_health_result_reset(&health_result);
                request_rc = 0;
                reply_page = 0;

                if (!hermes_app_config_build_health_url(&config, health_url, sizeof(health_url))) {
                    snprintf(status_line, sizeof(status_line), "Local config is incomplete.");
                    render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                    continue;
                }

                snprintf(status_line, sizeof(status_line), "Checking bridge...");
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                gfxFlushBuffers();
                gfxSwapBuffers();
                gspWaitForVBlank();

                if (network_ready) {
                    request_rc = bridge_health_check_run(health_url, &health_result);
                    if (R_SUCCEEDED(request_rc) && health_result.success)
                        snprintf(status_line, sizeof(status_line), "Bridge answered successfully.");
                    else if (health_result.error[0] == '\0')
                        snprintf(status_line, sizeof(status_line), "Bridge check failed: 0x%08lX", (unsigned long)request_rc);
                    else
                        snprintf(status_line, sizeof(status_line), "%s", health_result.error);
                } else {
                    snprintf(status_line, sizeof(status_line), "Networking services failed to start.");
                }

                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
            }

            if ((kDown & KEY_B) != 0) {
                char message_buffer[BRIDGE_CHAT_MESSAGE_MAX];
                char chat_url[HERMES_APP_CHAT_URL_MAX];

                message_buffer[0] = '\0';
                if (!prompt_message_input(message_buffer, sizeof(message_buffer))) {
                    snprintf(status_line, sizeof(status_line), "Ask Hermes canceled.");
                    render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                    continue;
                }

                copy_bounded_string(last_message, sizeof(last_message), message_buffer);
                bridge_chat_result_reset(&chat_result);
                request_rc = 0;
                reply_page = 0;

                if (!hermes_app_config_build_chat_url(&config, chat_url, sizeof(chat_url))) {
                    snprintf(status_line, sizeof(status_line), "Local config is incomplete.");
                    render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                    continue;
                }

                snprintf(status_line, sizeof(status_line), "Asking Hermes...");
                render_ui(screen, &config, selected_field, settings_dirty, &health_result, &chat_result, last_message, reply_page, status_line, request_rc);
                gfxFlushBuffers();
                gfxSwapBuffers();
                gspWaitForVBlank();

                if (network_ready) {
                    request_rc = bridge_chat_run(chat_url, config.token, message_buffer, &chat_result);
                    if (R_SUCCEEDED(request_rc) && chat_result.success)
                        snprintf(status_line, sizeof(status_line), chat_result.truncated ? "Reply received (trimmed)." : "Reply received.");
                    else if (chat_result.error[0] == '\0')
                        snprintf(status_line, sizeof(status_line), "Chat failed: 0x%08lX", (unsigned long)request_rc);
                    else
                        snprintf(status_line, sizeof(status_line), "%s", chat_result.error);
                } else {
                    snprintf(status_line, sizeof(status_line), "Networking services failed to start.");
                }

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
