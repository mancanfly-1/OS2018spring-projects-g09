#include "user.h"

/*
 * Print a number (base <= 16) in reverse order,
 * using specified putch function and associated pointer putdat.
 */
static void printnum(void (*putch)(int, void *), void *putdat, unsigned long long num,
                     unsigned base, int width, int padc, bool upper)
{
    /* recursion a bad idea here */
    char buf[68], *x;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    for (x = buf; num; num /= base)
        *x++ = digits[num % base];
    if (x == buf)
        *x++ = '0';

    if (padc != '-')
        for (; width > x - buf; width--)
            putch(padc, putdat);

    for (; x > buf; width--)
        putch(*--x, putdat);

    if (padc == '-')
        for (; width > 0; width--)
            putch(' ', putdat);
}

/*
 * Get an unsigned int of various possible sizes from a varargs list,
 * depending on the lflag parameter.
 *
 * These cannot be functions without autoconf-like glue, unfortunately.
 * gcc-3.4.5 on x86_64 passes va_list by reference, and doesn't like the
 * type (va_list*), and gcc-3.4.6 on i386 passes va_list by value, and
 * does handle the type (va_list*).
 *
 * [http://www.opengroup.org/onlinepubs/009695399/basedefs/stdarg.h.html]
 * The object ap may be passed as an argument to another function;
 * if that function invokes the va_arg() macro with parameter ap,
 * the value of ap in the calling function is unspecified and shall
 * be passed to the va_end() macro prior to any further reference to ap.
 */

#define getuint(ap, lflag)                                                                         \
    ({                                                                                             \
        long long __v;                                                                             \
        if (lflag >= 2)                                                                            \
            __v = va_arg(ap, unsigned long long);                                                  \
        else if (lflag)                                                                            \
            __v = va_arg(ap, unsigned long);                                                       \
        else                                                                                       \
            __v = va_arg(ap, unsigned int);                                                        \
        __v;                                                                                       \
    })

/* same as getuint but signed - can't use getuint because of sign extension */
#define getint(ap, lflag)                                                                          \
    ({                                                                                             \
        long long __v;                                                                             \
        if (lflag >= 2)                                                                            \
            __v = va_arg(ap, long long);                                                           \
        else if (lflag)                                                                            \
            __v = va_arg(ap, long);                                                                \
        else                                                                                       \
            __v = va_arg(ap, int);                                                                 \
        __v;                                                                                       \
    })

static void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap)
{
    const char *p;
    int ch;
    unsigned long long num;
    int base, lflag, width, precision, altflag;
    char padc;
    bool upper;

    while (1) {
        while ((ch = *(unsigned char *)fmt++) != '%') {
            if (ch == '\0')
                return;
            putch(ch, putdat);
        }

        /* process a %-escape sequence */
        padc = ' ';
        width = -1;
        precision = -1;
        lflag = 0;
        altflag = 0;
        upper = false;
    reswitch:
        switch (ch = *(unsigned char *)fmt++) {

        /* flag to pad on the right */
        case '-':
            padc = '-';
            goto reswitch;

        /* flag to pad with 0's instead of spaces */
        case '0':
            padc = '0';
            goto reswitch;

        /* width field */
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            for (precision = 0;; ++fmt) {
                precision = precision * 10 + ch - '0';
                ch = *fmt;
                if (ch < '0' || ch > '9')
                    break;
            }
            goto process_precision;

        case '*':
            precision = va_arg(ap, int);
            goto process_precision;

        case '.':
            if (width < 0)
                width = 0;
            goto reswitch;

        case '#':
            altflag = 1;
            goto reswitch;

        process_precision:
            if (width < 0)
                width = precision, precision = -1;
            goto reswitch;

        case 'h':
            /* ignore */
            goto reswitch;

        /* long flag (doubled for long long) */
        case 'l':
            lflag++;
            goto reswitch;

        /* size_t */
        case 'z':
            if (sizeof(size_t) == sizeof(long))
                lflag = 1;
            else if (sizeof(size_t) == sizeof(long long))
                lflag = 2;
            else
                lflag = 0;
            goto reswitch;

        /* character */
        case 'c':
            putch(va_arg(ap, int), putdat);
            break;

        /* string */
        case 's':
            if ((p = va_arg(ap, char *)) == NULL)
                p = "(null)";
            if (width > 0 && padc != '-')
                for (width -= strlen(p); width > 0; width--)
                    putch(padc, putdat);
            for (; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--)
                if (altflag && (ch < ' ' || ch > '~'))
                    putch('?', putdat);
                else
                    putch(ch, putdat);
            for (; width > 0; width--)
                putch(' ', putdat);
            break;

        /* binary */
        case 'b':
            num = getint(ap, lflag);
            base = 2;
            goto number;

        /* (signed) decimal */
        case 'd':
            num = getint(ap, lflag);
            if ((long long)num < 0) {
                putch('-', putdat);
                num = -(long long)num;
            }
            base = 10;
            goto number;

        /* unsigned decimal */
        case 'u':
            num = getuint(ap, lflag);
            base = 10;
            goto number;

        /* (unsigned) octal */
        case 'o':
            num = getuint(ap, lflag);
            base = 8;
            if (altflag && num)
                putch('0', putdat);
            goto number;

        /* pointer */
        case 'p':
            putch('0', putdat);
            putch('x', putdat);
            num = (unsigned long long)(uintptr_t)va_arg(ap, void *);
            base = 16;
            goto number;

        /* (unsigned) hexadecimal */
        case 'X':
            upper = true;
        case 'x':
            num = getuint(ap, lflag);
            base = 16;
            if (altflag && num) {
                putch('0', putdat);
                putch('x', putdat);
            }
        number:
            printnum(putch, putdat, num, base, max(width, 0), padc, upper);
            break;

        /* unrecognized escape sequence - just print it literally */
        default:
            putch('%', putdat);
            while (lflag-- > 0)
                putch('l', putdat);
        /* FALLTHROUGH */

        /* escaped '%' character */
        case '%':
            putch(ch, putdat);
        }
    }
}

void vdprintf(int fd, const char *fmt, va_list ap)
{
    char buf[1024] = {0};

    vsnprintf(buf, sizeof(buf), fmt, ap);
    write(fd, buf, strlen(buf));
}

void dprintf(int fd, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fd, fmt, ap);
    va_end(ap);
}

struct sputcharg {
    char *str;
    char *end;
};

static void sputch(int c, void *arg)
{
    struct sputcharg *b = arg;

    if (b->str < b->end) {
        *b->str = c;
        ++b->str;
    }
}

void vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    struct sputcharg arg = {str, str + size - 1};

    vprintfmt(sputch, &arg, fmt, ap);
    if (arg.str < arg.end)
        *arg.str = 0;
    else if (size > 0)
        *arg.end = 0;
}

void snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(str, size, fmt, ap);
    va_end(ap);
}

static void putc(int c, void *arg)
{
    char ch = c;

    debug_print_console(&ch, 1);
}

void vcprintf(const char *fmt, va_list ap)
{
    vprintfmt(putc, NULL, fmt, ap);
}

void cprintf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vcprintf(fmt, ap);
    va_end(ap);
}

//static void debug_print(const void *s, size_t n, void (*func)(physaddr_t, size_t))
//{
//    physaddr_t pa;
//    size_t off, len;
//
//    off = ((uintptr_t)s) % PAGE_SIZE;
//    pa = virt_to_phys(s - off) + off;
//    while (n) {
//        len = min(n, PAGE_SIZE - (pa % PAGE_SIZE));
//        func(pa, len);
//        n -= len;
//        pa += len;
//    }
//}

static void debug_print(const void *s, size_t n, void (*func)(uintptr_t, size_t))
{
	func(s, n);
}

void debug_print_console(const void *s, size_t n)
{
    debug_print(s, n, sys_debug_print_console);
}

void debug_print_screen(const void *s, size_t n)
{
    debug_print(s, n, sys_debug_print_screen);
}

void syslog(int priority, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    libs_vcprintf(fmt, ap);
    //vsyslog(priority, fmt, ap);
    va_end(ap);
}

void vsyslog(int priority, const char *fmt, va_list ap)
{
    vcprintf(fmt, ap);
}
