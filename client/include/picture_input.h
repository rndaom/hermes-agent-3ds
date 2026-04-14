#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#include "app_config.h"

#define PICTURE_INPUT_CAPTURE_WIDTH 400
#define PICTURE_INPUT_CAPTURE_HEIGHT 240
#define PICTURE_INPUT_PREVIEW_WIDTH 256
#define PICTURE_INPUT_PREVIEW_HEIGHT 128

typedef struct PictureInputPreview {
    u16 width;
    u16 height;
    u8* rgba8_data;
} PictureInputPreview;

void picture_input_preview_reset(PictureInputPreview* preview);
void picture_input_preview_free(PictureInputPreview* preview);

bool picture_input_capture_prompt(
    const HermesAppConfig* config,
    u8** out_bmp_data,
    size_t* out_bmp_size,
    PictureInputPreview* out_preview,
    char* status_line,
    size_t status_line_size
);

bool picture_input_decode_bmp_preview(
    const void* bmp_data,
    size_t bmp_size,
    PictureInputPreview* out_preview,
    char* status_line,
    size_t status_line_size
);
