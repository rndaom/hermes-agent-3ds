#include "picture_input.h"

#include "app_ui.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PICTURE_INPUT_TIMEOUT_NS 300000000ULL
#define PICTURE_INPUT_PREVIEW_TIMEOUT_NS 120000000ULL
#define PICTURE_INPUT_CAPTURE_WARMUP_NS 450000000ULL

static void set_status(char* status_line, size_t status_line_size, const char* message)
{
    if (status_line == NULL || status_line_size == 0)
        return;

    snprintf(status_line, status_line_size, "%s", message != NULL ? message : "Picture input error.");
}

static void picture_status_from_rc(char* status_line, size_t status_line_size, const char* label, Result rc)
{
    if (status_line == NULL || status_line_size == 0)
        return;

    snprintf(status_line, status_line_size, "%s failed: 0x%08lX", label != NULL ? label : "Camera", (unsigned long)rc);
}

static void write_le16(u8* out, u16 value)
{
    out[0] = (u8)(value & 0xFF);
    out[1] = (u8)((value >> 8) & 0xFF);
}

static void write_le32(u8* out, u32 value)
{
    out[0] = (u8)(value & 0xFF);
    out[1] = (u8)((value >> 8) & 0xFF);
    out[2] = (u8)((value >> 16) & 0xFF);
    out[3] = (u8)((value >> 24) & 0xFF);
}

static u8 expand_5_to_8(u16 value)
{
    return (u8)(((u32)value * 255U + 15U) / 31U);
}

static u8 expand_6_to_8(u16 value)
{
    return (u8)(((u32)value * 255U + 31U) / 63U);
}

static u8 brighten_channel(u8 value)
{
    u32 inverse = 255U - (u32)value;

    // A lightweight gamma-style lift helps the 3DS camera read better indoors.
    return (u8)(255U - ((inverse * inverse) / 255U));
}

typedef struct PictureToneMap {
    float gain;
    u8 shadow_lift;
} PictureToneMap;

typedef struct PictureProcessedFrame {
    u16 width;
    u16 height;
    u8* rgba8_data;
} PictureProcessedFrame;

typedef struct PictureCameraSession {
    bool cam_ready;
    bool capture_started;
    bool camera_active;
    u32 transfer_bytes;
} PictureCameraSession;

static u8 tone_map_channel(u8 value, const PictureToneMap* tone)
{
    float boosted;

    if (tone == NULL)
        return brighten_channel(value);

    boosted = ((float)value * tone->gain) + (float)tone->shadow_lift;
    if (boosted < 96.0f)
        boosted += (96.0f - boosted) * 0.18f;
    if (boosted > 255.0f)
        boosted = 255.0f;
    if (boosted < 0.0f)
        boosted = 0.0f;
    return (u8)(boosted + 0.5f);
}

static void picture_processed_frame_reset(PictureProcessedFrame* frame)
{
    if (frame == NULL)
        return;

    frame->width = 0;
    frame->height = 0;
    frame->rgba8_data = NULL;
}

static void picture_processed_frame_free(PictureProcessedFrame* frame)
{
    if (frame == NULL)
        return;

    if (frame->rgba8_data != NULL)
        free(frame->rgba8_data);
    picture_processed_frame_reset(frame);
}

static u8 clamp_u8_from_int(int value)
{
    if (value < 0)
        return 0;
    if (value > 255)
        return 255;
    return (u8)value;
}

static PictureToneMap compute_picture_tone_map(const u16* pixels)
{
    PictureToneMap tone;
    u32 luma_sum = 0;
    size_t sample_count = 0;
    size_t y;
    const float target_luma = 132.0f;

    tone.gain = 1.0f;
    tone.shadow_lift = 0;
    if (pixels == NULL)
        return tone;

    for (y = 0; y < PICTURE_INPUT_CAPTURE_HEIGHT; y += 2) {
        size_t x;

        for (x = 0; x < PICTURE_INPUT_CAPTURE_WIDTH; x += 2) {
            u16 pixel = pixels[y * PICTURE_INPUT_CAPTURE_WIDTH + x];
            u8 red = expand_5_to_8((u16)((pixel >> 11) & 0x1F));
            u8 green = expand_6_to_8((u16)((pixel >> 5) & 0x3F));
            u8 blue = expand_5_to_8((u16)(pixel & 0x1F));

            luma_sum += ((u32)red * 54U + (u32)green * 183U + (u32)blue * 19U) >> 8;
            sample_count++;
        }
    }

    if (sample_count > 0) {
        float average_luma = (float)luma_sum / (float)sample_count;

        if (average_luma < target_luma) {
            float deficit = (target_luma - average_luma) / target_luma;

            tone.gain = 1.0f + (deficit * 1.15f);
            if (tone.gain > 2.25f)
                tone.gain = 2.25f;
        }
        if (average_luma < 86.0f) {
            int lift = (int)((86.0f - average_luma) * 0.38f);

            if (lift > 20)
                lift = 20;
            if (lift < 0)
                lift = 0;
            tone.shadow_lift = (u8)lift;
        }
    }

    return tone;
}

static void camera_rgb565_to_rgba8(u16 pixel, const PictureToneMap* tone, u8* out_rgba)
{
    out_rgba[0] = tone_map_channel(expand_5_to_8((u16)((pixel >> 11) & 0x1F)), tone);
    out_rgba[1] = tone_map_channel(expand_6_to_8((u16)((pixel >> 5) & 0x3F)), tone);
    out_rgba[2] = tone_map_channel(expand_5_to_8((u16)(pixel & 0x1F)), tone);
    out_rgba[3] = 0xFF;
}

static void camera_rgb565_to_rgba8_raw(u16 pixel, u8* out_rgba)
{
    out_rgba[0] = expand_5_to_8((u16)((pixel >> 11) & 0x1F));
    out_rgba[1] = expand_6_to_8((u16)((pixel >> 5) & 0x3F));
    out_rgba[2] = expand_5_to_8((u16)(pixel & 0x1F));
    out_rgba[3] = 0xFF;
}

static bool build_processed_frame_from_camera(const u16* pixels, PictureProcessedFrame* out_frame, char* status_line, size_t status_line_size)
{
    PictureToneMap tone;
    size_t byte_count;
    u8* base_rgba;
    u8* processed_rgba;
    size_t y;

    if (pixels == NULL || out_frame == NULL) {
        set_status(status_line, status_line_size, "Camera image buffer was invalid.");
        return false;
    }

    picture_processed_frame_reset(out_frame);
    tone = compute_picture_tone_map(pixels);
    byte_count = (size_t)PICTURE_INPUT_CAPTURE_WIDTH * (size_t)PICTURE_INPUT_CAPTURE_HEIGHT * 4U;
    base_rgba = (u8*)malloc(byte_count);
    if (base_rgba == NULL) {
        set_status(status_line, status_line_size, "Could not process the camera frame.");
        return false;
    }

    for (y = 0; y < PICTURE_INPUT_CAPTURE_HEIGHT; y++) {
        size_t x;

        for (x = 0; x < PICTURE_INPUT_CAPTURE_WIDTH; x++) {
            camera_rgb565_to_rgba8(
                pixels[y * PICTURE_INPUT_CAPTURE_WIDTH + x],
                &tone,
                base_rgba + (((y * PICTURE_INPUT_CAPTURE_WIDTH) + x) * 4U)
            );
        }
    }

    processed_rgba = (u8*)malloc(byte_count);
    if (processed_rgba == NULL) {
        out_frame->width = PICTURE_INPUT_CAPTURE_WIDTH;
        out_frame->height = PICTURE_INPUT_CAPTURE_HEIGHT;
        out_frame->rgba8_data = base_rgba;
        return true;
    }

    for (y = 0; y < PICTURE_INPUT_CAPTURE_HEIGHT; y++) {
        size_t x;

        for (x = 0; x < PICTURE_INPUT_CAPTURE_WIDTH; x++) {
            size_t pixel_index = ((y * PICTURE_INPUT_CAPTURE_WIDTH) + x) * 4U;
            const u8* base_pixel = base_rgba + pixel_index;
            u8* dest_pixel = processed_rgba + pixel_index;

            if (x == 0 || y == 0 || x + 1U >= PICTURE_INPUT_CAPTURE_WIDTH || y + 1U >= PICTURE_INPUT_CAPTURE_HEIGHT) {
                memcpy(dest_pixel, base_pixel, 4U);
                continue;
            }

            {
                const u8* north_west = base_rgba + ((((y - 1U) * PICTURE_INPUT_CAPTURE_WIDTH) + (x - 1U)) * 4U);
                const u8* north = base_rgba + ((((y - 1U) * PICTURE_INPUT_CAPTURE_WIDTH) + x) * 4U);
                const u8* north_east = base_rgba + ((((y - 1U) * PICTURE_INPUT_CAPTURE_WIDTH) + (x + 1U)) * 4U);
                const u8* west = base_rgba + (((y * PICTURE_INPUT_CAPTURE_WIDTH) + (x - 1U)) * 4U);
                const u8* east = base_rgba + (((y * PICTURE_INPUT_CAPTURE_WIDTH) + (x + 1U)) * 4U);
                const u8* south_west = base_rgba + ((((y + 1U) * PICTURE_INPUT_CAPTURE_WIDTH) + (x - 1U)) * 4U);
                const u8* south = base_rgba + ((((y + 1U) * PICTURE_INPUT_CAPTURE_WIDTH) + x) * 4U);
                const u8* south_east = base_rgba + ((((y + 1U) * PICTURE_INPUT_CAPTURE_WIDTH) + (x + 1U)) * 4U);
                int channel;
                int base_luma = ((int)base_pixel[0] * 54 + (int)base_pixel[1] * 183 + (int)base_pixel[2] * 19) >> 8;
                int blur_luma = 0;

                for (channel = 0; channel < 3; channel++) {
                    int blur =
                        ((int)base_pixel[channel] * 4) +
                        ((int)north[channel] * 2) +
                        ((int)south[channel] * 2) +
                        ((int)west[channel] * 2) +
                        ((int)east[channel] * 2) +
                        (int)north_west[channel] +
                        (int)north_east[channel] +
                        (int)south_west[channel] +
                        (int)south_east[channel];

                    blur = (blur + 8) / 16;
                    if (channel == 0)
                        blur_luma += blur * 54;
                    else if (channel == 1)
                        blur_luma += blur * 183;
                    else
                        blur_luma += blur * 19;

                    dest_pixel[channel] = (u8)blur;
                }

                blur_luma >>= 8;

                for (channel = 0; channel < 3; channel++) {
                    int base_value = (int)base_pixel[channel];
                    int blur_value = (int)dest_pixel[channel];
                    int edge_strength = base_luma - blur_luma;

                    if (edge_strength < 0)
                        edge_strength = -edge_strength;

                    if (edge_strength < 10)
                        dest_pixel[channel] = (u8)((base_value + (blur_value * 3) + 2) / 4);
                    else if (edge_strength < 24)
                        dest_pixel[channel] = (u8)(((base_value * 3) + blur_value + 2) / 4);
                    else
                        dest_pixel[channel] = clamp_u8_from_int(base_value + ((base_value - blur_value) / 4));
                }
                dest_pixel[3] = 0xFF;
            }
        }
    }

    free(base_rgba);
    out_frame->width = PICTURE_INPUT_CAPTURE_WIDTH;
    out_frame->height = PICTURE_INPUT_CAPTURE_HEIGHT;
    out_frame->rgba8_data = processed_rgba;
    return true;
}

void picture_input_preview_reset(PictureInputPreview* preview)
{
    if (preview == NULL)
        return;

    preview->width = 0;
    preview->height = 0;
    preview->rgba8_data = NULL;
}

void picture_input_preview_free(PictureInputPreview* preview)
{
    if (preview == NULL)
        return;

    if (preview->rgba8_data != NULL)
        free(preview->rgba8_data);
    picture_input_preview_reset(preview);
}

static bool build_bmp_from_processed_frame(const PictureProcessedFrame* frame, u8** out_bmp_data, size_t* out_bmp_size, char* status_line, size_t status_line_size)
{
    size_t row_stride;
    size_t pixel_bytes;
    size_t file_size;
    u8* bmp_data;
    u8* cursor;
    int y;

    if (frame == NULL || frame->rgba8_data == NULL || out_bmp_data == NULL || out_bmp_size == NULL) {
        set_status(status_line, status_line_size, "Camera image buffer was invalid.");
        return false;
    }

    row_stride = (((size_t)frame->width * 3U) + 3U) & ~3U;
    pixel_bytes = row_stride * (size_t)frame->height;
    file_size = 54U + pixel_bytes;
    bmp_data = (u8*)malloc(file_size);
    if (bmp_data == NULL) {
        set_status(status_line, status_line_size, "Could not package the picture note.");
        return false;
    }

    memset(bmp_data, 0, file_size);
    memcpy(bmp_data, "BM", 2);
    write_le32(bmp_data + 2, (u32)file_size);
    write_le32(bmp_data + 10, 54);
    write_le32(bmp_data + 14, 40);
    write_le32(bmp_data + 18, frame->width);
    write_le32(bmp_data + 22, (u32)(-(s32)frame->height));
    write_le16(bmp_data + 26, 1);
    write_le16(bmp_data + 28, 24);
    write_le32(bmp_data + 34, (u32)pixel_bytes);
    write_le32(bmp_data + 38, 2835);
    write_le32(bmp_data + 42, 2835);

    cursor = bmp_data + 54;
    for (y = 0; y < frame->height; y++) {
        int x;

        for (x = 0; x < frame->width; x++) {
            const u8* source_pixel = frame->rgba8_data + ((((size_t)y * (size_t)frame->width) + (size_t)x) * 4U);

            cursor[(size_t)x * 3U] = source_pixel[2];
            cursor[(size_t)x * 3U + 1U] = source_pixel[1];
            cursor[(size_t)x * 3U + 2U] = source_pixel[0];
        }
        cursor += row_stride;
    }

    *out_bmp_data = bmp_data;
    *out_bmp_size = file_size;
    return true;
}

static bool build_preview_from_processed_frame(const PictureProcessedFrame* frame, PictureInputPreview* out_preview, char* status_line, size_t status_line_size)
{
    u8* preview_rgba;
    int y;

    if (frame == NULL || frame->rgba8_data == NULL || out_preview == NULL) {
        set_status(status_line, status_line_size, "Camera preview buffer was invalid.");
        return false;
    }

    preview_rgba = (u8*)malloc(PICTURE_INPUT_PREVIEW_WIDTH * PICTURE_INPUT_PREVIEW_HEIGHT * 4U);
    if (preview_rgba == NULL) {
        set_status(status_line, status_line_size, "Could not build the picture preview.");
        return false;
    }

    for (y = 0; y < PICTURE_INPUT_PREVIEW_HEIGHT; y++) {
        int x;
        size_t source_y0 = ((size_t)y * (size_t)frame->height) / PICTURE_INPUT_PREVIEW_HEIGHT;
        size_t source_y1 = (((size_t)y + 1U) * (size_t)frame->height) / PICTURE_INPUT_PREVIEW_HEIGHT;

        if (source_y1 <= source_y0)
            source_y1 = source_y0 + 1U;
        if (source_y1 > frame->height)
            source_y1 = frame->height;

        for (x = 0; x < PICTURE_INPUT_PREVIEW_WIDTH; x++) {
            size_t source_x0 = ((size_t)x * (size_t)frame->width) / PICTURE_INPUT_PREVIEW_WIDTH;
            size_t source_x1 = (((size_t)x + 1U) * (size_t)frame->width) / PICTURE_INPUT_PREVIEW_WIDTH;
            size_t sample_y1;
            size_t sample_x1;
            const u8* sample00;
            const u8* sample10;
            const u8* sample01;
            const u8* sample11;
            u8* dest_pixel = preview_rgba + (((size_t)y * PICTURE_INPUT_PREVIEW_WIDTH + (size_t)x) * 4U);
            size_t channel;

            if (source_x1 <= source_x0)
                source_x1 = source_x0 + 1U;
            if (source_x1 > frame->width)
                source_x1 = frame->width;

            sample_y1 = source_y1 - 1U;
            sample_x1 = source_x1 - 1U;
            sample00 = frame->rgba8_data + (((source_y0 * (size_t)frame->width) + source_x0) * 4U);
            sample10 = frame->rgba8_data + (((source_y0 * (size_t)frame->width) + sample_x1) * 4U);
            sample01 = frame->rgba8_data + (((sample_y1 * (size_t)frame->width) + source_x0) * 4U);
            sample11 = frame->rgba8_data + (((sample_y1 * (size_t)frame->width) + sample_x1) * 4U);

            for (channel = 0; channel < 3; channel++)
                dest_pixel[channel] = (u8)(((u32)sample00[channel] + (u32)sample10[channel] + (u32)sample01[channel] + (u32)sample11[channel] + 2U) / 4U);
            dest_pixel[3] = 0xFF;
        }
    }

    out_preview->width = PICTURE_INPUT_PREVIEW_WIDTH;
    out_preview->height = PICTURE_INPUT_PREVIEW_HEIGHT;
    out_preview->rgba8_data = preview_rgba;
    return true;
}

static bool build_live_preview_from_camera(const u16* pixels, u8* out_rgba, char* status_line, size_t status_line_size)
{
    int y;

    if (pixels == NULL || out_rgba == NULL) {
        set_status(status_line, status_line_size, "Camera preview buffer was invalid.");
        return false;
    }

    for (y = 0; y < PICTURE_INPUT_PREVIEW_HEIGHT; y++) {
        int x;
        size_t source_y0 = ((size_t)y * PICTURE_INPUT_CAPTURE_HEIGHT) / PICTURE_INPUT_PREVIEW_HEIGHT;
        size_t source_y1 = (((size_t)y + 1U) * PICTURE_INPUT_CAPTURE_HEIGHT) / PICTURE_INPUT_PREVIEW_HEIGHT;

        if (source_y1 <= source_y0)
            source_y1 = source_y0 + 1U;
        if (source_y1 > PICTURE_INPUT_CAPTURE_HEIGHT)
            source_y1 = PICTURE_INPUT_CAPTURE_HEIGHT;

        for (x = 0; x < PICTURE_INPUT_PREVIEW_WIDTH; x++) {
            size_t source_x0 = ((size_t)x * PICTURE_INPUT_CAPTURE_WIDTH) / PICTURE_INPUT_PREVIEW_WIDTH;
            size_t source_x1 = (((size_t)x + 1U) * PICTURE_INPUT_CAPTURE_WIDTH) / PICTURE_INPUT_PREVIEW_WIDTH;
            size_t sample_y1;
            size_t sample_x1;
            u8 sample00[4];
            u8 sample10[4];
            u8 sample01[4];
            u8 sample11[4];
            u8* dest_pixel = out_rgba + (((size_t)y * PICTURE_INPUT_PREVIEW_WIDTH + (size_t)x) * 4U);
            size_t channel;

            if (source_x1 <= source_x0)
                source_x1 = source_x0 + 1U;
            if (source_x1 > PICTURE_INPUT_CAPTURE_WIDTH)
                source_x1 = PICTURE_INPUT_CAPTURE_WIDTH;

            sample_y1 = source_y1 - 1U;
            sample_x1 = source_x1 - 1U;

            camera_rgb565_to_rgba8_raw(pixels[source_y0 * PICTURE_INPUT_CAPTURE_WIDTH + source_x0], sample00);
            camera_rgb565_to_rgba8_raw(pixels[source_y0 * PICTURE_INPUT_CAPTURE_WIDTH + sample_x1], sample10);
            camera_rgb565_to_rgba8_raw(pixels[sample_y1 * PICTURE_INPUT_CAPTURE_WIDTH + source_x0], sample01);
            camera_rgb565_to_rgba8_raw(pixels[sample_y1 * PICTURE_INPUT_CAPTURE_WIDTH + sample_x1], sample11);

            for (channel = 0; channel < 3; channel++)
                dest_pixel[channel] = (u8)(((u32)sample00[channel] + (u32)sample10[channel] + (u32)sample01[channel] + (u32)sample11[channel] + 2U) / 4U);
            dest_pixel[3] = 0xFF;
        }
    }

    return true;
}

static void picture_camera_session_reset(PictureCameraSession* session)
{
    if (session == NULL)
        return;

    session->cam_ready = false;
    session->capture_started = false;
    session->camera_active = false;
    session->transfer_bytes = 0;
}

static void picture_camera_session_close(PictureCameraSession* session)
{
    if (session == NULL)
        return;

    if (session->capture_started)
        CAMU_StopCapture(PORT_CAM1);
    if (session->camera_active)
        CAMU_Activate(SELECT_NONE);
    if (session->cam_ready)
        camExit();
    picture_camera_session_reset(session);
}

static bool picture_camera_session_start(PictureCameraSession* session, char* status_line, size_t status_line_size)
{
    Result rc;

    if (session == NULL) {
        set_status(status_line, status_line_size, "Camera image buffer was invalid.");
        return false;
    }

    picture_camera_session_reset(session);

    rc = camInit();
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "camInit", rc);
        return false;
    }
    session->cam_ready = true;

    rc = CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetSize", rc);
        goto cleanup;
    }
    rc = CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetOutputFormat", rc);
        goto cleanup;
    }
    rc = CAMU_SetFrameRate(SELECT_OUT1, FRAME_RATE_30);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetFrameRate", rc);
        goto cleanup;
    }
    rc = CAMU_SetNoiseFilter(SELECT_OUT1, true);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetNoiseFilter", rc);
        goto cleanup;
    }
    rc = CAMU_SetAutoExposure(SELECT_OUT1, true);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetAutoExposure", rc);
        goto cleanup;
    }
    rc = CAMU_SetAutoWhiteBalance(SELECT_OUT1, true);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetAutoWhiteBalance", rc);
        goto cleanup;
    }
    rc = CAMU_SetAutoExposureWindow(
        SELECT_OUT1,
        PICTURE_INPUT_CAPTURE_WIDTH / 4,
        PICTURE_INPUT_CAPTURE_HEIGHT / 4,
        PICTURE_INPUT_CAPTURE_WIDTH / 2,
        PICTURE_INPUT_CAPTURE_HEIGHT / 2
    );
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetAutoExposureWindow", rc);
        goto cleanup;
    }
    rc = CAMU_SetAutoWhiteBalanceWindow(
        SELECT_OUT1,
        PICTURE_INPUT_CAPTURE_WIDTH / 4,
        PICTURE_INPUT_CAPTURE_HEIGHT / 4,
        PICTURE_INPUT_CAPTURE_WIDTH / 2,
        PICTURE_INPUT_CAPTURE_HEIGHT / 2
    );
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetAutoWhiteBalanceWindow", rc);
        goto cleanup;
    }
    rc = CAMU_SetTrimming(PORT_CAM1, false);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetTrimming", rc);
        goto cleanup;
    }

    rc = CAMU_GetMaxBytes(&session->transfer_bytes, PICTURE_INPUT_CAPTURE_WIDTH, PICTURE_INPUT_CAPTURE_HEIGHT);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_GetMaxBytes", rc);
        goto cleanup;
    }
    rc = CAMU_SetTransferBytes(PORT_CAM1, session->transfer_bytes, PICTURE_INPUT_CAPTURE_WIDTH, PICTURE_INPUT_CAPTURE_HEIGHT);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetTransferBytes", rc);
        goto cleanup;
    }
    rc = CAMU_Activate(SELECT_OUT1);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_Activate", rc);
        goto cleanup;
    }
    session->camera_active = true;

    // Let the sensor and auto-exposure settle before the capture we keep.
    svcSleepThread(PICTURE_INPUT_CAPTURE_WARMUP_NS);

    return true;

cleanup:
    picture_camera_session_close(session);
    return false;
}

static bool picture_camera_session_receive_frame(
    PictureCameraSession* session,
    u16* out_pixels,
    u64 timeout_ns,
    bool play_shutter,
    char* status_line,
    size_t status_line_size
)
{
    Result rc;
    bool success = false;
    Handle receive_event = 0;

    if (session == NULL || out_pixels == NULL || !session->capture_started) {
        if (session == NULL || out_pixels == NULL) {
            set_status(status_line, status_line_size, "Camera image buffer was invalid.");
            return false;
        }
    }

    if (session->capture_started) {
        CAMU_StopCapture(PORT_CAM1);
        session->capture_started = false;
    }

    rc = CAMU_ClearBuffer(PORT_CAM1);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_ClearBuffer", rc);
        goto cleanup;
    }

    rc = CAMU_StartCapture(PORT_CAM1);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_StartCapture", rc);
        goto cleanup;
    }
    session->capture_started = true;

    rc = CAMU_SetReceiving(
        &receive_event,
        out_pixels,
        PORT_CAM1,
        PICTURE_INPUT_CAPTURE_WIDTH * PICTURE_INPUT_CAPTURE_HEIGHT * 2U,
        (s16)session->transfer_bytes
    );
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetReceiving", rc);
        goto cleanup;
    }

    rc = svcWaitSynchronization(receive_event, timeout_ns);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, play_shutter ? "Camera capture" : "Camera preview", rc);
        goto cleanup;
    }

    if (play_shutter) {
        CAMU_PlayShutterSound(SHUTTER_SOUND_TYPE_NORMAL);
        set_status(status_line, status_line_size, "Picture captured.");
    }
    success = true;

cleanup:
    if (receive_event != 0)
        svcCloseHandle(receive_event);
    return success;
}

typedef enum PictureInputPromptState {
    PICTURE_INPUT_PROMPT_CAPTURE = 0,
    PICTURE_INPUT_PROMPT_REVIEW = 1,
} PictureInputPromptState;

static void clear_pending_capture(u8** out_bmp_data, size_t* out_bmp_size, PictureInputPreview* out_preview)
{
    if (out_bmp_data != NULL && *out_bmp_data != NULL) {
        free(*out_bmp_data);
        *out_bmp_data = NULL;
    }
    if (out_bmp_size != NULL)
        *out_bmp_size = 0;
    picture_input_preview_free(out_preview);
    hermes_app_ui_picture_capture_clear_preview();
}

bool picture_input_capture_prompt(
    const HermesAppConfig* config,
    u8** out_bmp_data,
    size_t* out_bmp_size,
    PictureInputPreview* out_preview,
    char* status_line,
    size_t status_line_size
)
{
    u16* pixels = NULL;
    u8* live_preview_rgba = NULL;
    bool waiting_for_a_release = (hidKeysHeld() & KEY_A) != 0;
    bool review_waiting_for_a_release = false;
    bool success = false;
    PictureInputPromptState prompt_state = PICTURE_INPUT_PROMPT_CAPTURE;
    PictureCameraSession camera_session;

    if (out_bmp_data == NULL || out_bmp_size == NULL || out_preview == NULL) {
        set_status(status_line, status_line_size, "Picture input is not available.");
        return false;
    }

    picture_camera_session_reset(&camera_session);
    *out_bmp_data = NULL;
    *out_bmp_size = 0;
    picture_input_preview_reset(out_preview);
    hermes_app_ui_picture_capture_clear_preview();

    pixels = (u16*)malloc(PICTURE_INPUT_CAPTURE_WIDTH * PICTURE_INPUT_CAPTURE_HEIGHT * sizeof(u16));
    if (pixels == NULL) {
        set_status(status_line, status_line_size, "Could not allocate the camera buffer.");
        return false;
    }

    live_preview_rgba = (u8*)malloc(PICTURE_INPUT_PREVIEW_WIDTH * PICTURE_INPUT_PREVIEW_HEIGHT * 4U);
    if (live_preview_rgba == NULL) {
        free(pixels);
        set_status(status_line, status_line_size, "Could not allocate the live preview buffer.");
        return false;
    }

    if (!picture_camera_session_start(&camera_session, status_line, status_line_size)) {
        free(live_preview_rgba);
        free(pixels);
        return false;
    }

    if (waiting_for_a_release)
        set_status(status_line, status_line_size, "Release A once before taking the picture.");
    else
        set_status(status_line, status_line_size, "Live camera ready. Press A to capture.");

    while (aptMainLoop()) {
        u32 kDown;

        if (prompt_state == PICTURE_INPUT_PROMPT_CAPTURE &&
            picture_camera_session_receive_frame(&camera_session, pixels, PICTURE_INPUT_PREVIEW_TIMEOUT_NS, false, NULL, 0) &&
            build_live_preview_from_camera(pixels, live_preview_rgba, NULL, 0)) {
            hermes_app_ui_picture_capture_set_preview(live_preview_rgba, PICTURE_INPUT_PREVIEW_WIDTH, PICTURE_INPUT_PREVIEW_HEIGHT);
        }

        if (prompt_state == PICTURE_INPUT_PROMPT_REVIEW)
            hermes_app_ui_render_picture_review(config, status_line, review_waiting_for_a_release);
        else
            hermes_app_ui_render_picture_capture(config, status_line, waiting_for_a_release);

        gspWaitForVBlank();
        hidScanInput();
        kDown = hidKeysDown();

        if ((kDown & KEY_START) != 0) {
            set_status(status_line, status_line_size, "Picture prompt canceled.");
            break;
        }

        if (prompt_state == PICTURE_INPUT_PROMPT_REVIEW) {
            if ((kDown & KEY_B) != 0) {
                set_status(status_line, status_line_size, "Picture prompt canceled.");
                break;
            }

            if (review_waiting_for_a_release) {
                if ((hidKeysHeld() & KEY_A) == 0) {
                    review_waiting_for_a_release = false;
                    if (status_line != NULL && strstr(status_line, "Preview unavailable") != NULL)
                        set_status(status_line, status_line_size, "Picture captured. Preview unavailable. Press A to send or X to retake.");
                    else
                        set_status(status_line, status_line_size, "Picture captured. Press A to send or X to retake.");
                }
                continue;
            }

            if ((kDown & KEY_X) != 0) {
                clear_pending_capture(out_bmp_data, out_bmp_size, out_preview);
                prompt_state = PICTURE_INPUT_PROMPT_CAPTURE;
                waiting_for_a_release = (hidKeysHeld() & KEY_A) != 0;
                if (waiting_for_a_release)
                    set_status(status_line, status_line_size, "Release A once before retaking the picture.");
                else
                    set_status(status_line, status_line_size, "Retake ready. Press A to capture.");
                continue;
            }

            if ((kDown & KEY_A) != 0) {
                set_status(status_line, status_line_size, "Picture captured. Sending to Hermes...");
                success = true;
                break;
            }
            continue;
        }

        if ((kDown & KEY_B) != 0) {
            set_status(status_line, status_line_size, "Picture prompt canceled.");
            break;
        }

        if (waiting_for_a_release) {
            if ((hidKeysHeld() & KEY_A) == 0) {
                waiting_for_a_release = false;
                set_status(status_line, status_line_size, "Live camera ready. Press A to capture.");
            }
            continue;
        }

        if ((kDown & KEY_A) != 0) {
            PictureProcessedFrame processed_frame;
            bool preview_uploaded;

            picture_processed_frame_reset(&processed_frame);
            hermes_app_ui_render_picture_capture(config, "Capturing picture...", false);
            gspWaitForVBlank();
            clear_pending_capture(out_bmp_data, out_bmp_size, out_preview);
            if (!picture_camera_session_receive_frame(&camera_session, pixels, PICTURE_INPUT_TIMEOUT_NS, true, status_line, status_line_size))
                break;
            if (!build_processed_frame_from_camera(pixels, &processed_frame, status_line, status_line_size))
                break;
            if (!build_bmp_from_processed_frame(&processed_frame, out_bmp_data, out_bmp_size, status_line, status_line_size)) {
                picture_processed_frame_free(&processed_frame);
                break;
            }
            if (!build_preview_from_processed_frame(&processed_frame, out_preview, status_line, status_line_size)) {
                picture_processed_frame_free(&processed_frame);
                break;
            }
            picture_processed_frame_free(&processed_frame);

            preview_uploaded = hermes_app_ui_picture_capture_set_preview(out_preview->rgba8_data, out_preview->width, out_preview->height);
            prompt_state = PICTURE_INPUT_PROMPT_REVIEW;
            review_waiting_for_a_release = (hidKeysHeld() & KEY_A) != 0;
            if (review_waiting_for_a_release) {
                set_status(
                    status_line,
                    status_line_size,
                    preview_uploaded
                        ? "Release A, then press A to send or X to retake."
                        : "Release A, then press A to send or X to retake. Preview unavailable."
                );
            } else {
                set_status(
                    status_line,
                    status_line_size,
                    preview_uploaded
                        ? "Picture captured. Press A to send or X to retake."
                        : "Picture captured. Preview unavailable. Press A to send or X to retake."
                );
            }
        }
    }

    if (!success)
        clear_pending_capture(out_bmp_data, out_bmp_size, out_preview);

    picture_camera_session_close(&camera_session);
    free(live_preview_rgba);
    free(pixels);
    hermes_app_ui_picture_capture_clear_preview();
    return success;
}

bool picture_input_decode_bmp_preview(
    const void* bmp_data,
    size_t bmp_size,
    PictureInputPreview* out_preview,
    char* status_line,
    size_t status_line_size
)
{
    const u8* bytes = (const u8*)bmp_data;
    size_t pixel_offset;
    int width;
    int height_signed;
    int height;
    int bits_per_pixel;
    size_t row_stride;
    u8* preview_rgba;
    int y;
    bool top_down;

    if (bmp_data == NULL || out_preview == NULL || bmp_size < 54) {
        set_status(status_line, status_line_size, "Hermes picture preview was invalid.");
        return false;
    }

    picture_input_preview_reset(out_preview);
    if (bytes[0] != 'B' || bytes[1] != 'M') {
        set_status(status_line, status_line_size, "Hermes picture preview was not a BMP file.");
        return false;
    }

    pixel_offset = (size_t)(bytes[10] | (bytes[11] << 8) | (bytes[12] << 16) | (bytes[13] << 24));
    width = (int)(bytes[18] | (bytes[19] << 8) | (bytes[20] << 16) | (bytes[21] << 24));
    height_signed = (int)(bytes[22] | (bytes[23] << 8) | (bytes[24] << 16) | (bytes[25] << 24));
    bits_per_pixel = (int)(bytes[28] | (bytes[29] << 8));
    height = height_signed < 0 ? -height_signed : height_signed;
    top_down = height_signed < 0;

    if (pixel_offset >= bmp_size || width <= 0 || height <= 0 || bits_per_pixel != 24) {
        set_status(status_line, status_line_size, "Hermes picture preview used an unsupported BMP format.");
        return false;
    }

    row_stride = (((size_t)width * 3U) + 3U) & ~3U;
    if (pixel_offset + row_stride * (size_t)height > bmp_size) {
        set_status(status_line, status_line_size, "Hermes picture preview data was truncated.");
        return false;
    }

    preview_rgba = (u8*)malloc(PICTURE_INPUT_PREVIEW_WIDTH * PICTURE_INPUT_PREVIEW_HEIGHT * 4U);
    if (preview_rgba == NULL) {
        set_status(status_line, status_line_size, "Could not allocate the picture preview.");
        return false;
    }

    for (y = 0; y < PICTURE_INPUT_PREVIEW_HEIGHT; y++) {
        int x;
        int source_y = (int)(((size_t)y * (size_t)height) / PICTURE_INPUT_PREVIEW_HEIGHT);
        int stored_row = top_down ? source_y : (height - 1 - source_y);
        const u8* source_row = bytes + pixel_offset + ((size_t)stored_row * row_stride);

        for (x = 0; x < PICTURE_INPUT_PREVIEW_WIDTH; x++) {
            int source_x = (int)(((size_t)x * (size_t)width) / PICTURE_INPUT_PREVIEW_WIDTH);
            const u8* source_pixel = source_row + ((size_t)source_x * 3U);
            u8* dest_pixel = preview_rgba + (((size_t)y * PICTURE_INPUT_PREVIEW_WIDTH + (size_t)x) * 4U);

            dest_pixel[0] = source_pixel[2];
            dest_pixel[1] = source_pixel[1];
            dest_pixel[2] = source_pixel[0];
            dest_pixel[3] = 0xFF;
        }
    }

    out_preview->width = PICTURE_INPUT_PREVIEW_WIDTH;
    out_preview->height = PICTURE_INPUT_PREVIEW_HEIGHT;
    out_preview->rgba8_data = preview_rgba;
    return true;
}
