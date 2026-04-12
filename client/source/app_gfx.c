#include "app_gfx.h"

#include <string.h>

static C3D_RenderTarget* g_top_target = NULL;
static C3D_RenderTarget* g_bottom_target = NULL;
static C2D_TextBuf g_text_buf = NULL;

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

    return g_top_target != NULL && g_bottom_target != NULL && g_text_buf != NULL;
}

void app_gfx_fini(void)
{
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
    C2D_Text draw_text;

    if (g_text_buf == NULL || text == NULL || text[0] == '\0')
        return;

    C2D_TextParse(&draw_text, g_text_buf, text);
    C2D_TextOptimize(&draw_text);
    C2D_DrawText(&draw_text, C2D_WithColor, x, y, 0.5f, scale_x, scale_y, color);
}

void app_gfx_text_right(float right_x, float y, float scale_x, float scale_y, u32 color, const char* text)
{
    C2D_Text draw_text;
    float width = 0.0f;
    float height = 0.0f;

    if (g_text_buf == NULL || text == NULL || text[0] == '\0')
        return;

    C2D_TextParse(&draw_text, g_text_buf, text);
    C2D_TextOptimize(&draw_text);
    C2D_TextGetDimensions(&draw_text, scale_x, scale_y, &width, &height);
    (void)height;
    C2D_DrawText(&draw_text, C2D_WithColor, right_x - width, y, 0.5f, scale_x, scale_y, color);
}
