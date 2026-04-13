#include "app_gfx.h"

#include "nintendo_ds_bios_body_font_bin.h"
#include "nintendo_ds_bios_font_bin.h"

#include <stdio.h>
#include <string.h>

static C3D_RenderTarget* g_top_target = NULL;
static C3D_RenderTarget* g_bottom_target = NULL;
static C2D_TextBuf g_text_buf = NULL;
static C2D_TextBuf g_measure_buf = NULL;
static C2D_Font g_ui_font = NULL;
static C2D_Font g_ui_body_font = NULL;
static float g_text_scale_adjust = 1.0f;
static float g_body_text_scale_adjust = 1.0f;

#define APP_GFX_BODY_FONT_THRESHOLD 0.36f

typedef enum AppGfxFontKind {
    APP_GFX_FONT_DISPLAY = 0,
    APP_GFX_FONT_BODY = 1,
} AppGfxFontKind;

static bool app_gfx_should_use_body_font(float scale_x, float scale_y)
{
    return scale_x <= APP_GFX_BODY_FONT_THRESHOLD && scale_y <= APP_GFX_BODY_FONT_THRESHOLD;
}

static C2D_Font app_gfx_font_for_kind(AppGfxFontKind kind)
{
    if (kind == APP_GFX_FONT_BODY && g_ui_body_font != NULL)
        return g_ui_body_font;
    return g_ui_font;
}

static float app_gfx_scale_adjust_for_kind(AppGfxFontKind kind)
{
    if (kind == APP_GFX_FONT_BODY && g_ui_body_font != NULL)
        return g_body_text_scale_adjust;
    return g_text_scale_adjust;
}

static bool app_gfx_parse_text(C2D_Text* draw_text, C2D_TextBuf buf, const char* text, AppGfxFontKind kind)
{
    C2D_Font font;

    if (draw_text == NULL || buf == NULL || text == NULL)
        return false;

    font = app_gfx_font_for_kind(kind);
    if (font != NULL)
        return C2D_TextFontParse(draw_text, font, buf, text) != NULL;

    return C2D_TextParse(draw_text, buf, text) != NULL;
}

static float app_gfx_scale_text(float scale, AppGfxFontKind kind)
{
    return scale * app_gfx_scale_adjust_for_kind(kind);
}

static void app_gfx_refresh_scale_adjust(void)
{
    FINF_s* font_info;

    g_text_scale_adjust = 1.0f;
    g_body_text_scale_adjust = 1.0f;
    if (g_ui_font == NULL)
        ;
    else {
        font_info = C2D_FontGetInfo(g_ui_font);
        if (font_info != NULL && font_info->lineFeed > 0)
            g_text_scale_adjust = 30.0f / (float)font_info->lineFeed;
    }

    if (g_ui_body_font == NULL)
        return;

    font_info = C2D_FontGetInfo(g_ui_body_font);
    if (font_info != NULL && font_info->lineFeed > 0)
        g_body_text_scale_adjust = 30.0f / (float)font_info->lineFeed;
}

bool app_gfx_measure_text(const char* text, float scale_x, float scale_y, float* out_width, float* out_height)
{
    AppGfxFontKind kind = app_gfx_should_use_body_font(scale_x, scale_y) ? APP_GFX_FONT_BODY : APP_GFX_FONT_DISPLAY;
    C2D_Text draw_text;
    float draw_scale_x = app_gfx_scale_text(scale_x, kind);
    float draw_scale_y = app_gfx_scale_text(scale_y, kind);

    if (g_measure_buf == NULL || text == NULL || text[0] == '\0') {
        if (out_width != NULL)
            *out_width = 0.0f;
        if (out_height != NULL)
            *out_height = 0.0f;
        return false;
    }

    C2D_TextBufClear(g_measure_buf);
    if (!app_gfx_parse_text(&draw_text, g_measure_buf, text, kind))
        return false;
    C2D_TextOptimize(&draw_text);
    C2D_TextGetDimensions(&draw_text, draw_scale_x, draw_scale_y, out_width, out_height);
    return true;
}

static size_t app_gfx_utf8_prefix_boundary(const char* text, size_t max_bytes)
{
    size_t index = 0;
    size_t last_valid = 0;

    while (text != NULL && text[index] != '\0' && index < max_bytes) {
        unsigned char byte = (unsigned char)text[index];
        size_t codepoint_len = 1;
        size_t continuation_index;
        bool valid = true;

        if ((byte & 0x80U) == 0x00U) {
            codepoint_len = 1;
        } else if ((byte & 0xE0U) == 0xC0U) {
            codepoint_len = 2;
        } else if ((byte & 0xF0U) == 0xE0U) {
            codepoint_len = 3;
        } else if ((byte & 0xF8U) == 0xF0U) {
            codepoint_len = 4;
        } else {
            index++;
            last_valid = index;
            continue;
        }

        if (index + codepoint_len > max_bytes)
            break;

        for (continuation_index = 1; continuation_index < codepoint_len; continuation_index++) {
            unsigned char continuation = (unsigned char)text[index + continuation_index];
            if (continuation == '\0' || (continuation & 0xC0U) != 0x80U) {
                valid = false;
                break;
            }
        }

        if (!valid) {
            index++;
            last_valid = index;
            continue;
        }

        index += codepoint_len;
        last_valid = index;
    }

    return last_valid;
}

bool app_gfx_init(void)
{
    gfxInitDefault();
    gfxSet3D(false);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    g_top_target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_bottom_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_text_buf = C2D_TextBufNew(8192);
    g_measure_buf = C2D_TextBufNew(4096);
    g_ui_font = C2D_FontLoadFromMem(nintendo_ds_bios_font_bin, nintendo_ds_bios_font_bin_size);
    g_ui_body_font = C2D_FontLoadFromMem(nintendo_ds_bios_body_font_bin, nintendo_ds_bios_body_font_bin_size);
    if (g_ui_font != NULL)
        C2D_FontSetFilter(g_ui_font, GPU_NEAREST, GPU_LINEAR);
    if (g_ui_body_font != NULL)
        C2D_FontSetFilter(g_ui_body_font, GPU_NEAREST, GPU_LINEAR);
    app_gfx_refresh_scale_adjust();

    return g_top_target != NULL && g_bottom_target != NULL && g_text_buf != NULL && g_measure_buf != NULL;
}

void app_gfx_fini(void)
{
    if (g_ui_font != NULL) {
        C2D_FontFree(g_ui_font);
        g_ui_font = NULL;
    }

    if (g_ui_body_font != NULL) {
        C2D_FontFree(g_ui_body_font);
        g_ui_body_font = NULL;
    }

    if (g_measure_buf != NULL) {
        C2D_TextBufDelete(g_measure_buf);
        g_measure_buf = NULL;
    }

    if (g_text_buf != NULL) {
        C2D_TextBufDelete(g_text_buf);
        g_text_buf = NULL;
    }

    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

void app_gfx_begin_frame(void)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    if (g_text_buf != NULL)
        C2D_TextBufClear(g_text_buf);
}

void app_gfx_begin_top(u32 clear_color)
{
    C2D_TargetClear(g_top_target, clear_color);
    C2D_SceneBegin(g_top_target);
}

void app_gfx_begin_bottom(u32 clear_color)
{
    C2D_TargetClear(g_bottom_target, clear_color);
    C2D_SceneBegin(g_bottom_target);
}

void app_gfx_end_frame(void)
{
    C3D_FrameEnd(0);
}

void app_gfx_panel(float x, float y, float w, float h, u32 fill, u32 border)
{
    C2D_DrawRectSolid(x, y, 0.0f, w, h, border);
    C2D_DrawRectSolid(x + 2.0f, y + 2.0f, 0.0f, w - 4.0f, h - 4.0f, fill);
}

void app_gfx_panel_inset(float x, float y, float w, float h, u32 fill, u32 border, u32 accent)
{
    app_gfx_panel(x, y, w, h, fill, border);
    C2D_DrawRectSolid(x + 4.0f, y + 4.0f, 0.0f, w - 8.0f, 2.0f, accent);
    C2D_DrawRectSolid(x + 4.0f, y + h - 6.0f, 0.0f, w - 8.0f, 2.0f, accent);
}

void app_gfx_highlight_bar(float x, float y, float w, float h, u32 color)
{
    C2D_DrawRectSolid(x, y, 0.0f, w, h, color);
}

void app_gfx_text(float x, float y, float scale_x, float scale_y, u32 color, const char* text)
{
    AppGfxFontKind kind = app_gfx_should_use_body_font(scale_x, scale_y) ? APP_GFX_FONT_BODY : APP_GFX_FONT_DISPLAY;
    C2D_Text draw_text;
    float draw_scale_x = app_gfx_scale_text(scale_x, kind);
    float draw_scale_y = app_gfx_scale_text(scale_y, kind);

    if (g_text_buf == NULL || text == NULL || text[0] == '\0')
        return;

    if (!app_gfx_parse_text(&draw_text, g_text_buf, text, kind))
        return;
    C2D_TextOptimize(&draw_text);
    C2D_DrawText(&draw_text, C2D_WithColor, x, y, 0.5f, draw_scale_x, draw_scale_y, color);
}

void app_gfx_text_fit(float x, float y, float max_width, float scale_x, float scale_y, u32 color, const char* text)
{
    char fitted[128];
    float width = 0.0f;
    float height = 0.0f;
    size_t full_length;
    size_t low;
    size_t high;
    size_t best = 0;

    if (text == NULL || text[0] == '\0' || max_width <= 0.0f) {
        app_gfx_text(x, y, scale_x, scale_y, color, text);
        return;
    }

    if (app_gfx_measure_text(text, scale_x, scale_y, &width, &height) && width <= max_width) {
        app_gfx_text(x, y, scale_x, scale_y, color, text);
        return;
    }

    full_length = strlen(text);
    if (full_length >= sizeof(fitted))
        full_length = sizeof(fitted) - 1;

    low = 0;
    high = full_length;
    while (low <= high) {
        size_t mid = low + (high - low) / 2;

        if (mid <= 3) {
            snprintf(fitted, sizeof(fitted), "...");
        } else {
            size_t keep = app_gfx_utf8_prefix_boundary(text, mid - 3);
            if (keep == 0) {
                snprintf(fitted, sizeof(fitted), "...");
            } else {
                memcpy(fitted, text, keep);
                fitted[keep] = '.';
                fitted[keep + 1] = '.';
                fitted[keep + 2] = '.';
                fitted[keep + 3] = '\0';
            }
        }

        if (app_gfx_measure_text(fitted, scale_x, scale_y, &width, &height) && width <= max_width) {
            best = mid;
            low = mid + 1;
        } else {
            if (mid == 0)
                break;
            high = mid - 1;
        }
    }

    if (best <= 3) {
        snprintf(fitted, sizeof(fitted), "...");
    } else {
        size_t keep = app_gfx_utf8_prefix_boundary(text, best - 3);
        if (keep == 0) {
            snprintf(fitted, sizeof(fitted), "...");
        } else {
            memcpy(fitted, text, keep);
            fitted[keep] = '.';
            fitted[keep + 1] = '.';
            fitted[keep + 2] = '.';
            fitted[keep + 3] = '\0';
        }
    }

    app_gfx_text(x, y, scale_x, scale_y, color, fitted);
}

void app_gfx_text_right(float right_x, float y, float scale_x, float scale_y, u32 color, const char* text)
{
    AppGfxFontKind kind = app_gfx_should_use_body_font(scale_x, scale_y) ? APP_GFX_FONT_BODY : APP_GFX_FONT_DISPLAY;
    C2D_Text draw_text;
    float width = 0.0f;
    float height = 0.0f;
    float draw_scale_x = app_gfx_scale_text(scale_x, kind);
    float draw_scale_y = app_gfx_scale_text(scale_y, kind);

    if (g_text_buf == NULL || text == NULL || text[0] == '\0')
        return;

    if (!app_gfx_parse_text(&draw_text, g_text_buf, text, kind))
        return;
    C2D_TextOptimize(&draw_text);
    C2D_TextGetDimensions(&draw_text, draw_scale_x, draw_scale_y, &width, &height);
    (void)height;
    C2D_DrawText(&draw_text, C2D_WithColor, right_x - width, y, 0.5f, draw_scale_x, draw_scale_y, color);
}
