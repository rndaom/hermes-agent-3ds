#include "bridge_chat.h"

#include <3ds.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define BRIDGE_CHAT_CONNECT_TIMEOUT_SECONDS 5
#define BRIDGE_CHAT_IO_TIMEOUT_SECONDS 8
#define BRIDGE_CHAT_RESPONSE_MAX 4096

static void set_error(BridgeChatResult* result, const char* message)
{
    if (result == NULL)
        return;

    result->success = false;
    snprintf(result->error, sizeof(result->error), "%s", message != NULL ? message : "Unknown chat error.");
}

static void set_socket_debug(BridgeChatResult* result, const char* stage, int socket_errno)
{
    if (result == NULL)
        return;

    result->socket_errno = socket_errno;
    snprintf(result->socket_stage, sizeof(result->socket_stage), "%s", stage != NULL ? stage : "unknown");
}

static bool extract_json_string(const char* json, const char* key, char* out, size_t out_size)
{
    char pattern[64];
    const char* cursor;
    size_t out_used = 0;

    if (json == NULL || key == NULL || out == NULL || out_size == 0)
        return false;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    cursor = strstr(json, pattern);
    if (cursor == NULL)
        return false;

    cursor += strlen(pattern);
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
        cursor++;
    if (*cursor != ':')
        return false;

    cursor++;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
        cursor++;
    if (*cursor != '"')
        return false;

    cursor++;
    while (*cursor != '\0') {
        char ch = *cursor++;
        if (ch == '"')
            break;

        if (ch == '\\') {
            char escaped = *cursor++;
            switch (escaped) {
                case '"':
                    ch = '"';
                    break;
                case '\\':
                    ch = '\\';
                    break;
                case '/':
                    ch = '/';
                    break;
                case 'n':
                    ch = '\n';
                    break;
                case 'r':
                    ch = '\r';
                    break;
                case 't':
                    ch = '\t';
                    break;
                case 'b':
                    ch = '\b';
                    break;
                case 'f':
                    ch = '\f';
                    break;
                case '\0':
                    return false;
                default:
                    ch = escaped;
                    break;
            }
        }

        if (out_used + 1 >= out_size)
            break;
        out[out_used++] = ch;
    }

    out[out_used] = '\0';
    return true;
}

static bool parse_chat_url(const char* url, char* host, size_t host_size, u16* port, char* path, size_t path_size)
{
    const char* cursor;
    const char* slash;
    const char* colon;
    const char* host_end;
    size_t host_length;

    if (url == NULL || host == NULL || port == NULL || path == NULL)
        return false;

    if (strncmp(url, "http://", 7) != 0)
        return false;

    cursor = url + 7;
    slash = strchr(cursor, '/');
    if (slash == NULL)
        slash = cursor + strlen(cursor);

    colon = memchr(cursor, ':', (size_t)(slash - cursor));
    host_end = colon != NULL ? colon : slash;
    host_length = (size_t)(host_end - cursor);
    if (host_length == 0 || host_length >= host_size)
        return false;

    memcpy(host, cursor, host_length);
    host[host_length] = '\0';

    if (colon != NULL) {
        long parsed_port = strtol(colon + 1, NULL, 10);
        if (parsed_port <= 0 || parsed_port > 65535)
            return false;
        *port = (u16)parsed_port;
    } else {
        *port = 80;
    }

    if (*slash == '\0')
        snprintf(path, path_size, "/");
    else
        snprintf(path, path_size, "%s", slash);

    return true;
}

static bool resolve_ipv4_address(const char* host, struct in_addr* out_addr)
{
    struct hostent* resolved;

    if (inet_aton(host, out_addr) != 0)
        return true;

    resolved = gethostbyname(host);
    if (resolved == NULL || resolved->h_addrtype != AF_INET || resolved->h_length != sizeof(struct in_addr))
        return false;

    memcpy(out_addr, resolved->h_addr_list[0], sizeof(struct in_addr));
    return true;
}

static Result wait_for_socket(int socket_fd, bool want_write, int timeout_seconds)
{
    fd_set read_fds;
    fd_set write_fds;
    struct timeval timeout;
    int selected;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    if (want_write)
        FD_SET(socket_fd, &write_fds);
    else
        FD_SET(socket_fd, &read_fds);

    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    selected = select(socket_fd + 1, want_write ? NULL : &read_fds, want_write ? &write_fds : NULL, NULL, &timeout);
    if (selected == 0)
        return MAKERESULT(RL_STATUS, RS_INTERNAL, RM_APPLICATION, RD_TIMEOUT);
    if (selected < 0)
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);

    return 0;
}

static Result connect_with_timeout(int socket_fd, const struct sockaddr_in* address, BridgeChatResult* result)
{
    int flags;
    int connect_rc;
    Result wait_rc;

    flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0)
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);

    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);

    connect_rc = connect(socket_fd, (const struct sockaddr*)address, sizeof(*address));
    if (connect_rc == 0) {
        set_socket_debug(result, "connect", 0);
        fcntl(socket_fd, F_SETFL, flags);
        return 0;
    }

    if (errno != EINPROGRESS) {
        set_socket_debug(result, "connect", errno);
        fcntl(socket_fd, F_SETFL, flags);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    wait_rc = wait_for_socket(socket_fd, true, BRIDGE_CHAT_CONNECT_TIMEOUT_SECONDS);
    if (R_FAILED(wait_rc)) {
        set_socket_debug(result, "connect-wait", errno);
        fcntl(socket_fd, F_SETFL, flags);
        return wait_rc;
    }

    set_socket_debug(result, "connect-ready", 0);

    if (fcntl(socket_fd, F_SETFL, flags) < 0)
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);

    return 0;
}

static Result send_all(int socket_fd, const char* buffer, size_t length)
{
    size_t sent_total = 0;

    while (sent_total < length) {
        ssize_t sent = send(socket_fd, buffer + sent_total, length - sent_total, 0);
        if (sent < 0) {
            if (errno == EINTR)
                continue;
            return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
        }

        sent_total += (size_t)sent;
    }

    return 0;
}

static bool escape_json_string(const char* input, char* out, size_t out_size)
{
    size_t out_used = 0;

    if (input == NULL || out == NULL || out_size == 0)
        return false;

    while (*input != '\0') {
        const char* replacement = NULL;
        char scratch[3] = {0, 0, 0};
        size_t replacement_length;

        switch (*input) {
            case '\\':
                replacement = "\\\\";
                break;
            case '"':
                replacement = "\\\"";
                break;
            case '\n':
                replacement = "\\n";
                break;
            case '\r':
                replacement = "\\r";
                break;
            case '\t':
                replacement = "\\t";
                break;
            default:
                scratch[0] = *input;
                replacement = scratch;
                break;
        }

        replacement_length = strlen(replacement);
        if (out_used + replacement_length >= out_size)
            return false;

        memcpy(out + out_used, replacement, replacement_length);
        out_used += replacement_length;
        input++;
    }

    out[out_used] = '\0';
    return true;
}

void bridge_chat_result_reset(BridgeChatResult* result)
{
    if (result == NULL)
        return;

    memset(result, 0, sizeof(*result));
}

Result bridge_chat_run(const char* url, const char* token, const char* message, BridgeChatResult* result)
{
    Result wifi_rc;
    Result rc;
    u32 wifi_status = 0;
    char host[64];
    char path[128];
    u16 port = 0;
    struct sockaddr_in address;
    int socket_fd = -1;
    char escaped_token[(128 * 2) + 16];
    char escaped_message[(BRIDGE_CHAT_MESSAGE_MAX * 2) + 32];
    char body[1024];
    char request[2048];
    char response[BRIDGE_CHAT_RESPONSE_MAX];
    size_t response_used = 0;
    const char* response_body;
    unsigned int status_code = 0;
    int body_length;

    if (result == NULL)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    bridge_chat_result_reset(result);

    if (url == NULL || message == NULL || message[0] == '\0') {
        set_error(result, "Message is required.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    wifi_rc = ACU_GetWifiStatus(&wifi_status);
    if (R_FAILED(wifi_rc)) {
        set_error(result, "Could not read Wi-Fi status.");
        return wifi_rc;
    }

    if (wifi_status == 0) {
        set_error(result, "Wi-Fi is not connected.");
        return MAKERESULT(RL_STATUS, RS_NOTFOUND, RM_APPLICATION, RD_NOT_FOUND);
    }

    if (!parse_chat_url(url, host, sizeof(host), &port, path, sizeof(path))) {
        set_error(result, "Bridge URL is invalid.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    if (!escape_json_string(token != NULL ? token : "", escaped_token, sizeof(escaped_token)) ||
        !escape_json_string(message, escaped_message, sizeof(escaped_message))) {
        set_error(result, "Message was too long to send.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    body_length = snprintf(
        body,
        sizeof(body),
        "{\"token\":\"%s\",\"message\":\"%s\",\"context\":[],\"client\":{\"platform\":\"3ds\",\"app_version\":\"0.1.0\"}}",
        escaped_token,
        escaped_message
    );
    if (body_length <= 0 || (size_t)body_length >= sizeof(body)) {
        set_error(result, "Message was too long to send.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (!resolve_ipv4_address(host, &address.sin_addr)) {
        set_socket_debug(result, "resolve", h_errno);
        set_error(result, "Could not resolve bridge host.");
        return MAKERESULT(RL_STATUS, RS_NOTFOUND, RM_APPLICATION, RD_NOT_FOUND);
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (socket_fd < 0) {
        set_socket_debug(result, "socket", errno);
        set_error(result, "Could not open a network socket.");
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    rc = connect_with_timeout(socket_fd, &address, result);
    if (R_FAILED(rc)) {
        set_error(result, rc == MAKERESULT(RL_STATUS, RS_INTERNAL, RM_APPLICATION, RD_TIMEOUT)
                              ? "Bridge timed out."
                              : "Could not reach the bridge.");
        close(socket_fd);
        return rc;
    }

    snprintf(
        request,
        sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "User-Agent: hermes-agent-3ds/0.1.0\r\n"
        "Connection: close\r\n"
        "Accept: application/json\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
        path,
        host,
        (unsigned int)port,
        (unsigned int)body_length,
        body
    );

    rc = send_all(socket_fd, request, strlen(request));
    if (R_FAILED(rc)) {
        set_socket_debug(result, "send", errno);
        set_error(result, "Could not send the chat request.");
        close(socket_fd);
        return rc;
    }

    memset(response, 0, sizeof(response));
    while (response_used < sizeof(response) - 1) {
        Result wait_rc = wait_for_socket(socket_fd, false, BRIDGE_CHAT_IO_TIMEOUT_SECONDS);
        ssize_t received;

        if (R_FAILED(wait_rc)) {
            set_socket_debug(result, "recv-wait", errno);
            set_error(result, "Bridge timed out.");
            close(socket_fd);
            return wait_rc;
        }

        received = recv(socket_fd, response + response_used, sizeof(response) - 1 - response_used, 0);
        if (received < 0) {
            if (errno == EINTR)
                continue;
            set_socket_debug(result, "recv", errno);
            set_error(result, "Bridge response read failed.");
            close(socket_fd);
            return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
        }

        if (received == 0)
            break;

        response_used += (size_t)received;
    }

    close(socket_fd);
    response[response_used] = '\0';

    if (sscanf(response, "HTTP/%*s %u", &status_code) != 1) {
        set_error(result, "Bridge returned an invalid HTTP response.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    result->http_status = status_code;
    response_body = strstr(response, "\r\n\r\n");
    if (response_body == NULL) {
        set_error(result, "Bridge response body was missing.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    response_body += 4;

    if (status_code != 200) {
        if (!extract_json_string(response_body, "error", result->error, sizeof(result->error)))
            snprintf(result->error, sizeof(result->error), "Bridge returned HTTP %u.", status_code);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    if (strstr(response_body, "\"ok\":true") == NULL) {
        if (!extract_json_string(response_body, "error", result->error, sizeof(result->error)))
            set_error(result, "Bridge response was not OK.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    if (!extract_json_string(response_body, "reply", result->reply, sizeof(result->reply))) {
        set_error(result, "Bridge reply was missing.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    result->truncated = strstr(response_body, "\"truncated\":true") != NULL;
    result->success = true;
    result->error[0] = '\0';
    return 0;
}
