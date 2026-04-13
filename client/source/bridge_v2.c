#include "bridge_v2.h"

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

#define BRIDGE_V2_CONNECT_TIMEOUT_SECONDS 5
#define BRIDGE_V2_IO_TIMEOUT_SECONDS 20
#define BRIDGE_V2_RESPONSE_MAX 4096
#define BRIDGE_V2_MESSAGE_BODY_MAX ((BRIDGE_V2_TEXT_MAX * 2) + (BRIDGE_V2_CONVERSATION_ID_MAX * 4) + 256)

static void set_error(char* out, size_t out_size, const char* message)
{
    if (out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "%s", message != NULL ? message : "Unknown v2 bridge error.");
}

static bool response_ok(const char* response_body)
{
    if (response_body == NULL)
        return false;
    return strstr(response_body, "\"ok\":true") != NULL || strstr(response_body, "\"ok\": true") != NULL;
}

static int hex_digit_value(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F')
        return 10 + (ch - 'A');
    return -1;
}

static bool append_text(char* out, size_t out_size, size_t* io_used, const char* text)
{
    size_t length;

    if (out == NULL || io_used == NULL || text == NULL)
        return false;

    length = strlen(text);
    if (*io_used + length >= out_size)
        return false;

    memcpy(out + *io_used, text, length);
    *io_used += length;
    return true;
}

static bool append_codepoint_fallback(char* out, size_t out_size, size_t* io_used, unsigned int codepoint)
{
    switch (codepoint) {
        case 0x2018:
        case 0x2019:
        case 0x201A:
        case 0x201B:
        case 0x2032:
            return append_text(out, out_size, io_used, "'");
        case 0x201C:
        case 0x201D:
        case 0x201E:
        case 0x201F:
        case 0x2033:
            return append_text(out, out_size, io_used, "\"");
        case 0x2010:
        case 0x2011:
        case 0x2012:
        case 0x2013:
        case 0x2014:
        case 0x2015:
        case 0x2212:
            return append_text(out, out_size, io_used, "-");
        case 0x2026:
            return append_text(out, out_size, io_used, "...");
        case 0x00A0:
            return append_text(out, out_size, io_used, " ");
        default:
            if (codepoint >= 0x20 && codepoint <= 0x7E) {
                char ch[2] = {(char)codepoint, '\0'};
                return append_text(out, out_size, io_used, ch);
            }
            return append_text(out, out_size, io_used, "?");
    }
}

static bool extract_json_string_v2(const char* json, const char* key, char* out, size_t out_size)
{
    char pattern[64];
    const char* cursor;
    size_t used = 0;

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
                case 'u': {
                    int digit0 = hex_digit_value(cursor[0]);
                    int digit1 = hex_digit_value(cursor[1]);
                    int digit2 = hex_digit_value(cursor[2]);
                    int digit3 = hex_digit_value(cursor[3]);
                    unsigned int codepoint;

                    if (digit0 < 0 || digit1 < 0 || digit2 < 0 || digit3 < 0)
                        return false;

                    codepoint = (unsigned int)((digit0 << 12) | (digit1 << 8) | (digit2 << 4) | digit3);
                    cursor += 4;
                    if (!append_codepoint_fallback(out, out_size, &used, codepoint)) {
                        out[used] = '\0';
                        return true;
                    }
                    continue;
                }
                case '\0':
                    return false;
                default:
                    ch = escaped;
                    break;
            }
        }

        if (used + 1 >= out_size)
            break;
        out[used++] = ch;
    }

    out[used] = '\0';
    return true;
}

static bool extract_json_string_after(const char* json, const char* marker, const char* key, char* out, size_t out_size)
{
    const char* cursor;

    if (json == NULL || marker == NULL)
        return false;

    cursor = strstr(json, marker);
    if (cursor == NULL)
        return false;

    return extract_json_string_v2(cursor, key, out, out_size);
}

static bool extract_json_u32(const char* json, const char* key, u32* out_value)
{
    char pattern[64];
    const char* cursor;
    char* end_ptr = NULL;
    unsigned long parsed;

    if (json == NULL || key == NULL || out_value == NULL)
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

    parsed = strtoul(cursor, &end_ptr, 10);
    if (end_ptr == cursor)
        return false;

    *out_value = (u32)parsed;
    return true;
}

static bool is_safe_query_value(const char* value)
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

static bool escape_json_string(const char* input, char* out, size_t out_size)
{
    size_t used = 0;

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
        if (used + replacement_length >= out_size)
            return false;

        memcpy(out + used, replacement, replacement_length);
        used += replacement_length;
        input++;
    }

    out[used] = '\0';
    return true;
}

static bool parse_http_url(const char* url, char* host, size_t host_size, u16* port, char* path, size_t path_size)
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

static Result connect_with_timeout(int socket_fd, const struct sockaddr_in* address)
{
    int flags;
    int connect_rc;
    int socket_error = 0;
    socklen_t socket_error_size = sizeof(socket_error);
    struct sockaddr_in peer_address;
    socklen_t peer_address_size = sizeof(peer_address);
    Result wait_rc;

    flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0)
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);

    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);

    connect_rc = connect(socket_fd, (const struct sockaddr*)address, sizeof(*address));
    if (connect_rc == 0) {
        fcntl(socket_fd, F_SETFL, flags);
        return 0;
    }

    if (errno != EINPROGRESS) {
        fcntl(socket_fd, F_SETFL, flags);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    wait_rc = wait_for_socket(socket_fd, true, BRIDGE_V2_CONNECT_TIMEOUT_SECONDS);
    if (R_FAILED(wait_rc)) {
        fcntl(socket_fd, F_SETFL, flags);
        return wait_rc;
    }

    if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_size) != 0) {
        fcntl(socket_fd, F_SETFL, flags);
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (socket_error != 0) {
        memset(&peer_address, 0, sizeof(peer_address));
        if (getpeername(socket_fd, (struct sockaddr*)&peer_address, &peer_address_size) == 0) {
            if (fcntl(socket_fd, F_SETFL, flags) < 0)
                return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
            return 0;
        }
        errno = socket_error;
        fcntl(socket_fd, F_SETFL, flags);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    if (fcntl(socket_fd, F_SETFL, flags) < 0)
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);

    return 0;
}

static Result send_all(int socket_fd, const void* buffer, size_t length)
{
    const u8* bytes = (const u8*)buffer;
    size_t sent_total = 0;

    while (sent_total < length) {
        ssize_t sent = send(socket_fd, bytes + sent_total, length - sent_total, 0);
        if (sent < 0) {
            if (errno == EINTR)
                continue;
            return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
        }
        sent_total += (size_t)sent;
    }

    return 0;
}

static bool parse_content_length_header(const char* response, size_t* out_content_length)
{
    const char* header;
    const char* body;
    char* end_ptr = NULL;
    unsigned long parsed;

    if (response == NULL || out_content_length == NULL)
        return false;

    body = strstr(response, "\r\n\r\n");
    if (body == NULL)
        return false;

    header = strstr(response, "\r\nContent-Length:");
    if (header != NULL)
        header += 2;
    else if (strncmp(response, "Content-Length:", strlen("Content-Length:")) == 0)
        header = response;
    else
        return false;

    if (header >= body)
        return false;

    header = strchr(header, ':');
    if (header == NULL)
        return false;
    header++;

    while (*header == ' ' || *header == '\t')
        header++;

    parsed = strtoul(header, &end_ptr, 10);
    if (end_ptr == header)
        return false;

    *out_content_length = (size_t)parsed;
    return true;
}

static bool response_complete(const char* response, size_t response_used)
{
    const char* body;
    size_t header_bytes;
    size_t content_length;

    if (response == NULL)
        return false;

    body = strstr(response, "\r\n\r\n");
    if (body == NULL)
        return false;

    if (!parse_content_length_header(response, &content_length))
        return false;

    header_bytes = (size_t)((body + 4) - response);
    if (response_used < header_bytes)
        return false;

    return (response_used - header_bytes) >= content_length;
}

static Result perform_request(
    const char* method,
    const char* url,
    const char* token,
    const void* body,
    size_t body_length,
    const char* content_type,
    char* response,
    size_t response_size,
    u32* http_status,
    char* error_out,
    size_t error_out_size
)
{
    Result wifi_rc;
    u32 wifi_status = 0;
    char host[64];
    char path[192];
    u16 port = 0;
    struct sockaddr_in address;
    int socket_fd = -1;
    char request[1024];
    size_t response_used = 0;
    Result rc;
    int request_length;
    bool has_body = body != NULL && body_length > 0;
    const char* auth_header = "";

    if (response == NULL || response_size == 0)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    response[0] = '\0';
    if (http_status != NULL)
        *http_status = 0;

    wifi_rc = ACU_GetWifiStatus(&wifi_status);
    if (R_FAILED(wifi_rc)) {
        set_error(error_out, error_out_size, "Could not read Wi-Fi status.");
        return wifi_rc;
    }
    if (wifi_status == 0) {
        set_error(error_out, error_out_size, "Wi-Fi is not connected.");
        return MAKERESULT(RL_STATUS, RS_NOTFOUND, RM_APPLICATION, RD_NOT_FOUND);
    }

    if (!parse_http_url(url, host, sizeof(host), &port, path, sizeof(path))) {
        set_error(error_out, error_out_size, "Bridge URL is invalid.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (!resolve_ipv4_address(host, &address.sin_addr)) {
        set_error(error_out, error_out_size, "Could not resolve Hermes gateway host.");
        return MAKERESULT(RL_STATUS, RS_NOTFOUND, RM_APPLICATION, RD_NOT_FOUND);
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (socket_fd < 0) {
        set_error(error_out, error_out_size, "Could not open a network socket.");
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    rc = connect_with_timeout(socket_fd, &address);
    if (R_FAILED(rc)) {
        set_error(error_out, error_out_size, rc == MAKERESULT(RL_STATUS, RS_INTERNAL, RM_APPLICATION, RD_TIMEOUT)
            ? "Hermes gateway timed out."
            : "Could not reach the Hermes gateway.");
        close(socket_fd);
        return rc;
    }

    if (token != NULL && token[0] != '\0')
        auth_header = token;

    if (has_body) {
        request_length = snprintf(
            request,
            sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%u\r\n"
            "User-Agent: hermes-agent-3ds/0.2.0\r\n"
            "Connection: close\r\n"
            "Accept: application/json\r\n"
            "%s%s%s"
            "Content-Type: %s\r\n"
            "Content-Length: %u\r\n"
            "\r\n",
            method,
            path,
            host,
            (unsigned int)port,
            auth_header[0] != '\0' ? "Authorization: Bearer " : "",
            auth_header,
            auth_header[0] != '\0' ? "\r\n" : "",
            content_type != NULL ? content_type : "application/octet-stream",
            (unsigned int)body_length
        );
    } else {
        request_length = snprintf(
            request,
            sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%u\r\n"
            "User-Agent: hermes-agent-3ds/0.2.0\r\n"
            "Connection: close\r\n"
            "Accept: application/json\r\n"
            "%s%s%s"
            "\r\n",
            method,
            path,
            host,
            (unsigned int)port,
            auth_header[0] != '\0' ? "Authorization: Bearer " : "",
            auth_header,
            auth_header[0] != '\0' ? "\r\n" : ""
        );
    }

    if (request_length <= 0 || (size_t)request_length >= sizeof(request)) {
        set_error(error_out, error_out_size, "HTTP request was too large.");
        close(socket_fd);
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    rc = send_all(socket_fd, request, (size_t)request_length);
    if (R_FAILED(rc)) {
        set_error(error_out, error_out_size, "Could not send the request.");
        close(socket_fd);
        return rc;
    }

    if (has_body) {
        rc = send_all(socket_fd, body, body_length);
        if (R_FAILED(rc)) {
            set_error(error_out, error_out_size, "Could not send the request body.");
            close(socket_fd);
            return rc;
        }
    }

    while (response_used < response_size - 1) {
        Result wait_rc = wait_for_socket(socket_fd, false, BRIDGE_V2_IO_TIMEOUT_SECONDS);
        ssize_t received;

        if (R_FAILED(wait_rc)) {
            set_error(error_out, error_out_size, "Timed out waiting for a Hermes reply.");
            close(socket_fd);
            return wait_rc;
        }

        received = recv(socket_fd, response + response_used, response_size - 1 - response_used, 0);
        if (received < 0) {
            if (errno == EINTR)
                continue;
            set_error(error_out, error_out_size, "Hermes gateway response read failed.");
            close(socket_fd);
            return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
        }

        if (received == 0)
            break;

        response_used += (size_t)received;
        response[response_used] = '\0';
        if (response_complete(response, response_used))
            break;
    }

    close(socket_fd);
    response[response_used] = '\0';

    if (http_status != NULL) {
        unsigned int status_code = 0;
        if (sscanf(response, "HTTP/%*s %u", &status_code) != 1) {
            set_error(error_out, error_out_size, "Bridge returned an invalid HTTP response.");
            return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
        }
        *http_status = status_code;
    }

    return 0;
}

static const char* response_body_start(const char* response)
{
    const char* body = strstr(response, "\r\n\r\n");
    if (body == NULL)
        return NULL;
    return body + 4;
}

void bridge_v2_conversation_list_result_reset(BridgeV2ConversationListResult* result)
{
    if (result == NULL)
        return;
    memset(result, 0, sizeof(*result));
}

void bridge_v2_message_result_reset(BridgeV2MessageResult* result)
{
    if (result == NULL)
        return;
    memset(result, 0, sizeof(*result));
}

void bridge_v2_event_poll_result_reset(BridgeV2EventPollResult* result)
{
    if (result == NULL)
        return;
    memset(result, 0, sizeof(*result));
}

void bridge_v2_interaction_result_reset(BridgeV2InteractionResult* result)
{
    if (result == NULL)
        return;
    memset(result, 0, sizeof(*result));
}

static Result parse_message_ack_response(const char* response, u32 status_code, BridgeV2MessageResult* result, const char* failure_label)
{
    const char* response_body;

    response_body = response_body_start(response);
    if (response_body == NULL) {
        set_error(result->error, sizeof(result->error), "Hermes gateway response body was missing.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (status_code != 200) {
        if (!extract_json_string_v2(response_body, "error", result->error, sizeof(result->error)))
            snprintf(result->error, sizeof(result->error), "Hermes gateway returned HTTP %lu.", (unsigned long)status_code);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (!response_ok(response_body)) {
        if (!extract_json_string_v2(response_body, "error", result->error, sizeof(result->error)))
            snprintf(result->error, sizeof(result->error), "%s response was not OK.", failure_label != NULL ? failure_label : "Request");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    if (!extract_json_string_v2(response_body, "chat_id", result->chat_id, sizeof(result->chat_id)) ||
        !extract_json_string_v2(response_body, "conversation_id", result->conversation_id, sizeof(result->conversation_id)) ||
        !extract_json_string_v2(response_body, "message_id", result->message_id, sizeof(result->message_id))) {
        snprintf(result->error, sizeof(result->error), "%s acknowledgement was incomplete.", failure_label != NULL ? failure_label : "Request");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (extract_json_u32(response_body, "cursor", &result->cursor) == false)
        result->cursor = 0;

    result->success = true;
    result->error[0] = '\0';
    return 0;
}

Result bridge_v2_list_conversations(const char* url, const char* token, const char* device_id, BridgeV2ConversationListResult* result)
{
    char host[64];
    char path[192];
    char path_with_query[320];
    char full_url[512];
    char response[BRIDGE_V2_RESPONSE_MAX];
    const char* body;
    const char* cursor;
    u16 port = 0;
    u32 status_code = 0;
    Result rc;

    bridge_v2_conversation_list_result_reset(result);
    if (result == NULL)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    if (url == NULL || device_id == NULL || device_id[0] == '\0') {
        set_error(result->error, sizeof(result->error), "device_id is required.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }
    if (!is_safe_query_value(device_id)) {
        set_error(result->error, sizeof(result->error), "device_id contains unsupported characters.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    if (!parse_http_url(url, host, sizeof(host), &port, path, sizeof(path))) {
        set_error(result->error, sizeof(result->error), "Bridge URL is invalid.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    snprintf(path_with_query, sizeof(path_with_query), "%s?device_id=%s&limit=%u", path, device_id, (unsigned int)BRIDGE_V2_CONVERSATION_COUNT_MAX);
    snprintf(full_url, sizeof(full_url), "http://%s:%u%s", host, (unsigned int)port, path_with_query);

    rc = perform_request("GET", full_url, token, NULL, 0, NULL, response, sizeof(response), &status_code, result->error, sizeof(result->error));
    if (R_FAILED(rc))
        return rc;

    body = response_body_start(response);
    if (body == NULL) {
        set_error(result->error, sizeof(result->error), "Hermes gateway response body was missing.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (status_code != 200) {
        if (!extract_json_string_v2(body, "error", result->error, sizeof(result->error)))
            snprintf(result->error, sizeof(result->error), "Hermes gateway returned HTTP %lu.", (unsigned long)status_code);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (!response_ok(body)) {
        if (!extract_json_string_v2(body, "error", result->error, sizeof(result->error)))
            set_error(result->error, sizeof(result->error), "Conversations response was not OK.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    cursor = strstr(body, "\"conversations\"");
    if (cursor != NULL)
        cursor = strstr(cursor, "\"conversation_id\"");
    while (cursor != NULL && result->count < BRIDGE_V2_CONVERSATION_COUNT_MAX) {
        BridgeV2ConversationInfo* info = &result->conversations[result->count];
        if (!extract_json_string_v2(cursor, "conversation_id", info->conversation_id, sizeof(info->conversation_id)))
            break;
        extract_json_string_v2(cursor, "session_id", info->session_id, sizeof(info->session_id));
        extract_json_string_v2(cursor, "title", info->title, sizeof(info->title));
        extract_json_string_v2(cursor, "preview", info->preview, sizeof(info->preview));
        extract_json_string_v2(cursor, "updated_at", info->updated_at, sizeof(info->updated_at));
        result->count++;
        cursor = strstr(cursor + 1, "\"conversation_id\"");
    }

    result->success = true;
    result->error[0] = '\0';
    return 0;
}

Result bridge_v2_send_message(const char* url, const char* token, const char* device_id, const char* conversation_id, const char* message, BridgeV2MessageResult* result)
{
    char escaped_message[(BRIDGE_V2_TEXT_MAX * 2) + 32];
    char escaped_device_id[(BRIDGE_V2_CONVERSATION_ID_MAX * 2) + 32];
    char escaped_conversation_id[(BRIDGE_V2_CONVERSATION_ID_MAX * 2) + 32];
    char body[BRIDGE_V2_MESSAGE_BODY_MAX];
    char response[BRIDGE_V2_RESPONSE_MAX];
    u32 status_code = 0;
    Result rc;
    int body_length;

    bridge_v2_message_result_reset(result);
    if (result == NULL)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    if (url == NULL || device_id == NULL || device_id[0] == '\0' || message == NULL || message[0] == '\0') {
        set_error(result->error, sizeof(result->error), "device_id and message are required.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }
    if (!is_safe_query_value(device_id) || !is_safe_query_value(conversation_id != NULL ? conversation_id : "main")) {
        set_error(result->error, sizeof(result->error), "device_id or conversation_id contains unsupported characters.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    if (!escape_json_string(device_id, escaped_device_id, sizeof(escaped_device_id)) ||
        !escape_json_string(conversation_id != NULL ? conversation_id : "main", escaped_conversation_id, sizeof(escaped_conversation_id)) ||
        !escape_json_string(message, escaped_message, sizeof(escaped_message))) {
        set_error(result->error, sizeof(result->error), "Message was too long to send.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    body_length = snprintf(
        body,
        sizeof(body),
        "{\"device_id\":\"%s\",\"conversation_id\":\"%s\",\"text\":\"%s\",\"client\":{\"platform\":\"3ds\",\"app_version\":\"0.2.0\"}}",
        escaped_device_id,
        escaped_conversation_id,
        escaped_message
    );
    if (body_length <= 0 || (size_t)body_length >= sizeof(body)) {
        set_error(result->error, sizeof(result->error), "Message was too long to send.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    rc = perform_request("POST", url, token, body, strlen(body), "application/json", response, sizeof(response), &status_code, result->error, sizeof(result->error));
    if (R_FAILED(rc))
        return rc;

    return parse_message_ack_response(response, status_code, result, "Message");
}

Result bridge_v2_send_voice_message(const char* url, const char* token, const char* device_id, const char* conversation_id, const void* wav_data, size_t wav_size, BridgeV2MessageResult* result)
{
    char host[64];
    char path[192];
    char path_with_query[320];
    char full_url[512];
    char response[BRIDGE_V2_RESPONSE_MAX];
    u16 port = 0;
    u32 status_code = 0;
    Result rc;

    bridge_v2_message_result_reset(result);
    if (result == NULL)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    if (url == NULL || device_id == NULL || device_id[0] == '\0' || wav_data == NULL || wav_size == 0) {
        set_error(result->error, sizeof(result->error), "device_id and wav_data are required.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }
    if (!is_safe_query_value(device_id) || !is_safe_query_value(conversation_id != NULL ? conversation_id : "main")) {
        set_error(result->error, sizeof(result->error), "device_id or conversation_id contains unsupported characters.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    if (!parse_http_url(url, host, sizeof(host), &port, path, sizeof(path))) {
        set_error(result->error, sizeof(result->error), "Bridge URL is invalid.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    snprintf(
        path_with_query,
        sizeof(path_with_query),
        "%s?device_id=%s&conversation_id=%s",
        path,
        device_id,
        conversation_id != NULL ? conversation_id : "main"
    );
    snprintf(full_url, sizeof(full_url), "http://%s:%u%s", host, (unsigned int)port, path_with_query);

    rc = perform_request("POST", full_url, token, wav_data, wav_size, "audio/wav", response, sizeof(response), &status_code, result->error, sizeof(result->error));
    if (R_FAILED(rc))
        return rc;

    return parse_message_ack_response(response, status_code, result, "Voice upload");
}

Result bridge_v2_poll_events(const char* url, const char* token, const char* device_id, const char* conversation_id, u32 cursor, u32 wait_ms, BridgeV2EventPollResult* result)
{
    char host[64];
    char path[192];
    char path_with_query[320];
    char full_url[512];
    char response[BRIDGE_V2_RESPONSE_MAX];
    const char* body;
    u16 port = 0;
    u32 status_code = 0;
    Result rc;
    u32 parsed_cursor = 0;
    u32 missed = 0;

    bridge_v2_event_poll_result_reset(result);
    if (result == NULL)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    if (url == NULL || device_id == NULL || device_id[0] == '\0') {
        set_error(result->error, sizeof(result->error), "device_id is required.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }
    if (!is_safe_query_value(device_id) || !is_safe_query_value(conversation_id != NULL ? conversation_id : "main")) {
        set_error(result->error, sizeof(result->error), "device_id or conversation_id contains unsupported characters.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    if (!parse_http_url(url, host, sizeof(host), &port, path, sizeof(path))) {
        set_error(result->error, sizeof(result->error), "Bridge URL is invalid.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    snprintf(
        path_with_query,
        sizeof(path_with_query),
        "%s?device_id=%s&conversation_id=%s&cursor=%lu&wait=%lu",
        path,
        device_id,
        conversation_id != NULL ? conversation_id : "main",
        (unsigned long)cursor,
        (unsigned long)wait_ms
    );
    snprintf(full_url, sizeof(full_url), "http://%s:%u%s", host, (unsigned int)port, path_with_query);

    rc = perform_request("GET", full_url, token, NULL, 0, NULL, response, sizeof(response), &status_code, result->error, sizeof(result->error));
    if (R_FAILED(rc))
        return rc;

    body = response_body_start(response);
    if (body == NULL) {
        set_error(result->error, sizeof(result->error), "Hermes gateway response body was missing.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (status_code != 200) {
        if (!extract_json_string_v2(body, "error", result->error, sizeof(result->error)))
            snprintf(result->error, sizeof(result->error), "Hermes gateway returned HTTP %lu.", (unsigned long)status_code);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (!response_ok(body)) {
        if (!extract_json_string_v2(body, "error", result->error, sizeof(result->error)))
            set_error(result->error, sizeof(result->error), "Event response was not OK.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    if (extract_json_u32(body, "cursor", &parsed_cursor))
        result->cursor = parsed_cursor;
    if (extract_json_u32(body, "missed_events", &missed))
        result->missed_events = missed > 0;

    if (strstr(body, "\"approval.request\"") != NULL) {
        result->approval_required = true;
        snprintf(result->event_type, sizeof(result->event_type), "%s", "approval.request");
        extract_json_string_after(body, "\"approval.request\"", "request_id", result->request_id, sizeof(result->request_id));
        result->success = true;
        return 0;
    }

    if (strstr(body, "\"message.updated\"") != NULL) {
        snprintf(result->event_type, sizeof(result->event_type), "%s", "message.updated");
        extract_json_string_after(body, "\"message.updated\"", "text", result->reply_text, sizeof(result->reply_text));
        extract_json_string_after(body, "\"message.updated\"", "reply_to", result->reply_to_message_id, sizeof(result->reply_to_message_id));
        result->success = true;
        return 0;
    }

    if (strstr(body, "\"message.created\"") != NULL) {
        snprintf(result->event_type, sizeof(result->event_type), "%s", "message.created");
        extract_json_string_after(body, "\"message.created\"", "text", result->reply_text, sizeof(result->reply_text));
        extract_json_string_after(body, "\"message.created\"", "reply_to", result->reply_to_message_id, sizeof(result->reply_to_message_id));
        result->success = true;
        return 0;
    }

    result->success = true;
    return 0;
}

Result bridge_v2_submit_interaction(const char* url, const char* token, const char* choice, BridgeV2InteractionResult* result)
{
    char escaped_choice[48];
    char body[128];
    char response[BRIDGE_V2_RESPONSE_MAX];
    const char* response_body;
    Result rc;
    u32 status_code = 0;

    bridge_v2_interaction_result_reset(result);
    if (result == NULL)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    if (url == NULL || choice == NULL || choice[0] == '\0') {
        set_error(result->error, sizeof(result->error), "choice is required.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    if (!escape_json_string(choice, escaped_choice, sizeof(escaped_choice))) {
        set_error(result->error, sizeof(result->error), "Choice was too long.");
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SIZE);
    }

    snprintf(body, sizeof(body), "{\"choice\":\"%s\"}", escaped_choice);

    rc = perform_request("POST", url, token, body, strlen(body), "application/json", response, sizeof(response), &status_code, result->error, sizeof(result->error));
    if (R_FAILED(rc))
        return rc;

    response_body = response_body_start(response);
    if (response_body == NULL) {
        set_error(result->error, sizeof(result->error), "Hermes gateway response body was missing.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (status_code != 200) {
        if (!extract_json_string_v2(response_body, "error", result->error, sizeof(result->error)))
            snprintf(result->error, sizeof(result->error), "Hermes gateway returned HTTP %lu.", (unsigned long)status_code);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (!response_ok(response_body)) {
        if (!extract_json_string_v2(response_body, "error", result->error, sizeof(result->error)))
            set_error(result->error, sizeof(result->error), "Interaction response was not OK.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    extract_json_string_v2(response_body, "choice", result->choice, sizeof(result->choice));
    extract_json_string_v2(response_body, "request_id", result->request_id, sizeof(result->request_id));
    result->success = true;
    result->error[0] = '\0';
    return 0;
}
