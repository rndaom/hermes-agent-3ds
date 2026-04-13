#include "bridge_health.h"

#include <3ds.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
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

#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000
#define BRIDGE_HEALTH_CONNECT_TIMEOUT_SECONDS 5
#define BRIDGE_HEALTH_IO_TIMEOUT_SECONDS 5
#define BRIDGE_HEALTH_RESPONSE_MAX 2048
#define BRIDGE_HEALTH_PATH_MAX 896

static u32* g_soc_buffer = NULL;
static bool g_soc_ready = false;

static void set_error(BridgeHealthResult* result, const char* message)
{
    if (result == NULL)
        return;

    result->success = false;
    snprintf(result->error, sizeof(result->error), "%s", message);
}

static void set_socket_debug(BridgeHealthResult* result, const char* stage, int socket_errno)
{
    if (result == NULL)
        return;

    result->socket_errno = socket_errno;
    snprintf(result->socket_stage, sizeof(result->socket_stage), "%s", stage != NULL ? stage : "unknown");
}

static bool extract_json_string(const char* json, const char* key, char* out, size_t out_size)
{
    char pattern[64];
    const char* start;
    const char* end;
    size_t length;

    if (json == NULL || key == NULL || out == NULL || out_size == 0)
        return false;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    start = strstr(json, pattern);
    if (start == NULL)
        return false;

    start += strlen(pattern);
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;

    if (*start != ':')
        return false;

    start++;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;

    if (*start != '"')
        return false;

    start++;
    end = strchr(start, '"');
    if (end == NULL)
        return false;

    length = (size_t)(end - start);
    if (length >= out_size)
        length = out_size - 1;

    memcpy(out, start, length);
    out[length] = '\0';
    return true;
}

static bool extract_json_u32(const char* json, const char* key, u32* out_value)
{
    char pattern[64];
    const char* start;
    char* end_ptr;
    unsigned long parsed;

    if (json == NULL || key == NULL || out_value == NULL)
        return false;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    start = strstr(json, pattern);
    if (start == NULL)
        return false;

    start += strlen(pattern);
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;
    if (*start != ':')
        return false;

    start++;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;

    if (*start < '0' || *start > '9')
        return false;

    parsed = strtoul(start, &end_ptr, 10);
    if (end_ptr == start)
        return false;

    *out_value = (u32)parsed;
    return true;
}

static bool parse_health_url(const char* url, char* host, size_t host_size, u16* port, char* path, size_t path_size)
{
    const char* cursor;
    const char* host_end;
    const char* slash;
    const char* colon;
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

    if (*slash == '\0') {
        if (snprintf(path, path_size, "/") <= 0 || strlen(path) >= path_size)
            return false;
    } else {
        int written = snprintf(path, path_size, "%s", slash);
        if (written <= 0 || (size_t)written >= path_size)
            return false;
    }

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

static Result connect_with_timeout(int socket_fd, const struct sockaddr_in* address, BridgeHealthResult* result)
{
    int flags;
    int connect_rc;
    int socket_error = 0;
    socklen_t socket_error_size = sizeof(socket_error);
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

    wait_rc = wait_for_socket(socket_fd, true, BRIDGE_HEALTH_CONNECT_TIMEOUT_SECONDS);
    if (R_FAILED(wait_rc)) {
        set_socket_debug(result, "connect-wait", errno);
        fcntl(socket_fd, F_SETFL, flags);
        return wait_rc;
    }

    if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_size) != 0) {
        set_socket_debug(result, "connect-verify", errno);
        fcntl(socket_fd, F_SETFL, flags);
        return MAKERESULT(RL_FATAL, RS_INTERNAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }
    if (socket_error != 0) {
        set_socket_debug(result, "connect-error", socket_error);
        errno = socket_error;
        fcntl(socket_fd, F_SETFL, flags);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
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

void bridge_health_result_reset(BridgeHealthResult* result)
{
    if (result == NULL)
        return;

    memset(result, 0, sizeof(*result));
}

Result bridge_health_network_init(void)
{
    Result rc;

    if (g_soc_ready)
        return 0;

    g_soc_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (g_soc_buffer == NULL)
        return MAKERESULT(RL_FATAL, RS_OUTOFRESOURCE, RM_APPLICATION, RD_OUT_OF_MEMORY);

    rc = socInit(g_soc_buffer, SOC_BUFFERSIZE);
    if (R_FAILED(rc)) {
        free(g_soc_buffer);
        g_soc_buffer = NULL;
        return rc;
    }

    g_soc_ready = true;
    return 0;
}

void bridge_health_network_exit(void)
{
    if (!g_soc_ready)
        return;

    socExit();
    free(g_soc_buffer);
    g_soc_buffer = NULL;
    g_soc_ready = false;
}

Result bridge_health_check_run(const char* url, BridgeHealthResult* result)
{
    Result wifi_rc;
    Result rc;
    u32 wifi_status = 0;
    char host[64];
    char path[BRIDGE_HEALTH_PATH_MAX];
    u16 port = 0;
    struct sockaddr_in address;
    int socket_fd = -1;
    char request[1400];
    char response[BRIDGE_HEALTH_RESPONSE_MAX];
    size_t response_used = 0;
    const char* body;
    unsigned int status_code = 0;

    if (result == NULL)
        return MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);

    bridge_health_result_reset(result);

    wifi_rc = ACU_GetWifiStatus(&wifi_status);
    if (R_FAILED(wifi_rc)) {
        set_error(result, "Could not read Wi-Fi status.");
        return wifi_rc;
    }

    if (wifi_status == 0) {
        set_error(result, "Wi-Fi is not connected.");
        return MAKERESULT(RL_STATUS, RS_NOTFOUND, RM_APPLICATION, RD_NOT_FOUND);
    }

    if (!g_soc_ready) {
        set_error(result, "Socket service is not initialized.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_NOT_INITIALIZED);
    }

    if (!parse_health_url(url, host, sizeof(host), &port, path, sizeof(path))) {
        set_error(result, "Bridge URL is invalid.");
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
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "User-Agent: hermes-agent-3ds/0.2.0\r\n"
        "Connection: close\r\n"
        "Accept: application/json\r\n"
        "\r\n",
        path,
        host,
        (unsigned int)port
    );

    rc = send_all(socket_fd, request, strlen(request));
    if (R_FAILED(rc)) {
        set_socket_debug(result, "send", errno);
        set_error(result, "Could not send the bridge request.");
        close(socket_fd);
        return rc;
    }

    memset(response, 0, sizeof(response));
    while (response_used < sizeof(response) - 1) {
        Result wait_rc = wait_for_socket(socket_fd, false, BRIDGE_HEALTH_IO_TIMEOUT_SECONDS);
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
        response[response_used] = '\0';
        if (response_complete(response, response_used))
            break;
    }

    close(socket_fd);
    response[response_used] = '\0';

    if (sscanf(response, "HTTP/%*s %u", &status_code) != 1) {
        set_error(result, "Bridge returned an invalid HTTP response.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    result->http_status = status_code;
    if (status_code != 200) {
        snprintf(result->error, sizeof(result->error), "Bridge returned HTTP %u.", status_code);
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    body = strstr(response, "\r\n\r\n");
    if (body == NULL) {
        set_error(result, "Bridge response body was missing.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    body += 4;
    if (strstr(body, "\"ok\":true") == NULL && strstr(body, "\"ok\": true") == NULL) {
        set_error(result, "Bridge response was not OK.");
        return MAKERESULT(RL_STATUS, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    if (!extract_json_string(body, "service", result->service, sizeof(result->service)))
        snprintf(result->service, sizeof(result->service), "unknown-service");

    if (!extract_json_string(body, "version", result->version, sizeof(result->version)))
        snprintf(result->version, sizeof(result->version), "unknown-version");

    if (!extract_json_string(body, "model_name", result->model_name, sizeof(result->model_name)))
        result->model_name[0] = '\0';

    if (!extract_json_u32(body, "context_length", &result->context_length))
        result->context_length = 0;
    if (!extract_json_u32(body, "context_tokens", &result->context_tokens))
        result->context_tokens = 0;
    if (!extract_json_u32(body, "context_percent", &result->context_percent))
        result->context_percent = 0;

    result->success = true;
    result->error[0] = '\0';
    return 0;
}
