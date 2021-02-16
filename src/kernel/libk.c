#include "libk.h"

typedef enum FormatLookupTableIndex
{
    BINARY = 0,
    DECIMAL = 1,
    HEXADECIMAL = 2,
} FormatLookupTableIndex;

bool memequal(const void* a, const void* b, usize bytes)
{
    u8* a1 = (u8*)a;
    u8* b1 = (u8*)b;
    for (usize i = 0; i < bytes; i++)
    {
        if (a1[i] != b1[i])
        {
            return false;
        }
    }
    return true;
}

void* memcpy(void* dst, const void* src, usize bytes)
{
    u8* writer = dst;
    u8* reader = (u8*)src;
    for (u64 i = 0; i < bytes; i++)
    {
        *writer++ = *reader++;
    }

    return dst;
}

void unsigned_to_string_vprintf(u64 value, char* buffer)
{
    u8 digit_count = 0;
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
        buffer[digit_count - index] = remainder + '0';
        index++;
    }

    u8 remainder = value % 10;
    buffer[digit_count - index] = remainder + '0';
    buffer[digit_count + 1] = 0;
}

void signed_to_string_vprintf(s64 value, char* buffer)
{
    u8 is_negative = value < 0;
    if (is_negative)
    {
        value *= is_negative ? -1 : 1;
        buffer[0] = '-';
    }

    u8 digit_count = 0;
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
        buffer[is_negative + digit_count - index] = remainder + '0';
        index++;
    }

    u8 remainder = value % 10;
    buffer[is_negative + digit_count - index] = remainder + '0';
    buffer[is_negative + digit_count + 1] = 0;
}

void hex_to_string_bytes_vprintf(u64 value, u8 bytes_to_print, char* buffer)
{
    char aux_buffer[256];

    u32 digit_count = 0;

    if (value)
    {
        while (value)
        {
            u64 remainder = value % 16;

            aux_buffer[digit_count++] = remainder + 48 + (7 * (remainder > 10));
            value /= 16;
        }
    }
    else
    {
        aux_buffer[digit_count++] = '0';
    }

    u32 prefix = 0;
    buffer[prefix++] = '0';
    buffer[prefix++] = 'x';
    u8 digits_demanded = bytes_to_print * 2;
    u8 zero_to_write_count = digits_demanded - digit_count;

    for (u32 i = 0; i < zero_to_write_count; i++)
    {
        buffer[prefix++] = '0';
    }

    char* ptr = buffer + prefix;

    for (s32 i = digit_count - 1, real_i = 0; i >= 0; i--, real_i++)
    {
        ptr[real_i] = aux_buffer[i];
    }
    ptr[digit_count] = 0;
}

void hex_to_string_u8_vprintf(u8 value, char* buffer)
{
    return hex_to_string_bytes_vprintf(value, sizeof(u8), buffer);
}
void hex_to_string_u16_vprintf(u16 value, char* buffer)
{
    return hex_to_string_bytes_vprintf(value, sizeof(u16), buffer);
}
void hex_to_string_u32_vprintf(u32 value, char* buffer)
{
    return hex_to_string_bytes_vprintf(value, sizeof(u32), buffer);
}
void hex_to_string_u64_vprintf(u64 value, char* buffer)
{
    return hex_to_string_bytes_vprintf(value, sizeof(u64), buffer);
}

void float_to_string_vprintf(f64 value, u8 decimal_digits, char* buffer)
{
    if (decimal_digits > 20)
    {
        decimal_digits = 20;
    }

    char signed_buffer[128];
    signed_to_string_vprintf((s64)value, signed_buffer);

    if (value < 0)
    {
        value *= -1;
    }

    char* int_ptr = signed_buffer;
    char* float_ptr = buffer;

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
}

typedef enum FormattingMode
{
    Error = 0,
    String,
    SignedInteger8,
    SignedInteger16,
    SignedInteger32,
    SignedInteger64,
    UnsignedInteger8,
    UnsignedInteger16,
    UnsignedInteger32,
    UnsignedInteger64,
    Hexadecimal8,
    Hexadecimal16,
    Hexadecimal32,
    Hexadecimal64,
    Binary8,
    Binary16,
    Binary32,
    Binary64,
    Float,
    Char,
    Bool,
} FormattingMode;

const FormattingMode format_modes8[] =
{
    SignedInteger8, UnsignedInteger8, Hexadecimal8, Binary8,
};

const FormattingMode format_modes16[] =
{
    SignedInteger16, UnsignedInteger16, Hexadecimal16, Binary16,
};
const FormattingMode format_modes32[] =
{
    SignedInteger32, UnsignedInteger32, Hexadecimal32, Binary32,
};
const FormattingMode format_modes64[] =
{
    SignedInteger64, UnsignedInteger64, Hexadecimal64, Binary64,
};

static inline FormattingMode find_formatting_mode_internal(char c, const FormattingMode* allowed, usize allowed_count)
{
    FormattingMode bot_format_mode = 0;
    FormattingMode top_format_mode = 0;

    switch (c)
    {
        case 's':
            bot_format_mode = SignedInteger8;
            top_format_mode = SignedInteger64;
            break;
        case 'u':
            bot_format_mode = UnsignedInteger8;
            top_format_mode = UnsignedInteger64;
            break;
        case 'h':
            bot_format_mode = Hexadecimal8;
            top_format_mode = Hexadecimal64;
            break;
        case 'b':
            bot_format_mode = Binary8;
            top_format_mode = Binary64;
            break;
        default:
            return Error;
    }

    for (u32 format = bot_format_mode; format <= top_format_mode; format++)
    {
        for (u32 i = 0; i < allowed_count; i++)
        {
            if (allowed[i] == format)
            {
                return format;
            }
        }
    }

    return Error;
}

FormattingMode find_formatting_mode(const char *str, u32 i)
{
    FormattingMode mode;
    char next_char = str[i + 1];
    switch (next_char)
    {
        case '8':
        {
            return find_formatting_mode_internal(str[i + 2], format_modes8, array_length(format_modes8));
        }
        case '1':
        {
            if (str[i + 2] == '6')
            {
                return find_formatting_mode_internal(str[i + 3], format_modes16, array_length(format_modes16));
            }

            break;
        }
        case '3':
        {
            if (str[i + 2] == '2')
            {
                return find_formatting_mode_internal(str[i + 3], format_modes32, array_length(format_modes32));
            }

            break;
        }
        case '6':
        {
            if (str[i + 2] == '4')
            {
                return find_formatting_mode_internal(str[i + 3], format_modes64, array_length(format_modes64));
            }

            break;
        }
        case 's':
        {
            return String;
        }
        case 'f':
        {
            return Float;
        }
        case 'c':
        {
            return Char;
        }
        case 'b':
        {
            return Bool;
        }
        default:
            break;
    }

    return Error;
}


void panic(const char*, ...);
s32 println(const char*, ...);
s32 print(const char*, ...);
s32 vprint(const char*, va_list list);

void _assert(bool condition, const char* condition_name)
{
    if (!condition)
    {
        panic(condition_name);
    }
}

// @TODO: this is just 8-bit memset
void memset(void* address, u8 value, u64 bytes)
{
    u8* it = (u8*)address;
    for (u64 i = 0; i < bytes; i++)
    {
        it[i] = value;
    }
}

s32 vprint(const char* format, va_list list)
{
    char buffer[128];
    s32 char_count = 0;

    for (u32 i = 0; format[i]; i++)
    {
        char c = format[i];
        bool formatting_mode = c == '%';

        switch (c)
        {
            case '%':
                {
                    FormattingMode mode;
                    if (format[i + 1])
                    {
                        mode = find_formatting_mode(format, i);
                    }
                    else
                    {
                        mode = Error;
                    }

                    if (mode) // No error
                    {
                        char* write_here = buffer;
                        switch (mode)
                        {
                            case String:
                                {
                                    i++;
                                    write_here = va_arg(list, char*);
                                    break;
                                }
                            case UnsignedInteger8:
                                {
                                    i += 2;
                                    u8 value = (u8)va_arg(list, u32);
                                    unsigned_to_string_vprintf(value, write_here);
                                    break;
                                }
                            case UnsignedInteger16:
                                {
                                    i += 3;
                                    u16 value = (u16)va_arg(list, u32);
                                    unsigned_to_string_vprintf(value, write_here);
                                    break;
                                }
                            case UnsignedInteger32:
                                {
                                    i += 3;
                                    u32 value = (u32)va_arg(list, u32);
                                    unsigned_to_string_vprintf(value, write_here);
                                    break;
                                }
                            case UnsignedInteger64:
                                {
                                    i += 3;
                                    u64 value = (u64)va_arg(list, u64);
                                    unsigned_to_string_vprintf(value, write_here);
                                    break;
                                }
                            case SignedInteger8:
                                {
                                    i += 2;
                                    s8 value = (s8)va_arg(list, s32);
                                    signed_to_string_vprintf(value, write_here);
                                    break;
                                }
                            case SignedInteger16:
                                {
                                    i += 3;
                                    s16 value = (s16)va_arg(list, s32);
                                    signed_to_string_vprintf(value, write_here);
                                    break;
                                }
                            case SignedInteger32:
                                {
                                    i += 3;
                                    s32 value = (s32)va_arg(list, s32);
                                    signed_to_string_vprintf(value, write_here);
                                    break;
                                }
                            case SignedInteger64:
                                {
                                    i += 3;
                                    s64 value = (s64)va_arg(list, s64);
                                    signed_to_string_vprintf(value, write_here);
                                    break;
                                }
                            case Hexadecimal8:
                                {
                                    i += 2;
                                    u8 value = (u8)va_arg(list, u32);
                                    hex_to_string_u8_vprintf(value, write_here);
                                    break;
                                }
                            case Hexadecimal16:
                                {
                                    i += 3;
                                    u16 value = (u16)va_arg(list, u32);
                                    hex_to_string_u16_vprintf(value, write_here);
                                    break;
                                }
                            case Hexadecimal32:
                                {
                                    i += 3;
                                    u32 value = (u32)va_arg(list, u32);
                                    hex_to_string_u32_vprintf(value, write_here);
                                    break;
                                }
                            case Hexadecimal64:
                                {
                                    i += 3;
                                    u64 value = (u64)va_arg(list, u64);
                                    hex_to_string_u64_vprintf(value, write_here);
                                    break;
                                }
                            case Binary8: case Binary16: case Binary32: case Binary64:
                                {
                                    // @TODO: implement
                                    break;
                                }
                            case Float:
                                {
                                    i++;
                                    f64 value = va_arg(list, f64);
                                    float_to_string_vprintf(value, 5, write_here);
                                    break;
                                }
                            case Char:
                                {
                                    i++;
                                    char value = (char)va_arg(list, u32);
                                    write_here[0] = value;
                                    write_here[1] = 0;
                                    break;
                                }
                            case Bool:
                                {
                                    i++;
                                    bool value = (bool)va_arg(list, u32);
                                    if (value)
                                    {
                                        write_here[0] = 't';
                                        write_here[1] = 'r';
                                        write_here[2] = 'u';
                                        write_here[3] = 'e';
                                        write_here[4] = '\0';
                                    }
                                    else
                                    {
                                        write_here[0] = 'f';
                                        write_here[1] = 'a';
                                        write_here[2] = 'l';
                                        write_here[3] = 's';
                                        write_here[4] = 'e';
                                        write_here[5] = '\0';
                                    }
                                }
                            default:
                                break;
                        }

                        char c;
                        for (u32 i = 0; (c = write_here[i]); i++)
                        {
                            putc(c);
                            char_count++;
                        }
                    }
                    else
                    {
error:
                        assert("This is an error" && false);
                        return char_count;
                    }
                }
                break;
            case '\t':
                putc(' ');
                char_count++;
                while (char_count % 4 != 0)
                {
                    putc(' ');
                    char_count++;
                }
                break;
            case '\n':
                new_line();
                char_count++;
                break;
            default:
                putc(c);
                char_count++;
                break;
        }
    }

    return char_count;
}

s32 print(const char* format, ...)
{
    va_list list;
    va_start(list, format);
    s32 written_char_count = vprint(format, list);
    va_end(list);
    return written_char_count;

}

s32 println(const char* format, ...)
{
    va_list list;
    va_start(list, format);
    s32 written_char_count = vprint(format, list);
    va_end(list);
    new_line();
    return written_char_count + 1;
}

char* strcpy(char* dst, const char* src)
{
    if (src && dst)
    {
        char* src_it = (char*)src;
        char* dst_it = dst;
        while (*src_it)
        {
            *dst_it++ = *src_it++;
        }
        *dst_it = *src_it;
    }

    return dst;
}

s32 strcmp(const char* lhs, const char* rhs)
{
    char* lit = (char*)lhs;
    char* rit = (char*)rhs;

    while (*lit == *rit)
    {
        if (*lit == 0)
        {
            return 0;
        }
        lit++;
        rit++;
    }

    return *lit - *rit;
}

s32 strncmp(const char* lhs, const char* rhs, usize count)
{
    char* lit = (char*)lhs;
    char* rit = (char*)rhs;
    bool eq;

    for (usize i = 0; i < count && (eq = *lit == *rit); i++, lit++, rit++)
    {
        if (*lit == 0)
        {
            return 0;
        }
    }

    return (*lit - *rit) * (!eq);
}

usize strlen(const char* s)
{
    char* it = (char*)s;

    while (*it)
    {
        it++;
    }

    return it - s;
}

bool string_eq(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}
// Reverse copy the string (including the null-terminated character
void string_reverse(char* dst, const char* src, usize bytes)
{
    for (u32 i = 0; i < bytes; i++)
    {
        dst[i] = src[bytes - i - 1];
    }
    dst[bytes] = 0;
}

static char hex_lookup_table[255] =
{
    ['0'] = 0,
    ['1'] = 1,
    ['2'] = 2,
    ['3'] = 3,
    ['4'] = 4,
    ['5'] = 5,
    ['6'] = 6,
    ['7'] = 7,
    ['8'] = 8,
    ['9'] = 9,
    ['a'] = 0xa,
    ['b'] = 0xb,
    ['c'] = 0xc,
    ['d'] = 0xd,
    ['e'] = 0xe,
    ['f'] = 0xf,
    ['A'] = 0xA,
    ['B'] = 0xB,
    ['C'] = 0xC,
    ['D'] = 0xD,
    ['E'] = 0xE,
    ['F'] = 0xF,
};

static char decimal_lookup_table[255] =
{
    ['0'] = 0,
    ['1'] = 1,
    ['2'] = 2,
    ['3'] = 3,
    ['4'] = 4,
    ['5'] = 5,
    ['6'] = 6,
    ['7'] = 7,
    ['8'] = 8,
    ['9'] = 9,
};

static char binary_lookup_table[255] =
{
    ['0'] = 0,
    ['1'] = 1,
};

static const char* format_lookup_table[] =
{
    [BINARY] = binary_lookup_table,
    [DECIMAL] = decimal_lookup_table,
    [HEXADECIMAL] = hex_lookup_table
};

static const u64 format_values_per_digit[] = 
{
    [BINARY] = 2,
    [DECIMAL] = 10,
    [HEXADECIMAL] = 16,
};

u64 string_to_unsigned(const char* str)
{
    FormatLookupTableIndex table_index = DECIMAL;
    usize len = strlen(str);
    char* it = (char*) str;

    if (it[0] == '0' && it[1] == 'x')
    {
        table_index = HEXADECIMAL;
        len -= 2;
        it += 2;
    }
    else if (it[0] == '0' && it[1] == 'b')
    {
        table_index = BINARY;
        len -= 2;
        it += 2;
    }

    const char* lookup_table = format_lookup_table[table_index];
    u64 values_per_digit = format_values_per_digit[table_index];

    char copy[65];
    string_reverse(copy, it, len);

    u64 result = 0;
    u64 byte_multiplier = 1;

    for (u32 i = 0; i < len; i++, byte_multiplier *= values_per_digit)
    {
        u64 lookup_value = lookup_table[copy[i]];
        u64 mul = lookup_value * byte_multiplier;
        result += mul;
    }

    return result;
}