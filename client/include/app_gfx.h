#pragma once

#include <3ds.h>
#include <stdbool.h>

#include <citro2d.h>

bool app_gfx_init(void);
void app_gfx_fini(void);

void app_gfx_begin_frame(void);
void app_gfx_begin_top(u32 clear_color);
void app_gfx_begin_bottom(u32 clear_color);
void app_gfx_end_frame(void);

void app_gfx_panel(float x, float y, float w, float h, u32 fill, u32 border);
void app_gfx_panel_inset(float x, float y, float w, float h, u32 fill, u32 border, u32 accent);
void app_gfx_highlight_bar(float x, float y, float w, float h, u32 color);
bool app_gfx_measure_text(const char* text, float scale_x, float scale_y, float* out_width, float* out_height);
void app_gfx_text(float x, float y, float scale_x, float scale_y, u32 color, const char* text);
void app_gfx_text_fit(float x, float y, float max_width, float scale_x, float scale_y, u32 color, const char* text);
void app_gfx_text_right(float right_x, float y, float scale_x, float scale_y, u32 color, const char* text);
