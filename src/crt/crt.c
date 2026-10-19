/* crt.c: the Microsoft C 5.1 run-time functions the game calls, with that
 * library's behaviour and 16-bit int (int16_t here, int32_t for long).
 * The game sees them through src/period/include under their usual names
 * (src/period/crtnames.h maps each to crt_*).
 *
 * Only what the game uses is here. Where MSC's behaviour is visible to the
 * game (rand's generator, itoa's radix handling, fscanf's one format), it
 * is reproduced exactly. */
#include <ctype.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "crt.h"
#include "../platform/platform.h"

int16_t crt_errno;

/* ---------------------------------------------------------------- numbers */

int16_t crt_abs(int16_t v)
{
    return (int16_t) (v < 0 ? -v : v);
}

/* rand(): MSC's linear congruential generator, seed 1 at start-up. */
static uint32_t holdrand = 1;

void crt_srand(uint16_t seed)
{
    holdrand = seed;
}

int16_t crt_rand(void)
{
    holdrand = holdrand * 214013u + 2531011u;
    return (int16_t) ((holdrand >> 16) & 0x7FFF);
}

/* itoa(): radix 10 is signed; other radixes print the 16-bit pattern. */
char *crt_itoa(int16_t value, char *buf, int16_t radix)
{
    char digits[18];
    int n = 0;
    uint16_t u = (uint16_t) value;
    char *p = buf;
    if (radix == 10 && value < 0) {
        *p++ = '-';
        u = (uint16_t) -value;
    }
    do {
        int d = u % (uint16_t) radix;
        digits[n++] = (char) (d < 10 ? '0' + d : 'a' + d - 10);
        u /= (uint16_t) radix;
    } while (u);
    while (n)
        *p++ = digits[--n];
    *p = 0;
    return buf;
}

/* time(): seconds since 1970, as a 32-bit long. */
int32_t crt_time(int32_t *t)
{
    int32_t now = (int32_t) time(NULL);
    if (t)
        *t = now;
    return now;
}

/* ---------------------------------------------------------------- strings */

uint16_t crt_strlen(const char *s)
{
    return (uint16_t) strlen(s);
}

char *crt_strcpy(char *d, const char *s)
{
    return strcpy(d, s);
}

char *crt_strcat(char *d, const char *s)
{
    return strcat(d, s);
}

char *crt_strstr(const char *s, const char *t)
{
    return strstr(s, t);
}

static int fold(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int16_t crt_stricmp(const char *a, const char *b)
{
    while (*a && fold((unsigned char) *a) == fold((unsigned char) *b))
        a++, b++;
    return (int16_t) (fold((unsigned char) *a) - fold((unsigned char) *b));
}

int16_t crt_strnicmp(const char *a, const char *b, uint16_t n)
{
    for (; n; n--, a++, b++) {
        int d = fold((unsigned char) *a) - fold((unsigned char) *b);
        if (d || !*a)
            return (int16_t) d;
    }
    return 0;
}

char *crt_strupr(char *s)
{
    for (char *p = s; *p; p++)
        if (*p >= 'a' && *p <= 'z')
            *p -= 32;
    return s;
}

void *crt_memcpy(void *d, const void *s, uint16_t n)
{
    /* MSC's memcpy copies forwards; overlapping copies behave accordingly. */
    unsigned char *dp = d;
    const unsigned char *sp = s;
    while (n--)
        *dp++ = *sp++;
    return d;
}

void *crt_memset(void *d, int16_t c, uint16_t n)
{
    return memset(d, (unsigned char) c, n);
}

/* --------------------------------------------------------------- streams */

struct crt_iobuf {
    FILE *host;
};

struct crt_iobuf *crt_fopen(const char *name, const char *mode)
{
    FILE *f = reciv_fopen(name, mode);
    struct crt_iobuf *s;
    if (!f) {
        crt_errno = 2;      /* ENOENT */
        return NULL;
    }
    s = malloc(sizeof *s);
    s->host = f;
    return s;
}

int16_t crt_fclose(struct crt_iobuf *s)
{
    int r = fclose(s->host);
    free(s);
    return (int16_t) (r ? -1 : 0);
}

uint16_t crt_fread(void *buf, uint16_t size, uint16_t n, struct crt_iobuf *s)
{
    return (uint16_t) fread(buf, size, n, s->host);
}

uint16_t crt_fwrite(const void *buf, uint16_t size, uint16_t n, struct crt_iobuf *s)
{
    return (uint16_t) fwrite(buf, size, n, s->host);
}

int16_t crt_fseek(struct crt_iobuf *s, int32_t offset, int16_t whence)
{
    return (int16_t) (fseek(s->host, offset, whence) ? -1 : 0);
}

int32_t crt_ftell(struct crt_iobuf *s)
{
    return (int32_t) ftell(s->host);
}

/* fscanf(): the game only uses "%[^\n]\n" -- read up to a newline, then
 * skip any whitespace. Returns 1, or EOF at end of file. Text mode: CR
 * before LF is dropped, as MSC's text streams do. */
int16_t crt_fscanf(struct crt_iobuf *s, const char *format, ...)
{
    va_list ap;
    char *out;
    int c, n = 0;
    if (strcmp(format, "%[^\n]\n") != 0)
        reciv_fatal("fscanf: unsupported format \"%s\"", format);
    va_start(ap, format);
    out = va_arg(ap, char *);
    va_end(ap);
    while ((c = fgetc(s->host)) != EOF && c != '\n')
        if (c != '\r')
            out[n++] = (char) c;
    if (n == 0 && c == EOF)
        return -1;
    out[n] = 0;
    if (n == 0 && c == '\n') {
        /* %[ matched nothing: MSC stops here and assigns nothing. */
        ungetc(c, s->host);
        return 0;
    }
    while ((c = fgetc(s->host)) != EOF && isspace(c))
        ;
    if (c != EOF)
        ungetc(c, s->host);
    return 1;
}

void crt_perror(const char *s)
{
    reciv_log("perror: %s", s);
}

/* ------------------------------------------------- low-level file I/O */

int16_t crt_open(const char *name, int16_t flags, ...)
{
    int16_t fd = reciv_open(name, (uint16_t) flags);
    if (fd < 0)
        crt_errno = 2;
    return fd;
}

int16_t crt_close(int16_t fd)
{
    return reciv_close(fd);
}

int16_t crt_read(int16_t fd, void *buf, uint16_t n)
{
    return (int16_t) reciv_read(fd, buf, n);
}

int16_t crt_write(int16_t fd, const void *buf, uint16_t n)
{
    return (int16_t) reciv_write(fd, buf, n);
}

int32_t crt_lseek(int16_t fd, int32_t offset, int16_t whence)
{
    return reciv_lseek(fd, offset, whence);
}

/* ---------------------------------------------------------------- process */

void crt_exit(int16_t status)
{
    reciv_exit(status);
}
