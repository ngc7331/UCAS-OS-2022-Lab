/*
 * The Minimal snprintf() implementation
 *
 * Copyright (c) 2013,2014 Michal Ludvig <michal@logix.cz>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the auhor nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ----
 *
 * This is a minimal snprintf() implementation optimised
 * for embedded systems with a very limited program memory.
 * mini_snprintf() doesn't support _all_ the formatting
 * the glibc does but on the other hand is a lot smaller.
 * Here are some numbers from my STM32 project (.bin file size):
 *      no snprintf():      10768 bytes
 *      mini snprintf():    11420 bytes     (+  652 bytes)
 *      glibc snprintf():   34860 bytes     (+24092 bytes)
 * Wasting nearly 24kB of memory just for snprintf() on
 * a chip with 32kB flash is crazy. Use mini_snprintf() instead.
 *
 */
#include <screen.h>
#include <stdarg.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/time.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <printk.h>

static unsigned int mini_strlen(const char *s)
{
    unsigned int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

static unsigned int mini_itoa(
    long value, unsigned int radix, unsigned int uppercase,
    unsigned int unsig, char *buffer, unsigned int zero_pad)
{
    char *pbuffer = buffer;
    int negative  = 0;
    unsigned int i, len;

    /* No support for unusual radixes. */
    if (radix > 16) return 0;

    if (value < 0 && !unsig) {
        negative = 1;
        value    = -value;
    }

    /* This builds the string back to front ... */
    do {
        int digit = value % radix;
        *(pbuffer++) =
            (digit < 10 ? '0' + digit :
                          (uppercase ? 'A' : 'a') + digit - 10);
        value /= radix;
    } while (value > 0);

    for (i = (pbuffer - buffer); i < zero_pad; i++)
        *(pbuffer++) = '0';

    if (negative) *(pbuffer++) = '-';

    *(pbuffer) = '\0';

    /* ... now we reverse it (could do it recursively but will
     * conserve the stack space) */
    len = (pbuffer - buffer);
    for (i = 0; i < len / 2; i++) {
        char j              = buffer[i];
        buffer[i]           = buffer[len - i - 1];
        buffer[len - i - 1] = j;
    }

    return len;
}

struct mini_buff
{
    char *buffer, *pbuffer;
    unsigned int buffer_len;
};

static int _putc(int ch, struct mini_buff *b)
{
    if ((unsigned int)((b->pbuffer - b->buffer) + 1) >=
        b->buffer_len)
        return 0;
    *(b->pbuffer++) = ch;
    *(b->pbuffer)   = '\0';
    return 1;
}

static int _puts(char *s, unsigned int len, struct mini_buff *b)
{
    unsigned int i;

    if (b->buffer_len - (b->pbuffer - b->buffer) - 1 < len)
        len = b->buffer_len - (b->pbuffer - b->buffer) - 1;

    /* Copy to buffer */
    for (i = 0; i < len; i++) *(b->pbuffer++) = s[i];
    *(b->pbuffer) = '\0';

    return len;
}

static int mini_vsnprintf(
    char *buffer, unsigned int buffer_len, const char *fmt,
    va_list va)
{
    struct mini_buff b;
    char bf[24];
    char ch;

    b.buffer     = buffer;
    b.pbuffer    = buffer;
    b.buffer_len = buffer_len;

    while ((ch = *(fmt++))) {
        if ((unsigned int)((b.pbuffer - b.buffer) + 1) >=
            b.buffer_len)
            break;
        if (ch != '%')
            _putc(ch, &b);
        else {
            char zero_pad = 0;
            int longflag = 0;
            char *ptr;
            unsigned int len;

            ch = *(fmt++);

            /* Zero padding requested */
            if (ch == '0') {
                while ((ch = *(fmt++))) {
                    if (ch == '\0') goto end;
                    if (ch >= '0' && ch <= '9') {
                        zero_pad = zero_pad * 10 + ch - '0';
                    } else {
                        break;
                    }
                }
            }
            if (ch == 'l') {
                longflag = 1;
                ch = *(fmt++);
            }

            switch (ch) {
                case 0:
                    goto end;

                case 'l':
                    longflag = 1;
                    break;

                case 'u':
                case 'd':
                    len = mini_itoa(
                        longflag == 0 ? (unsigned long)va_arg(
                                            va, unsigned int) :
                                        va_arg(va, unsigned long),
                        10, 0, (ch == 'u'), bf, zero_pad);
                    _puts(bf, len, &b);
                    longflag = 0;
                    break;
                case 'x':
                case 'X':
                    len = mini_itoa(
                        longflag == 0 ? (unsigned long)va_arg(
                                            va, unsigned int) :
                                        va_arg(va, unsigned long),
                        16, (ch == 'X'), 1, bf, zero_pad);
                    _puts(bf, len, &b);
                    longflag = 0;
                    break;

                case 'c':
                    _putc((char)(va_arg(va, int)), &b);
                    break;

                case 's':
                    ptr = va_arg(va, char *);
                    _puts(ptr, mini_strlen(ptr), &b);
                    break;

                default:
                    _putc(ch, &b);
                    break;
            }
        }
    }
end:
    return b.pbuffer - b.buffer;
}

static int _vprint(const char *fmt, va_list _va,
                   void (*output)(char*))
{
    va_list va;
    va_copy(va, _va);

    int ret;
    char buff[256];

    ret = mini_vsnprintf(buff, 256, fmt, va);

    buff[ret] = '\0';

    output(buff);

    return ret;
}

static void _output_wrapper(char *buff)
{
    screen_write(buff);
    screen_reflush();
}

static int vprintk(const char *fmt, va_list _va)
{
    return _vprint(fmt, _va, _output_wrapper);
}

int printk(const char *fmt, ...)
{
    int ret = 0;
    va_list va;

    va_start(va, fmt);
    ret = vprintk(fmt, va);
    va_end(va);

    return ret;
}

static int vprintv(const char *fmt, va_list _va)
{
    return _vprint(fmt, _va, bios_putstr);
}

int printv(const char *fmt, ...)
{
    int ret = 0;
    va_list va;

    va_start(va, fmt);
    ret = vprintv(fmt, va);
    va_end(va);

    return ret;
}

static int vprintl(const char *fmt, va_list _va)
{
    return _vprint(fmt, _va, bios_logging);
}

int printl(const char *fmt, ...)
{
    int ret = 0;
    va_list va;

    va_start(va, fmt);
    ret = vprintl(fmt, va);
    va_end(va);

    return ret;
}

#define ENABLE_LOGGING

static loglevel_t __level = LOG_VERBOSE;

int logging(loglevel_t level, const char* name, const char *fmt, ...)
{
#ifndef ENABLE_LOGGING
    return -1;
#else
    if (level < __level) return -1;
    char buf[42];
    char __dict[7] = "VDIWEC";
    int ret;
    va_list va;

    buf[0] = '[';
    buf[1] = '0' + get_current_cpu_id();
    buf[2] = ']';
    buf[3] = '[';
    mini_itoa(get_ticks(), 10, 0, 1, buf+4, 20);
    buf[24] = ']';
    buf[25] = '[';
    for (ret=26; *name && ret<36; name++)
        buf[ret++] = *name;
    for (; ret<36; )
        buf[ret++] = ' ';
    buf[ret++] = ']';
    buf[ret++] = '[';
    buf[ret++] = __dict[level];
    buf[ret++] = ']';
    buf[ret++] = ' ';
    buf[ret] = '\0';

    bios_logging(buf);

    va_start(va, fmt);
    ret = vprintl(fmt, va);
    va_end(va);

    return ret;
#endif
}

void set_loglevel(loglevel_t level) {
    __level = level;
}
