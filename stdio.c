#include "stdio.h"
#include <stdarg.h>
#include <stdint.h>

extern void terminal_writestring(const char* str, uint32_t fg_color);
extern void terminal_putchar(char c, uint32_t fg_color);

int putchar(int c) {
    terminal_putchar((char)c, 0x00FFFFFF);
    return c;
}

int printf(const char* format, ...) {
    va_list parameters;
    va_start(parameters, format);

    int written = 0;
    while (*format != '\0') {
        if (*format != '%') {
            putchar(*format);
            format++;
            written++;
            continue;
        }

        format++;
        if (*format == 'c') {
            char c = (char)va_arg(parameters, int);
            putchar(c);
            written++;
        } else if (*format == 's') {
            const char* str = va_arg(parameters, const char*);
            terminal_writestring(str, 0x00FFFFFF);
            while (*str) { written++; str++; }
        } else if (*format == 'd') {
            int val = va_arg(parameters, int);
            char buf[32];
            int i = 0;
            if (val == 0) {
                putchar('0');
                written++;
            } else {
                if (val < 0) {
                    putchar('-');
                    written++;
                    val = -val;
                }
                while (val > 0) {
                    buf[i++] = '0' + (val % 10);
                    val /= 10;
                }
                while (i > 0) {
                    putchar(buf[--i]);
                    written++;
                }
            }
        }
        format++;
    }

    va_end(parameters);
    return written;
}
