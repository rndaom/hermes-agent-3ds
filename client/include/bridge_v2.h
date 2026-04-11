#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#define BRIDGE_V2_TEXT_MAX 1200
#define BRIDGE_V2_ERROR_MAX 160
#define BRIDGE_V2_EVENT_TYPE_MAX 32
#define BRIDGE_V2_REQUEST_ID_MAX 64
#define BRIDGE_V2_CONVERSATION_ID_MAX 64

typedef struct BridgeV2CapabilitiesResult {
    bool success;
    char service[64];
    char platform[16];
    char transport[32];
    char error[BRIDGE_V2_ERROR_MAX];
} BridgeV2CapabilitiesResult;

typedef struct BridgeV2MessageResult {
    bool success;
    u32 cursor;
    char chat_id[64];
    char conversation_id[BRIDGE_V2_CONVERSATION_ID_MAX];
    char message_id[BRIDGE_V2_REQUEST_ID_MAX];
    char error[BRIDGE_V2_ERROR_MAX];
} BridgeV2MessageResult;

typedef struct BridgeV2EventPollResult {
    bool success;
    bool approval_required;
    bool missed_events;
    u32 cursor;
    char event_type[BRIDGE_V2_EVENT_TYPE_MAX];
    char request_id[BRIDGE_V2_REQUEST_ID_MAX];
    char reply_text[BRIDGE_V2_TEXT_MAX];
    char error[BRIDGE_V2_ERROR_MAX];
} BridgeV2EventPollResult;

typedef struct BridgeV2InteractionResult {
    bool success;
    char request_id[BRIDGE_V2_REQUEST_ID_MAX];
    char choice[16];
    char error[BRIDGE_V2_ERROR_MAX];
} BridgeV2InteractionResult;

void bridge_v2_capabilities_result_reset(BridgeV2CapabilitiesResult* result);
void bridge_v2_message_result_reset(BridgeV2MessageResult* result);
void bridge_v2_event_poll_result_reset(BridgeV2EventPollResult* result);
void bridge_v2_interaction_result_reset(BridgeV2InteractionResult* result);

Result bridge_v2_get_capabilities(const char* url, const char* token, BridgeV2CapabilitiesResult* result);
Result bridge_v2_send_message(const char* url, const char* token, const char* device_id, const char* conversation_id, const char* message, BridgeV2MessageResult* result);
Result bridge_v2_poll_events(const char* url, const char* token, const char* device_id, const char* conversation_id, u32 cursor, u32 wait_ms, BridgeV2EventPollResult* result);
Result bridge_v2_submit_interaction(const char* url, const char* token, const char* choice, BridgeV2InteractionResult* result);
