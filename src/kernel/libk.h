#pragma once
#include "types.h"
#define assert(x) _assert(x, #x)

s32 vprint(const char* format, va_list va_args);
s32 print(const char* format, ...);
s32 println(const char* format, ...);
bool memequal(const void* a, const void* b, usize bytes);
void new_line();
void memset(void* mem, u8 value, usize bytes);
void* memcpy(void* dst, const void* src, usize bytes);
extern void putc(char c);
u64 string_to_unsigned(const char* str);
bool string_eq(const char* a, const char* b);