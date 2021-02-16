#include "renderer_internal.h"
#include "libk.h"

Renderer renderer = {0};

u8 mouse_pointer[] =
{
    0b11111111, 0b11100000,
    0b11111111, 0b10000000,
    0b11111110, 0b00000000,
    0b11111100, 0b00000000,
    0b11111000, 0b00000000,
    0b11110000, 0b00000000,
    0b11100000, 0b00000000,
    0b11000000, 0b00000000,
    0b11000000, 0b00000000,
    0b10000000, 0b00000000,
    0b10000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
};

static u32 mouse_cursor_buffer[array_length(mouse_pointer) * array_length(mouse_pointer)];
static u32 mouse_cursor_buffer_post_render[array_length(mouse_pointer) * array_length(mouse_pointer)];
static bool mouse_never_drawn = true;

void scroll(Renderer* renderer)
{
    Framebuffer* fb = renderer->fb;

    u64 fb_base = (u64)fb->base_address;
    u64 bytes_per_scanline = fb->pixels_per_scanline * sizeof(Color);
    u64 fb_height = fb->height;
    u64 fb_size = fb->size;

    u8 char_size = renderer->font->header->char_size;
    u64 lines_to_be_skipped = char_size;
    u64 lines_to_be_copied = fb_height - lines_to_be_skipped;
    memcpy((void*)fb_base, (void*)(fb_base + (bytes_per_scanline * lines_to_be_skipped)), bytes_per_scanline * lines_to_be_copied);

    u32* line_clear_it = (u32*)(fb_base + (bytes_per_scanline * lines_to_be_copied));
    u32* top = (u32*)(fb_base + (bytes_per_scanline * fb_height));
    Color clear_color = renderer->clear_color;

    while (line_clear_it < top)
    {
        *line_clear_it++ = clear_color;
    }

    // Don't advance line, we are scrolling
    renderer->cursor_position.x = 0;
}

void new_line_ex(Renderer* renderer)
{
    u8 char_size = renderer->font->header->char_size;
    usize fb_height = renderer->fb->height;
    u32 space_to_obviate = char_size + (fb_height % char_size);
    u32 line_limit = fb_height - space_to_obviate;

    if (renderer->cursor_position.y + char_size < line_limit)
    {
        renderer->cursor_position = (Point) { .x = 0, .y = renderer->cursor_position.y + char_size };
    }
    else
    {
        scroll(renderer);
    }
}

void new_line(void)
{
    new_line_ex(&renderer);
}

void handle_newline_while_printing(Renderer* renderer)
{
    renderer->cursor_position.x += 8;
    if (renderer->cursor_position.x + 8 > renderer->fb->width)
    {
        new_line_ex(renderer);
    }
}

void render_char(Renderer* renderer, char c, u32 xo, u32 yo)
{
    u32* pix_writer = (u32*)renderer->fb->base_address;
    char* font_reader = renderer->font->glyph_buffer + (c * renderer->font->header->char_size);

    for (u64 y = yo; y < yo + 16; y++, font_reader++)
    {
        for (u64 x = xo; x < xo + 8; x++)
        {
            if ((*font_reader & (0b10000000 >> (x - xo))) > 0)
            {
                *(u32*)(pix_writer + x + (y * renderer->fb->pixels_per_scanline)) = renderer->color;
            }
        }
    }
}

void putc(char c)
{
    render_char(&renderer, c, renderer.cursor_position.x, renderer.cursor_position.y);
    handle_newline_while_printing(&renderer);
}

void put_pixel(s64 x, s64 y, Color color)
{
    *(u32*)( (u64)renderer.fb->base_address + (x*4) + (y * renderer.fb->pixels_per_scanline * 4)) = color;
}

u32 get_pixel(s64 x, s64 y)
{
    return *(u32*)( (u64)renderer.fb->base_address + (x*4) + (y * renderer.fb->pixels_per_scanline * 4));
}

void put_char_in_point(char c, u32 xo, u32 yo)
{
    render_char(&renderer, c, xo, yo);
}

void fb_clear(void)
{
    Framebuffer* fb = renderer.fb;
    u64 fb_base = (u64)fb->base_address;
    u64 bytes_per_scanline = fb->pixels_per_scanline * sizeof(Color);
    u64 fb_height = fb->height;
    u64 fb_size = fb->size;

    for (u64 vertical_scanline = 0; vertical_scanline < fb_height; vertical_scanline++)
    {
        u64 pix_base_ptr = fb_base + (bytes_per_scanline * vertical_scanline);
        u32* top_ptr = (u32*)(pix_base_ptr + bytes_per_scanline);

        for (u32* pix_ptr = (u32*)pix_base_ptr; pix_ptr < top_ptr; pix_ptr++)
        {
            *pix_ptr = renderer.clear_color;
        }
    }
}


void clear_char(void)
{
    if (renderer.cursor_position.x == 0)
    {
        renderer.cursor_position.x = renderer.fb->width;
        renderer.cursor_position.y -= 16;
        if (renderer.cursor_position.y < 0)
        {
            renderer.cursor_position.y = 0;
        }
    }

    u32* pix_writer = (u32*)renderer.fb->base_address;

    for (u64 y = renderer.cursor_position.y; y < renderer.cursor_position.y + 16; y++)
    {
        for (u64 x = renderer.cursor_position.x - 8; x < renderer.cursor_position.x; x++)
        {
            *(u32*)(pix_writer + x + (y * renderer.fb->pixels_per_scanline)) = renderer.clear_color;
        }
    }
    renderer.cursor_position.x -= 8;

    if (renderer.cursor_position.x < 0)
    {
        renderer.cursor_position.x = renderer.fb->width;
        renderer.cursor_position.y -= 16;
        if (renderer.cursor_position.y < 0)
        {
            renderer.cursor_position.y = 0;
        }
    }
}

void clear_mouse_cursor(u8* mouse_cursor, Point position)
{
    if (mouse_never_drawn)
    {
        return;
    }

    s64 x_max = 16;
    s64 y_max = 16;
    s64 difference_x = renderer.fb->width - position.x;
    s64 difference_y = renderer.fb->height - position.y;

    if (difference_x < 16)
    {
        x_max = difference_x;
    }

    if (difference_y < 16)
    {
        y_max = difference_y;
    }

    for (s64 y = 0; y < y_max; y++)
    {
        for (s64 x = 0; x < x_max; x++)
        {
            s64 bit = y * 16 + x;
            s64 byte = bit / 8;
            if ((mouse_cursor[byte] & (0b10000000 >> (x % 8))) && (get_pixel(position.x + x, position.y + y) == mouse_cursor_buffer_post_render[x + y * 16]))
            {
                put_pixel(position.x + x, position.y + y, mouse_cursor_buffer[x + y * 16]);
            }
        }
    }
}

void draw_overlay_mouse_cursor(u8* mouse_cursor, Point position, Color color)
{
    s64 x_max = 16;
    s64 y_max = 16;
    s64 difference_x = renderer.fb->width - position.x;
    s64 difference_y = renderer.fb->height - position.y;

    if (difference_x < 16)
    {
        x_max = difference_x;
    }

    if (difference_y < 16)
    {
        y_max = difference_y;
    }

    for (s64 y = 0; y < y_max; y++)
    {
        for (s64 x = 0; x < x_max; x++)
        {
            s64 bit = y * 16 + x;
            s64 byte = bit / 8;
            if (mouse_cursor[byte] & (0b10000000 >> (x % 8)))
            {
                mouse_cursor_buffer[x + y * 16] = get_pixel(position.x + x, position.y + y);
                put_pixel(position.x + x, position.y + y, color);
                mouse_cursor_buffer_post_render[x + y * 16] = get_pixel(position.x + x, position.y + y);
            }
        }
    }

    mouse_never_drawn = false;
}
