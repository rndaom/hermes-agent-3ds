#include "app_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static char* lstrip(char* text)
{
    while (*text == ' ' || *text == '\t')
        text++;
    return text;
}

static void rstrip(char* text)
{
    size_t length = strlen(text);
    while (length > 0) {
        char ch = text[length - 1];
        if (ch != '\r' && ch != '\n' && ch != ' ' && ch != '\t')
            break;
        text[length - 1] = '\0';
        length--;
    }
}

static void copy_string(char* dest, size_t dest_size, const char* src)
{
    if (dest == NULL || dest_size == 0)
        return;

    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    snprintf(dest, dest_size, "%s", src);
}

static bool url_encode_component(const char* input, char* out, size_t out_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t write_index = 0;

    if (out == NULL || out_size == 0)
        return false;

    out[0] = '\0';
    if (input == NULL)
        return true;

    while (*input != '\0') {
        unsigned char ch = (unsigned char)*input;
        bool safe = (ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    ch == '-' || ch == '_' || ch == '.' || ch == '~';

        if (safe) {
            if (write_index + 1 >= out_size)
                return false;
            out[write_index++] = (char)ch;
        } else {
            if (write_index + 3 >= out_size)
                return false;
            out[write_index++] = '%';
            out[write_index++] = hex[(ch >> 4) & 0x0F];
            out[write_index++] = hex[ch & 0x0F];
        }

        input++;
    }

    out[write_index] = '\0';
    return true;
}

static bool is_safe_conversation_id(const char* value)
{
    if (value == NULL || value[0] == '\0')
        return false;

    while (*value != '\0') {
        if ((*value >= 'a' && *value <= 'z') ||
            (*value >= 'A' && *value <= 'Z') ||
            (*value >= '0' && *value <= '9') ||
            *value == '-' || *value == '_' || *value == '.') {
            value++;
            continue;
        }
        return false;
    }

    return true;
}

static void clear_recent_conversations(HermesAppConfig* config)
{
    size_t index;

    if (config == NULL)
        return;

    config->recent_conversation_count = 0;
    for (index = 0; index < HERMES_APP_RECENT_CONVERSATIONS_MAX; index++)
        config->recent_conversations[index][0] = '\0';
}

static void ensure_conversation_defaults(HermesAppConfig* config)
{
    if (config == NULL)
        return;

    if (!is_safe_conversation_id(config->active_conversation_id))
        copy_string(config->active_conversation_id, sizeof(config->active_conversation_id), DEFAULT_CONVERSATION_ID);

    if (config->recent_conversation_count == 0 || !is_safe_conversation_id(config->recent_conversations[0])) {
        clear_recent_conversations(config);
        copy_string(config->recent_conversations[0], sizeof(config->recent_conversations[0]), DEFAULT_CONVERSATION_ID);
        config->recent_conversation_count = 1;
    }
}

bool hermes_app_config_is_valid_conversation_id(const char* conversation_id)
{
    return is_safe_conversation_id(conversation_id);
}

void hermes_app_config_touch_recent_conversation(HermesAppConfig* config, const char* conversation_id)
{
    size_t existing_index = HERMES_APP_RECENT_CONVERSATIONS_MAX;
    size_t write_index;

    if (config == NULL || !is_safe_conversation_id(conversation_id))
        return;

    for (write_index = 0; write_index < config->recent_conversation_count; write_index++) {
        if (strcmp(config->recent_conversations[write_index], conversation_id) == 0) {
            existing_index = write_index;
            break;
        }
    }

    if (existing_index == HERMES_APP_RECENT_CONVERSATIONS_MAX) {
        if (config->recent_conversation_count < HERMES_APP_RECENT_CONVERSATIONS_MAX)
            config->recent_conversation_count++;
        existing_index = config->recent_conversation_count - 1;
    }

    while (existing_index > 0) {
        copy_string(
            config->recent_conversations[existing_index],
            sizeof(config->recent_conversations[existing_index]),
            config->recent_conversations[existing_index - 1]
        );
        existing_index--;
    }

    copy_string(config->recent_conversations[0], sizeof(config->recent_conversations[0]), conversation_id);

    for (write_index = config->recent_conversation_count; write_index < HERMES_APP_RECENT_CONVERSATIONS_MAX; write_index++)
        config->recent_conversations[write_index][0] = '\0';
}

bool hermes_app_config_set_active_conversation(HermesAppConfig* config, const char* conversation_id)
{
    if (config == NULL || !is_safe_conversation_id(conversation_id))
        return false;

    copy_string(config->active_conversation_id, sizeof(config->active_conversation_id), conversation_id);
    hermes_app_config_touch_recent_conversation(config, conversation_id);
    return true;
}

static void parse_recent_conversations(HermesAppConfig* config, const char* recent_text)
{
    char buffer[(HERMES_APP_CONVERSATION_ID_MAX + 1) * HERMES_APP_RECENT_CONVERSATIONS_MAX];
    char* token;
    char* rest;

    if (config == NULL || recent_text == NULL || recent_text[0] == '\0')
        return;

    clear_recent_conversations(config);
    copy_string(buffer, sizeof(buffer), recent_text);
    rest = buffer;
    while ((token = strtok(rest, ",")) != NULL) {
        bool duplicate = false;
        size_t index;

        rest = NULL;
        token = lstrip(token);
        rstrip(token);
        if (!is_safe_conversation_id(token))
            continue;

        for (index = 0; index < config->recent_conversation_count; index++) {
            if (strcmp(config->recent_conversations[index], token) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;
        if (config->recent_conversation_count >= HERMES_APP_RECENT_CONVERSATIONS_MAX)
            break;

        copy_string(
            config->recent_conversations[config->recent_conversation_count],
            sizeof(config->recent_conversations[config->recent_conversation_count]),
            token
        );
        config->recent_conversation_count++;
    }
}

void hermes_app_config_set_defaults(HermesAppConfig* config)
{
    if (config == NULL)
        return;

    memset(config, 0, sizeof(*config));
    copy_string(config->host, sizeof(config->host), DEFAULT_GATEWAY_HOST);
    config->port = DEFAULT_GATEWAY_PORT;
    config->token[0] = '\0';
    config->device_id[0] = '\0';
    copy_string(config->active_conversation_id, sizeof(config->active_conversation_id), DEFAULT_CONVERSATION_ID);
    clear_recent_conversations(config);
    copy_string(config->recent_conversations[0], sizeof(config->recent_conversations[0]), DEFAULT_CONVERSATION_ID);
    config->recent_conversation_count = 1;
    config->theme_color = PICTOCHAT_THEME_DEFAULT;
    config->dark_mode = true;
}

HermesAppConfigLoadStatus hermes_app_config_load(HermesAppConfig* config)
{
    FILE* file;
    char line[256];

    if (config == NULL)
        return HERMES_APP_CONFIG_LOAD_ERROR;

    hermes_app_config_set_defaults(config);

    file = fopen(HERMES_APP_CONFIG_PATH, "r");
    if (file == NULL)
        return HERMES_APP_CONFIG_LOAD_DEFAULTED;

    while (fgets(line, sizeof(line), file) != NULL) {
        char* value;
        rstrip(line);
        value = lstrip(line);

        if (*value == '\0' || *value == '#' || *value == ';')
            continue;

        if (strncmp(value, "host=", 5) == 0) {
            char* host = lstrip(value + 5);
            if (*host != '\0')
                copy_string(config->host, sizeof(config->host), host);
        } else if (strncmp(value, "port=", 5) == 0) {
            char* port_text = lstrip(value + 5);
            char* end_ptr = NULL;
            long port_value = strtol(port_text, &end_ptr, 10);
            if (end_ptr != port_text && *lstrip(end_ptr) == '\0' && port_value > 0 && port_value <= 65535)
                config->port = (u16)port_value;
        } else if (strncmp(value, "token=", 6) == 0) {
            copy_string(config->token, sizeof(config->token), value + 6);
        } else if (strncmp(value, "device_id=", 10) == 0) {
            char* device_id = lstrip(value + 10);
            if (*device_id != '\0')
                copy_string(config->device_id, sizeof(config->device_id), device_id);
        } else if (strncmp(value, "active_conversation_id=", 23) == 0) {
            char* conversation_id = lstrip(value + 23);
            if (*conversation_id != '\0' && is_safe_conversation_id(conversation_id))
                copy_string(config->active_conversation_id, sizeof(config->active_conversation_id), conversation_id);
        } else if (strncmp(value, "recent_conversations=", 21) == 0) {
            parse_recent_conversations(config, lstrip(value + 21));
        } else if (strncmp(value, "theme_color=", 12) == 0) {
            char* theme_text = lstrip(value + 12);
            char* end_ptr = NULL;
            long theme_value = strtol(theme_text, &end_ptr, 10);
            if (end_ptr != theme_text && *lstrip(end_ptr) == '\0' && theme_value >= 0 && theme_value < PICTOCHAT_THEME_COUNT)
                config->theme_color = (PictochatThemeColor)theme_value;
        } else if (strncmp(value, "dark_mode=", 10) == 0) {
            char* dark_text = lstrip(value + 10);
            if (strcmp(dark_text, "1") == 0 || strcmp(dark_text, "true") == 0)
                config->dark_mode = true;
            else
                config->dark_mode = false;
        }
    }

    fclose(file);

    if (config->host[0] == '\0')
        copy_string(config->host, sizeof(config->host), DEFAULT_GATEWAY_HOST);
    if (config->port == 0)
        config->port = DEFAULT_GATEWAY_PORT;

    ensure_conversation_defaults(config);
    hermes_app_config_touch_recent_conversation(config, config->active_conversation_id);

    return HERMES_APP_CONFIG_LOAD_OK;
}

bool hermes_app_config_save(const HermesAppConfig* config)
{
    FILE* file;
    size_t index;

    if (config == NULL)
        return false;

    if (mkdir(HERMES_APP_CONFIG_DIR, 0777) != 0 && errno != EEXIST)
        return false;

    file = fopen(HERMES_APP_CONFIG_PATH, "w");
    if (file == NULL)
        return false;

    if (fprintf(file, "host=%s\n", config->host) < 0 ||
        fprintf(file, "port=%u\n", (unsigned int)config->port) < 0 ||
        fprintf(file, "token=%s\n", config->token) < 0 ||
        fprintf(file, "device_id=%s\n", config->device_id) < 0 ||
        fprintf(file, "active_conversation_id=%s\n", config->active_conversation_id) < 0 ||
        fprintf(file, "theme_color=%d\n", (int)config->theme_color) < 0 ||
        fprintf(file, "dark_mode=%d\n", config->dark_mode ? 1 : 0) < 0 ||
        fprintf(file, "recent_conversations=") < 0) {
        fclose(file);
        return false;
    }

    for (index = 0; index < config->recent_conversation_count; index++) {
        if (index > 0 && fprintf(file, ",") < 0) {
            fclose(file);
            return false;
        }
        if (fprintf(file, "%s", config->recent_conversations[index]) < 0) {
            fclose(file);
            return false;
        }
    }

    if (fprintf(file, "\n") < 0) {
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

static bool build_url(const HermesAppConfig* config, const char* endpoint_path, char* out_url, size_t out_size)
{
    int written;

    if (config == NULL || endpoint_path == NULL || out_url == NULL || out_size == 0)
        return false;

    if (config->host[0] == '\0' || config->port == 0)
        return false;

    written = snprintf(out_url, out_size, "http://%s:%u%s", config->host, (unsigned int)config->port, endpoint_path);
    return written > 0 && (size_t)written < out_size;
}

bool hermes_app_config_build_health_url(const HermesAppConfig* config, char* out_url, size_t out_size)
{
    int written;
    char encoded_device_id[HERMES_APP_DEVICE_ID_MAX * 3 + 1];
    char encoded_conversation_id[HERMES_APP_CONVERSATION_ID_MAX * 3 + 1];
    char encoded_token[HERMES_APP_CONFIG_TOKEN_MAX * 3 + 1];

    if (config == NULL || out_url == NULL || out_size == 0)
        return false;

    if (config->host[0] == '\0' || config->port == 0)
        return false;

    if (!url_encode_component(config->device_id[0] != '\0' ? config->device_id : "", encoded_device_id, sizeof(encoded_device_id)) ||
        !url_encode_component(config->active_conversation_id[0] != '\0' ? config->active_conversation_id : DEFAULT_CONVERSATION_ID,
            encoded_conversation_id,
            sizeof(encoded_conversation_id)) ||
        !url_encode_component(config->token[0] != '\0' ? config->token : "", encoded_token, sizeof(encoded_token))) {
        return false;
    }

    written = snprintf(
        out_url,
        out_size,
        "http://%s:%u/api/v2/health?token=%s&device_id=%s&conversation_id=%s",
        config->host,
        (unsigned int)config->port,
        encoded_token,
        encoded_device_id,
        encoded_conversation_id
    );
    return written > 0 && (size_t)written < out_size;
}

bool hermes_app_config_build_conversations_url(const HermesAppConfig* config, char* out_url, size_t out_size)
{
    return build_url(config, "/api/v2/conversations", out_url, out_size);
}

bool hermes_app_config_build_messages_url(const HermesAppConfig* config, char* out_url, size_t out_size)
{
    return build_url(config, "/api/v2/messages", out_url, out_size);
}

bool hermes_app_config_build_events_url(const HermesAppConfig* config, char* out_url, size_t out_size)
{
    return build_url(config, "/api/v2/events", out_url, out_size);
}

bool hermes_app_config_build_voice_url(const HermesAppConfig* config, char* out_url, size_t out_size)
{
    return build_url(config, "/api/v2/voice", out_url, out_size);
}

bool hermes_app_config_build_interaction_url(const HermesAppConfig* config, const char* request_id, char* out_url, size_t out_size)
{
    int written;

    if (config == NULL || request_id == NULL || out_url == NULL || out_size == 0)
        return false;

    if (config->host[0] == '\0' || config->port == 0 || request_id[0] == '\0')
        return false;

    written = snprintf(
        out_url,
        out_size,
        "http://%s:%u%s%s/respond",
        config->host,
        (unsigned int)config->port,
        "/api/v2/interactions/",
        request_id
    );
    return written > 0 && (size_t)written < out_size;
}
