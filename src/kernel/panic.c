#include "panic.h"
#include "renderer_internal.h"
#include "libk.h"

void panic(const char* format, ...)
{
    renderer.clear_color = Color_Red;
    fb_clear();
    renderer.cursor_position = (Point){0};
    renderer.color = Color_Black;
    println("Kernel panic");
    new_line();
    va_list list;
    va_start(list, format);
    (void)vprint(format, list);
    va_end(list);
    new_line();
}