#pragma once

#include <3ds.h>
#include <stdbool.h>

/*
 * PictoChat Theme System
 * 
 * Based on Nintendo DS Design System V1.3.3
 * 16 color themes with light/dark variants
 */

#define PICTOCHAT_THEME_COUNT 16

typedef enum PictochatThemeColor {
    PICTOCHAT_THEME_BLUE = 0,
    PICTOCHAT_THEME_BROWN = 1,
    PICTOCHAT_THEME_RED = 2,
    PICTOCHAT_THEME_PINK = 3,
    PICTOCHAT_THEME_ORANGE = 4,
    PICTOCHAT_THEME_YELLOW = 5,
    PICTOCHAT_THEME_LIME = 6,
    PICTOCHAT_THEME_GREEN = 7,
    PICTOCHAT_THEME_FOREST = 8,
    PICTOCHAT_THEME_TEAL = 9,
    PICTOCHAT_THEME_SKY = 10,
    PICTOCHAT_THEME_ROYAL = 11,
    PICTOCHAT_THEME_NAVY = 12,
    PICTOCHAT_THEME_PURPLE = 13,
    PICTOCHAT_THEME_MAGENTA = 14,
    PICTOCHAT_THEME_FUCHSIA = 15,
} PictochatThemeColor;

typedef struct PictochatColorSet {
    u32 primary;        /* Main theme color */
    u32 primary_light;  /* Lighter variant for highlights */
    u32 primary_dark;   /* Darker variant for borders/shadows */
    u32 gradient_top;   /* Top of gradient bar */
    u32 gradient_mid;   /* Middle of gradient bar */
    u32 gradient_bottom;/* Bottom of gradient bar */
} PictochatColorSet;

typedef struct PictochatTheme {
    PictochatColorSet accent;
    u32 background;
    u32 paper;
    u32 paper_alt;
    u32 paper_shadow;
    u32 rule;           /* Horizontal notebook lines */
    u32 rule_strong;    /* Vertical margin line */
    u32 border;
    u32 border_dark;
    u32 text;
    u32 text_muted;
    u32 text_meta;
    u32 alert_bg;
    u32 alert_line;
    u32 alert_border;
} PictochatTheme;

/* Get theme by index (0-15) and dark mode flag */
const PictochatTheme* pictochat_theme_get(PictochatThemeColor color, bool dark_mode);

/* Get theme name for display */
const char* pictochat_theme_get_name(PictochatThemeColor color);

/* Convert color index to enum (with bounds checking) */
PictochatThemeColor pictochat_theme_color_from_index(int index);

/* Get next/previous theme color (for navigation) */
PictochatThemeColor pictochat_theme_next(PictochatThemeColor current);
PictochatThemeColor pictochat_theme_previous(PictochatThemeColor current);

/* Default theme color */
#define PICTOCHAT_THEME_DEFAULT PICTOCHAT_THEME_ORANGE

/* Helper macro to create RGBA color */
#define PCH_COLOR(r, g, b, a) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | ((u32)(a) << 24))

/* Helper macro to create RGB color (opaque) */
#define PCH_COLOR_RGB(r, g, b) PCH_COLOR(r, g, b, 0xFF)

/* Color interpolation for gradients */
u32 pictochat_color_lerp(u32 color1, u32 color2, float t);

/* Get lighter/darker variants of a color */
u32 pictochat_color_lighten(u32 color, float amount);
u32 pictochat_color_darken(u32 color, float amount);
