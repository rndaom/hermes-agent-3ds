#include "picture_input.h"

#include "app_ui.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PICTURE_INPUT_TIMEOUT_NS 300000000ULL

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

static void camera_rgb565_to_rgba8(u16 pixel, u8* out_rgba)
{
    out_rgba[0] = (u8)((pixel & 0x1F) << 3);
    out_rgba[1] = (u8)(((pixel >> 5) & 0x3F) << 2);
    out_rgba[2] = (u8)(((pixel >> 11) & 0x1F) << 3);
    out_rgba[3] = 0xFF;
}

static void camera_rgb565_to_bgr24(u16 pixel, u8* out_bgr)
{
    out_bgr[0] = (u8)(((pixel >> 11) & 0x1F) << 3);
    out_bgr[1] = (u8)(((pixel >> 5) & 0x3F) << 2);
    out_bgr[2] = (u8)((pixel & 0x1F) << 3);
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

static bool build_bmp_from_camera(const u16* pixels, u8** out_bmp_data, size_t* out_bmp_size, char* status_line, size_t status_line_size)
{
    size_t row_stride;
    size_t pixel_bytes;
    size_t file_size;
    u8* bmp_data;
    u8* cursor;
    int y;

    if (pixels == NULL || out_bmp_data == NULL || out_bmp_size == NULL) {
        set_status(status_line, status_line_size, "Camera image buffer was invalid.");
        return false;
    }

    row_stride = ((PICTURE_INPUT_CAPTURE_WIDTH * 3U) + 3U) & ~3U;
    pixel_bytes = row_stride * PICTURE_INPUT_CAPTURE_HEIGHT;
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
    write_le32(bmp_data + 18, PICTURE_INPUT_CAPTURE_WIDTH);
    write_le32(bmp_data + 22, PICTURE_INPUT_CAPTURE_HEIGHT);
    write_le16(bmp_data + 26, 1);
    write_le16(bmp_data + 28, 24);
    write_le32(bmp_data + 34, (u32)pixel_bytes);
    write_le32(bmp_data + 38, 2835);
    write_le32(bmp_data + 42, 2835);

    cursor = bmp_data + 54;
    for (y = 0; y < PICTURE_INPUT_CAPTURE_HEIGHT; y++) {
        int x;

        for (x = 0; x < PICTURE_INPUT_CAPTURE_WIDTH; x++) {
            camera_rgb565_to_bgr24(pixels[(size_t)y * PICTURE_INPUT_CAPTURE_WIDTH + (size_t)x], cursor + (size_t)x * 3U);
        }
        cursor += row_stride;
    }

    *out_bmp_data = bmp_data;
    *out_bmp_size = file_size;
    return true;
}

static bool build_preview_from_camera(const u16* pixels, PictureInputPreview* out_preview, char* status_line, size_t status_line_size)
{
    u8* preview_rgba;
    int y;

    if (pixels == NULL || out_preview == NULL) {
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
        size_t source_y = (size_t)(PICTURE_INPUT_CAPTURE_HEIGHT - 1) - (((size_t)y * PICTURE_INPUT_CAPTURE_HEIGHT) / PICTURE_INPUT_PREVIEW_HEIGHT);

        for (x = 0; x < PICTURE_INPUT_PREVIEW_WIDTH; x++) {
            size_t source_x = ((size_t)x * PICTURE_INPUT_CAPTURE_WIDTH) / PICTURE_INPUT_PREVIEW_WIDTH;
            camera_rgb565_to_rgba8(
                pixels[source_y * PICTURE_INPUT_CAPTURE_WIDTH + source_x],
                preview_rgba + (((size_t)y * PICTURE_INPUT_PREVIEW_WIDTH + (size_t)x) * 4U)
            );
        }
    }

    out_preview->width = PICTURE_INPUT_PREVIEW_WIDTH;
    out_preview->height = PICTURE_INPUT_PREVIEW_HEIGHT;
    out_preview->rgba8_data = preview_rgba;
    return true;
}

static bool capture_camera_frame(u16* out_pixels, char* status_line, size_t status_line_size)
{
    Result rc;
    bool cam_ready = false;
    bool capture_started = false;
    bool camera_active = false;
    u32 transfer_bytes = 0;
    Handle receive_event = 0;

    if (out_pixels == NULL) {
        set_status(status_line, status_line_size, "Camera image buffer was invalid.");
        return false;
    }

    rc = camInit();
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "camInit", rc);
        return false;
    }
    cam_ready = true;

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
    rc = CAMU_SetTrimming(PORT_CAM1, false);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetTrimming", rc);
        goto cleanup;
    }

    rc = CAMU_GetMaxBytes(&transfer_bytes, PICTURE_INPUT_CAPTURE_WIDTH, PICTURE_INPUT_CAPTURE_HEIGHT);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_GetMaxBytes", rc);
        goto cleanup;
    }
    rc = CAMU_SetTransferBytes(PORT_CAM1, transfer_bytes, PICTURE_INPUT_CAPTURE_WIDTH, PICTURE_INPUT_CAPTURE_HEIGHT);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetTransferBytes", rc);
        goto cleanup;
    }
    rc = CAMU_Activate(SELECT_OUT1);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_Activate", rc);
        goto cleanup;
    }
    camera_active = true;

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
    capture_started = true;

    rc = CAMU_SetReceiving(
        &receive_event,
        out_pixels,
        PORT_CAM1,
        PICTURE_INPUT_CAPTURE_WIDTH * PICTURE_INPUT_CAPTURE_HEIGHT * 2U,
        (s16)transfer_bytes
    );
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "CAMU_SetReceiving", rc);
        goto cleanup;
    }

    rc = svcWaitSynchronization(receive_event, PICTURE_INPUT_TIMEOUT_NS);
    if (R_FAILED(rc)) {
        picture_status_from_rc(status_line, status_line_size, "Camera capture", rc);
        goto cleanup;
    }

    CAMU_PlayShutterSound(SHUTTER_SOUND_TYPE_NORMAL);
    set_status(status_line, status_line_size, "Picture captured. Sending to Hermes...");

cleanup:
    if (receive_event != 0)
        svcCloseHandle(receive_event);
    if (capture_started)
        CAMU_StopCapture(PORT_CAM1);
    if (camera_active)
        CAMU_Activate(SELECT_NONE);
    if (cam_ready)
        camExit();
    return R_SUCCEEDED(rc);
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
    bool waiting_for_a_release = (hidKeysHeld() & KEY_A) != 0;
    bool last_waiting_for_a_release;
    bool canceled = false;
    bool success = false;

    if (out_bmp_data == NULL || out_bmp_size == NULL || out_preview == NULL) {
        set_status(status_line, status_line_size, "Picture input is not available.");
        return false;
    }

    *out_bmp_data = NULL;
    *out_bmp_size = 0;
    picture_input_preview_reset(out_preview);

    pixels = (u16*)malloc(PICTURE_INPUT_CAPTURE_WIDTH * PICTURE_INPUT_CAPTURE_HEIGHT * sizeof(u16));
    if (pixels == NULL) {
        set_status(status_line, status_line_size, "Could not allocate the camera buffer.");
        return false;
    }

    set_status(status_line, status_line_size, "Picture prompt ready.");
    last_waiting_for_a_release = waiting_for_a_release;

    while (aptMainLoop()) {
        u32 kDown;

        if (waiting_for_a_release != last_waiting_for_a_release) {
            hermes_app_ui_render_picture_capture(config, status_line, waiting_for_a_release);
            last_waiting_for_a_release = waiting_for_a_release;
        } else {
            hermes_app_ui_render_picture_capture(config, status_line, waiting_for_a_release);
        }

        gspWaitForVBlank();
        hidScanInput();
        kDown = hidKeysDown();

        if ((kDown & KEY_START) != 0) {
            canceled = true;
            set_status(status_line, status_line_size, "Picture prompt canceled.");
            break;
        }
        if ((kDown & KEY_B) != 0) {
            canceled = true;
            set_status(status_line, status_line_size, "Picture prompt canceled.");
            break;
        }

        if (waiting_for_a_release) {
            if ((hidKeysHeld() & KEY_A) == 0) {
                waiting_for_a_release = false;
                set_status(status_line, status_line_size, "Picture prompt ready.");
            }
            continue;
        }

        if ((kDown & KEY_A) != 0) {
            hermes_app_ui_render_picture_capture(config, "Capturing picture...", false);
            gspWaitForVBlank();
            if (!capture_camera_frame(pixels, status_line, status_line_size))
                break;
            if (!build_bmp_from_camera(pixels, out_bmp_data, out_bmp_size, status_line, status_line_size))
                break;
            if (!build_preview_from_camera(pixels, out_preview, status_line, status_line_size))
                break;
            success = true;
            break;
        }
    }

    if (!success) {
        if (!canceled && *out_bmp_data != NULL) {
            free(*out_bmp_data);
            *out_bmp_data = NULL;
            *out_bmp_size = 0;
        }
        if (!canceled)
            picture_input_preview_free(out_preview);
    }

    free(pixels);
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
