#pragma once
#include "types.h"
#include "renderer.h"

// @TODO: first bit can cause problems, check out
typedef enum Color
{
    Color_Black     = 0xff000000,
    Color_Red       = 0x00ff0000,
    Color_Green     = 0x0000ff00,
    Color_Blue      = 0x000000ff,
    Color_White     = Color_Blue | Color_Green | Color_Red,
} Color;

typedef struct Renderer
{
    Framebuffer* fb;
    PSF1Font* font;
    Point cursor_position;
    Color color;
    Color clear_color;
} Renderer;

extern Renderer renderer;
extern u8 mouse_pointer[];

void clear_mouse_cursor(u8* mouse_cursor, Point position);
void draw_overlay_mouse_cursor(u8* mouse_cursor, Point position, Color color);
void clear_char(void);