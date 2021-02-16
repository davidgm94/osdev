#include "mouse.h"
#include "interrupts.h"
#include "renderer_internal.h"

static Point mouse_position;
static Point old_mouse_position;

void PS2_mouse_process_packet(void)
{
    if (!mouse_packet_ready)
    {
        return;
    }

    mouse_packet_ready = false;

    bool x_neg = mouse_packet[0] & PS2XSign;
    bool y_neg = mouse_packet[0] & PS2YSign;
    bool x_overflow = mouse_packet[0] & PS2XOverflow;
    bool y_overflow = mouse_packet[0] & PS2YOverflow;

    if (!x_neg)
    {
        mouse_position.x += mouse_packet[1];
        if (x_overflow)
        {
            mouse_position.x += 255;
        }
    }
    else
    {
        mouse_packet[1] = 256 - mouse_packet[1];
        mouse_position.x -= mouse_packet[1];
        if (x_overflow)
        {
            mouse_position.x -= 255;
        }
    }

    if (!y_neg)
    {
        mouse_position.y -= mouse_packet[2];
        if (y_overflow)
        {
            mouse_position.y -= 255;
        }
    }
    else
    {
        mouse_packet[2] = 256 - mouse_packet[2];
        mouse_position.y += mouse_packet[2];
        if (y_overflow)
        {
            mouse_position.y += 255;
        }
    }

    if (mouse_position.x < 0)
    {
        mouse_position.x = 0;
    }

    if (mouse_position.x > renderer.fb->width - 1)
    {
        mouse_position.x = renderer.fb->width - 1;
    }

    if (mouse_position.y < 0)
    {
        mouse_position.y = 0;
    }

    if (mouse_position.y > renderer.fb->height - 1)
    {
        mouse_position.y = renderer.fb->height - 1;
    }

    clear_mouse_cursor(mouse_pointer, old_mouse_position);
    draw_overlay_mouse_cursor(mouse_pointer, mouse_position, Color_White);

    if (mouse_packet[0] & PS2LeftButton)
    {
    }

    if (mouse_packet[0] & PS2MiddleButton)
    {

    }

    if (mouse_packet[0] & PS2RightButton)
    {
    }

    mouse_packet_ready = false;
    old_mouse_position = mouse_position;
}