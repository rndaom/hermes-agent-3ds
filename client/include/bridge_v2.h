#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#define BRIDGE_V2_TEXT_MAX 8192
#define BRIDGE_V2_ERROR_MAX 160
#define BRIDGE_V2_EVENT_TYPE_MAX 32
#define BRIDGE_V2_REQUEST_ID_MAX 64
#define BRIDGE_V2_CONVERSATION_ID_MAX 64
#define BRIDGE_V2_CONVERSATION_TITLE_MAX 64
#define BRIDGE_V2_CONVERSATION_PREVIEW_MAX 96
#define BRIDGE_V2_CONVERSATION_COUNT_MAX 8
#define BRIDGE_V2_MEDIA_ID_MAX 64
#define BRIDGE_V2_MEDIA_TYPE_MAX 32
#define BRIDGE_V2_INTERACTION_TITLE_MAX 64
#define BRIDGE_V2_INTERACTION_TEXT_MAX 192
#define BRIDGE_V2_INTERACTION_CHOICE_MAX 32
#define BRIDGE_V2_INTERACTION_LABEL_MAX 48
#define BRIDGE_V2_INTERACTION_OPTION_COUNT_MAX 24

typedef struct BridgeV2InteractionOption {
    char choice[BRIDGE_V2_INTERACTION_CHOICE_MAX];
    char label[BRIDGE_V2_INTERACTION_LABEL_MAX];
} BridgeV2InteractionOption;

typedef struct BridgeV2ConversationInfo {
    char conversation_id[BRIDGE_V2_CONVERSATION_ID_MAX];
    char session_id[BRIDGE_V2_REQUEST_ID_MAX];
    char title[BRIDGE_V2_CONVERSATION_TITLE_MAX];
    char preview[BRIDGE_V2_CONVERSATION_PREVIEW_MAX];
    char updated_at[40];
} BridgeV2ConversationInfo;

typedef struct BridgeV2ConversationListResult {
    bool success;
    size_t count;
    BridgeV2ConversationInfo conversations[BRIDGE_V2_CONVERSATION_COUNT_MAX];
    char error[BRIDGE_V2_ERROR_MAX];
} BridgeV2ConversationListResult;

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
    bool interaction_required;
    bool missed_events;
    u32 cursor;
    char event_type[BRIDGE_V2_EVENT_TYPE_MAX];
    char request_id[BRIDGE_V2_REQUEST_ID_MAX];
    char reply_to_message_id[BRIDGE_V2_REQUEST_ID_MAX];
    char reply_text[BRIDGE_V2_TEXT_MAX];
    char media_id[BRIDGE_V2_MEDIA_ID_MAX];
    char media_type[BRIDGE_V2_MEDIA_TYPE_MAX];
    u16 media_width;
    u16 media_height;
    char interaction_title[BRIDGE_V2_INTERACTION_TITLE_MAX];
    char interaction_text[BRIDGE_V2_INTERACTION_TEXT_MAX];
    size_t interaction_option_count;
    BridgeV2InteractionOption interaction_options[BRIDGE_V2_INTERACTION_OPTION_COUNT_MAX];
    char error[BRIDGE_V2_ERROR_MAX];
} BridgeV2EventPollResult;

typedef struct BridgeV2InteractionResult {
    bool success;
    char request_id[BRIDGE_V2_REQUEST_ID_MAX];
    char choice[BRIDGE_V2_INTERACTION_CHOICE_MAX];
    char error[BRIDGE_V2_ERROR_MAX];
} BridgeV2InteractionResult;

void bridge_v2_conversation_list_result_reset(BridgeV2ConversationListResult* result);
void bridge_v2_message_result_reset(BridgeV2MessageResult* result);
void bridge_v2_event_poll_result_reset(BridgeV2EventPollResult* result);
void bridge_v2_interaction_result_reset(BridgeV2InteractionResult* result);

Result bridge_v2_list_conversations(const char* url, const char* token, const char* device_id, BridgeV2ConversationListResult* result);
Result bridge_v2_send_message(const char* url, const char* token, const char* device_id, const char* conversation_id, const char* message, BridgeV2MessageResult* result);
Result bridge_v2_send_image_message(const char* url, const char* token, const char* device_id, const char* conversation_id, const void* image_data, size_t image_size, const char* content_type, BridgeV2MessageResult* result);
Result bridge_v2_send_voice_message(const char* url, const char* token, const char* device_id, const char* conversation_id, const void* wav_data, size_t wav_size, BridgeV2MessageResult* result);
Result bridge_v2_poll_events(const char* url, const char* token, const char* device_id, const char* conversation_id, u32 cursor, u32 wait_ms, BridgeV2EventPollResult* result);
Result bridge_v2_submit_interaction(const char* url, const char* token, const char* choice, BridgeV2InteractionResult* result);
Result bridge_v2_download_media(const char* url, const char* token, void** out_data, size_t* out_size, char* out_content_type, size_t out_content_type_size, char* error_out, size_t error_out_size);
