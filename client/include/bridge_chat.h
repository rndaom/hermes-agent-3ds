#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define BRIDGE_CHAT_MESSAGE_MAX 256
#define BRIDGE_CHAT_REPLY_MAX 1200
#define BRIDGE_CHAT_ERROR_MAX 160

typedef struct BridgeChatResult {
    bool success;
    bool truncated;
    u32 http_status;
    int socket_errno;
    char socket_stage[32];
    char reply[BRIDGE_CHAT_REPLY_MAX];
    char error[BRIDGE_CHAT_ERROR_MAX];
} BridgeChatResult;

static inline void bridge_chat_result_reset(BridgeChatResult* result)
{
    if (result == NULL)
        return;

    memset(result, 0, sizeof(*result));
}
