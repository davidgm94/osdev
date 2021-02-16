#pragma once
#include "types.h"

typedef struct Point
{
    s64 x, y;
} Point;
typedef struct Framebuffer
{
    void* base_address;
    usize size; u32 width, height;
    u32 pixels_per_scanline;
} Framebuffer;

typedef struct PSF1Header
{
    u8 magic[2];
    u8 mode;
    u8 char_size;
} PSF1Header;

typedef struct PSF1Font
{
    PSF1Header* header;
    void* glyph_buffer;
} PSF1Font;

void fb_clear(void);