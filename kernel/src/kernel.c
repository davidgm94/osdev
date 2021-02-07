#include "types.h"

typedef struct Framebuffer
{
    void* base_address;
    usize size;
    u32 width, height;
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

typedef struct Point
{
    u32 x, y;
} Point;

typedef struct Renderer
{
    Framebuffer* fb;
    PSF1Font* font;
    Point cursor_position;
    u32 color;
} Renderer;

static Renderer renderer = {0};

void putchar_ex(Renderer* renderer, char c)
{
    u32* pix_writer = (u32*)renderer->fb->base_address;
    char* font_reader = renderer->font->glyph_buffer + (c * renderer->font->header->char_size);

    for (u64 y = renderer->cursor_position.y; y < renderer->cursor_position.y + 16; y++, font_reader++)
    {
        for (u64 x = renderer->cursor_position.x; x < renderer->cursor_position.x + 8; x++)
        {
            if ((*font_reader & (0b10000000 >> (x - renderer->cursor_position.x))) > 0)
            {
                *(u32*)(pix_writer + x + (y * renderer->fb->pixels_per_scanline)) = renderer->color;
            }
        }
    }
}

void print_ex(Renderer* renderer, char* str)
{
    if (str)
    {
        while (*str)
        {
            putchar_ex(renderer, *str);
            renderer->cursor_position.x += 8;
            if (renderer->cursor_position.x + 8 > renderer->fb->width)
            {
                renderer->cursor_position.x = 0;
                renderer->cursor_position.y += 16;
            }
            str++;
        }
    }
    else
    {
        print_ex(renderer, "(null)");
    }
}

void putc(char c)
{
    putchar_ex(&renderer, c);
}

void print(char* str)
{
    print_ex(&renderer, str);
}

char signed_to_string_output[128];
char unsigned_to_string_output[128];
char float_to_string_output[128];
char hex_to_string_output[128];

char* unsigned_to_string(u64 value)
{
    u8 digit_count;
    u64 it = value;
    
    while (it / 10 > 0)
    {
        it /= 10;
        digit_count++;
    }

    u8 index = 0;

    while (value / 10 > 0)
    {
        u8 remainder = value % 10;
        value /= 10;
        unsigned_to_string_output[digit_count - index] = remainder + '0';
        index++;
    }

    u8 remainder = value % 10;
    unsigned_to_string_output[digit_count - index] = remainder + '0';
    unsigned_to_string_output[digit_count + 1] = 0;

    return unsigned_to_string_output;
}

char* signed_to_string(s64 value)
{
    u8 is_negative = value < 0;
    if (is_negative)
    {
        value *= is_negative ? -1 : 1;
        signed_to_string_output[0] = '-';
    }

    u8 digit_count;
    u64 it = value;
    
    while (it / 10 > 0)
    {
        it /= 10;
        digit_count++;
    }

    u8 index = 0;

    while (value / 10 > 0)
    {
        u8 remainder = value % 10;
        value /= 10;
        signed_to_string_output[is_negative + digit_count - index] = remainder + '0';
        index++;
    }

    u8 remainder = value % 10;
    signed_to_string_output[is_negative + digit_count - index] = remainder + '0';
    signed_to_string_output[is_negative + digit_count + 1] = 0;

    return signed_to_string_output;
}

char* float_to_string(f64 value, u8 decimal_digits)
{
    if (decimal_digits > 20)
    {
        decimal_digits = 20;
    }

    char* int_ptr = (char*)signed_to_string((s64)value);
    char* float_ptr = float_to_string_output;

    if (value < 0)
    {
        value *= -1;
    }


    while (*int_ptr)
    {
        *float_ptr++ = *int_ptr++;
    }

    *float_ptr++ = '.';

    f64 new_value = value - (s64)value;

    for (u8 i = 0; i < decimal_digits; i++)
    {
        new_value *= 10;
        *float_ptr++ = (s64)new_value + '0';
        new_value -= (s64)new_value;
    }
    *float_ptr = 0;

    return float_to_string_output;
}

char* hex_to_string(u64 value)
{
    u8 digits = sizeof(u64) * 2 - 1;
    
    for (u8 i = 0; i < digits; i++)
    {
        u8* ptr = ((u8*)&value + i);

        u8 tmp = ((*ptr & 0xF0) >> 4);
        hex_to_string_output[digits - (i * 2 + 1)] = tmp + (tmp > 9 ? 55 : '0');

        tmp = (*ptr) & 0x0F;
        hex_to_string_output[digits - (i * 2)] = tmp + (tmp > 9 ? 55 : '0');
    }

    hex_to_string_output[digits + 1] = 0;

    return hex_to_string_output;
}

void _start(Framebuffer* fb, PSF1Font* font)
{
    renderer = (const Renderer)
    {
        .fb = fb,
        .font = font,
        .color = 0xffffffff,
        .cursor_position = { .x = 0, .y = 0, },
    };

    print("Number: ");
    print(hex_to_string(0x213afaedb281275));
}
