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

void hermes_app_config_set_defaults(HermesAppConfig* config)
{
    if (config == NULL)
        return;

    memset(config, 0, sizeof(*config));
    copy_string(config->host, sizeof(config->host), DEFAULT_BRIDGE_HOST);
    config->port = DEFAULT_BRIDGE_PORT;
    config->token[0] = '\0';
    config->device_id[0] = '\0';
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
        }
    }

    fclose(file);

    if (config->host[0] == '\0')
        copy_string(config->host, sizeof(config->host), DEFAULT_BRIDGE_HOST);
    if (config->port == 0)
        config->port = DEFAULT_BRIDGE_PORT;

    return HERMES_APP_CONFIG_LOAD_OK;
}

bool hermes_app_config_save(const HermesAppConfig* config)
{
    FILE* file;

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
        fprintf(file, "device_id=%s\n", config->device_id) < 0) {
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
    return build_url(config, "/api/v1/health", out_url, out_size);
}

bool hermes_app_config_build_chat_url(const HermesAppConfig* config, char* out_url, size_t out_size)
{
    return build_url(config, "/api/v1/chat", out_url, out_size);
}

bool hermes_app_config_build_capabilities_url(const HermesAppConfig* config, char* out_url, size_t out_size)
{
    return build_url(config, "/api/v2/capabilities", out_url, out_size);
}

bool hermes_app_config_build_messages_url(const HermesAppConfig* config, char* out_url, size_t out_size)
{
    return build_url(config, "/api/v2/messages", out_url, out_size);
}

bool hermes_app_config_build_events_url(const HermesAppConfig* config, char* out_url, size_t out_size)
{
    return build_url(config, "/api/v2/events", out_url, out_size);
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
