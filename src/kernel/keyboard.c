#include "keyboard.h"
#include "interrupts.h"
#include "libk.h"

bool left_shift_pressed = false;
bool right_shift_pressed = false;

extern void kb_backspace_action(void);
extern void kb_print_ch(u8 scancode);

void kb_input_process(void)
{
    KeyboardBuffer kb_buffer;
    u16 kb_event_count;
    // This cleans kb interrupt buffer
    bool overflow = kb_get_buffer(&kb_buffer, &kb_event_count);
    if (overflow)
    {
        println("KEYBOARD BUFFER OVERFLOW");
        while(1);
    }

    if (!allow_keyboard_input)
    {
        return;
    }

    for (u16 i = 0; i < kb_event_count; i++)
    {
        u8 scancode = kb_buffer.messages[i];
        switch (scancode)
        {
            case Key_LeftShift:
                left_shift_pressed = true;
                break;
            case Key_LeftShift + 0x80:
                left_shift_pressed = false;
                break;
            case Key_RightShift:
                right_shift_pressed = true;
                break;
            case Key_RightShift + 0x80:
                right_shift_pressed = false;
                break;
            case Key_Enter:
                new_line();
                allow_keyboard_input = false;
                return;
            case Key_Backspace:
                kb_backspace_action();
                break;
            default:
            {
                kb_print_ch(scancode);
            }
        }
    }
}
