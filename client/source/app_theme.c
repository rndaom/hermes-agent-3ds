#include "app_theme.h"

#include <string.h>

/*
 * Nintendo DS Design System Color Palette
 * Based on Theme Select asset RGB values
 */

/* Base accent colors from Theme Select asset */
#define BASE_BLUE    0x63849C
#define BASE_BROWN   0xBA4900
#define BASE_RED     0xFF0018
#define BASE_PINK    0xFF8CFF
#define BASE_ORANGE  0xFF9400
#define BASE_YELLOW  0xF7E700
#define BASE_LIME    0xADFF00
#define BASE_GREEN   0x00FF00
#define BASE_FOREST  0x00A539
#define BASE_TEAL    0x4ADE8C
#define BASE_SKY     0x31BDF7
#define BASE_ROYAL   0x005AF7
#define BASE_NAVY    0x000094
#define BASE_PURPLE  0x8C00D6
#define BASE_MAGENTA 0xD600EF
#define BASE_FUCHSIA 0xFF0094

/* Helper to extract RGB components */
#define R_FROM_HEX(c) (((c) >> 16) & 0xFF)
#define G_FROM_HEX(c) (((c) >> 8) & 0xFF)
#define B_FROM_HEX(c) ((c) & 0xFF)

/* Light mode backgrounds (paper colors) */
#define LIGHT_BG        0xEFEFEF  /* Warm off-white background */
#define LIGHT_PAPER     0xFBFBF6  /* Light paper */
#define LIGHT_PAPER_ALT 0xF3F3ED  /* Alternate paper */
#define LIGHT_SHADOW    0xE2E2DA  /* Paper shadow */
#define LIGHT_RULE      0xD4D4CC  /* Light gray notebook rules */
#define LIGHT_RULE_STR  0xC2C2BA  /* Stronger rule for margins */
#define LIGHT_BORDER    0x90918B  /* Medium gray borders */
#define LIGHT_BORDER_DK 0x626360  /* Dark borders */
#define LIGHT_TEXT      0x282A2E  /* Dark charcoal text */
#define LIGHT_MUTED     0x55575E  /* Muted text */
#define LIGHT_META      0x777980  /* Meta/secondary text */

/* Dark mode backgrounds */
#define DARK_BG         0x1A1A1A  /* Dark background */
#define DARK_PAPER      0x2A2A2A  /* Dark paper */
#define DARK_PAPER_ALT  0x323232  /* Alternate dark paper */
#define DARK_SHADOW     0x0A0A0A  /* Dark shadow */
#define DARK_RULE       0x3A3A3A  /* Dark mode rules */
#define DARK_RULE_STR   0x4A4A4A  /* Strong dark rule */
#define DARK_BORDER     0x5A5A5A  /* Dark borders */
#define DARK_BORDER_LT  0x7A7A7A  /* Lighter dark border */
#define DARK_TEXT       0xE8E8E8  /* Light text for dark mode */
#define DARK_MUTED      0xA0A0A0  /* Muted dark text */
#define DARK_META       0x808080  /* Meta dark text */

/* Alert colors */
#define ALERT_BG_DARK   0x1B1B1D
#define ALERT_LINE      0x3A3A3D
#define ALERT_BORDER    0xFF8C1C

/* Static theme storage */
static PictochatTheme g_themes[PICTOCHAT_THEME_COUNT][2]; /* [color][dark_mode] */
static bool g_themes_initialized = false;

static u32 hex_to_rgba(u32 hex)
{
    return PCH_COLOR_RGB(
        (hex >> 16) & 0xFF,
        (hex >> 8) & 0xFF,
        hex & 0xFF
    );
}

static u32 create_gradient_color(u32 base, float factor)
{
    int r = (int)(((base >> 16) & 0xFF) * factor);
    int g = (int)(((base >> 8) & 0xFF) * factor);
    int b = (int)((base & 0xFF) * factor);
    
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    
    return PCH_COLOR_RGB(r, g, b);
}

static void init_theme(PictochatThemeColor color, u32 base_hex)
{
    PictochatTheme* light = &g_themes[color][0];
    PictochatTheme* dark = &g_themes[color][1];
    
    /* Light mode theme */
    light->accent.primary = hex_to_rgba(base_hex);
    light->accent.primary_light = create_gradient_color(base_hex, 1.3f);
    light->accent.primary_dark = create_gradient_color(base_hex, 0.7f);
    light->accent.gradient_top = create_gradient_color(base_hex, 1.15f);
    light->accent.gradient_mid = hex_to_rgba(base_hex);
    light->accent.gradient_bottom = create_gradient_color(base_hex, 0.85f);
    
    light->background = hex_to_rgba(LIGHT_BG);
    light->paper = hex_to_rgba(LIGHT_PAPER);
    light->paper_alt = hex_to_rgba(LIGHT_PAPER_ALT);
    light->paper_shadow = hex_to_rgba(LIGHT_SHADOW);
    light->rule = hex_to_rgba(LIGHT_RULE);
    light->rule_strong = hex_to_rgba(LIGHT_RULE_STR);
    light->border = hex_to_rgba(LIGHT_BORDER);
    light->border_dark = hex_to_rgba(LIGHT_BORDER_DK);
    light->text = hex_to_rgba(LIGHT_TEXT);
    light->text_muted = hex_to_rgba(LIGHT_MUTED);
    light->text_meta = hex_to_rgba(LIGHT_META);
    light->alert_bg = hex_to_rgba(ALERT_BG_DARK);
    light->alert_line = hex_to_rgba(ALERT_LINE);
    light->alert_border = hex_to_rgba(ALERT_BORDER);
    
    /* Dark mode theme */
    dark->accent.primary = hex_to_rgba(base_hex);
    dark->accent.primary_light = create_gradient_color(base_hex, 1.2f);
    dark->accent.primary_dark = create_gradient_color(base_hex, 0.6f);
    dark->accent.gradient_top = create_gradient_color(base_hex, 1.1f);
    dark->accent.gradient_mid = hex_to_rgba(base_hex);
    dark->accent.gradient_bottom = create_gradient_color(base_hex, 0.75f);
    
    dark->background = hex_to_rgba(DARK_BG);
    dark->paper = hex_to_rgba(DARK_PAPER);
    dark->paper_alt = hex_to_rgba(DARK_PAPER_ALT);
    dark->paper_shadow = hex_to_rgba(DARK_SHADOW);
    dark->rule = hex_to_rgba(DARK_RULE);
    dark->rule_strong = hex_to_rgba(DARK_RULE_STR);
    dark->border = hex_to_rgba(DARK_BORDER);
    dark->border_dark = hex_to_rgba(DARK_BORDER_LT);
    dark->text = hex_to_rgba(DARK_TEXT);
    dark->text_muted = hex_to_rgba(DARK_MUTED);
    dark->text_meta = hex_to_rgba(DARK_META);
    dark->alert_bg = hex_to_rgba(ALERT_BG_DARK);
    dark->alert_line = hex_to_rgba(ALERT_LINE);
    dark->alert_border = hex_to_rgba(ALERT_BORDER);
}

static void ensure_initialized(void)
{
    if (g_themes_initialized)
        return;
    
    init_theme(PICTOCHAT_THEME_BLUE, BASE_BLUE);
    init_theme(PICTOCHAT_THEME_BROWN, BASE_BROWN);
    init_theme(PICTOCHAT_THEME_RED, BASE_RED);
    init_theme(PICTOCHAT_THEME_PINK, BASE_PINK);
    init_theme(PICTOCHAT_THEME_ORANGE, BASE_ORANGE);
    init_theme(PICTOCHAT_THEME_YELLOW, BASE_YELLOW);
    init_theme(PICTOCHAT_THEME_LIME, BASE_LIME);
    init_theme(PICTOCHAT_THEME_GREEN, BASE_GREEN);
    init_theme(PICTOCHAT_THEME_FOREST, BASE_FOREST);
    init_theme(PICTOCHAT_THEME_TEAL, BASE_TEAL);
    init_theme(PICTOCHAT_THEME_SKY, BASE_SKY);
    init_theme(PICTOCHAT_THEME_ROYAL, BASE_ROYAL);
    init_theme(PICTOCHAT_THEME_NAVY, BASE_NAVY);
    init_theme(PICTOCHAT_THEME_PURPLE, BASE_PURPLE);
    init_theme(PICTOCHAT_THEME_MAGENTA, BASE_MAGENTA);
    init_theme(PICTOCHAT_THEME_FUCHSIA, BASE_FUCHSIA);
    
    g_themes_initialized = true;
}

const PictochatTheme* pictochat_theme_get(PictochatThemeColor color, bool dark_mode)
{
    ensure_initialized();
    
    if (color < 0 || color >= PICTOCHAT_THEME_COUNT)
        color = PICTOCHAT_THEME_DEFAULT;
    
    return &g_themes[color][dark_mode ? 1 : 0];
}

const char* pictochat_theme_get_name(PictochatThemeColor color)
{
    switch (color) {
        case PICTOCHAT_THEME_BLUE: return "Blue";
        case PICTOCHAT_THEME_BROWN: return "Brown";
        case PICTOCHAT_THEME_RED: return "Red";
        case PICTOCHAT_THEME_PINK: return "Pink";
        case PICTOCHAT_THEME_ORANGE: return "Orange";
        case PICTOCHAT_THEME_YELLOW: return "Yellow";
        case PICTOCHAT_THEME_LIME: return "Lime";
        case PICTOCHAT_THEME_GREEN: return "Green";
        case PICTOCHAT_THEME_FOREST: return "Forest";
        case PICTOCHAT_THEME_TEAL: return "Teal";
        case PICTOCHAT_THEME_SKY: return "Sky";
        case PICTOCHAT_THEME_ROYAL: return "Royal";
        case PICTOCHAT_THEME_NAVY: return "Navy";
        case PICTOCHAT_THEME_PURPLE: return "Purple";
        case PICTOCHAT_THEME_MAGENTA: return "Magenta";
        case PICTOCHAT_THEME_FUCHSIA: return "Fuchsia";
        default: return "Blue";
    }
}

PictochatThemeColor pictochat_theme_color_from_index(int index)
{
    if (index < 0 || index >= PICTOCHAT_THEME_COUNT)
        return PICTOCHAT_THEME_DEFAULT;
    return (PictochatThemeColor)index;
}

PictochatThemeColor pictochat_theme_next(PictochatThemeColor current)
{
    int next = (int)current + 1;
    if (next >= PICTOCHAT_THEME_COUNT)
        next = 0;
    return (PictochatThemeColor)next;
}

PictochatThemeColor pictochat_theme_previous(PictochatThemeColor current)
{
    int prev = (int)current - 1;
    if (prev < 0)
        prev = PICTOCHAT_THEME_COUNT - 1;
    return (PictochatThemeColor)prev;
}

u32 pictochat_color_lerp(u32 color1, u32 color2, float t)
{
    int r1 = (color1 >> 0) & 0xFF;
    int g1 = (color1 >> 8) & 0xFF;
    int b1 = (color1 >> 16) & 0xFF;
    int a1 = (color1 >> 24) & 0xFF;
    
    int r2 = (color2 >> 0) & 0xFF;
    int g2 = (color2 >> 8) & 0xFF;
    int b2 = (color2 >> 16) & 0xFF;
    int a2 = (color2 >> 24) & 0xFF;
    
    int r = (int)(r1 + (r2 - r1) * t);
    int g = (int)(g1 + (g2 - g1) * t);
    int b = (int)(b1 + (b2 - b1) * t);
    int a = (int)(a1 + (a2 - a1) * t);
    
    return PCH_COLOR(r, g, b, a);
}

u32 pictochat_color_lighten(u32 color, float amount)
{
    int r = (int)(((color >> 0) & 0xFF) * (1.0f + amount));
    int g = (int)(((color >> 8) & 0xFF) * (1.0f + amount));
    int b = (int)(((color >> 16) & 0xFF) * (1.0f + amount));
    int a = (color >> 24) & 0xFF;
    
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    
    return PCH_COLOR(r, g, b, a);
}

u32 pictochat_color_darken(u32 color, float amount)
{
    float factor = 1.0f - amount;
    if (factor < 0) factor = 0;
    
    int r = (int)(((color >> 0) & 0xFF) * factor);
    int g = (int)(((color >> 8) & 0xFF) * factor);
    int b = (int)(((color >> 16) & 0xFF) * factor);
    int a = (color >> 24) & 0xFF;
    
    return PCH_COLOR(r, g, b, a);
}
