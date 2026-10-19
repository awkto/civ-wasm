/* files.c: DOS file names and handles on the host.
 *
 * The game opens files by DOS name in its current directory. reciv looks
 * them up in the save directory, then in the game directory (the player's
 * original Civilization files), matching names without regard to case.
 * New files are created in the save directory. DOS handles are small
 * numbers; 0-4 are DOS's standard devices, so game files start at 5. */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define truncate_at(f, n) _chsize(_fileno(f), (long) (n))
#else
#include <dirent.h>
#include <unistd.h>
#define truncate_at(f, n) ftruncate(fileno(f), (off_t) (n))
#endif

#include "platform.h"

static char game_dir[1024] = ".", save_dir[1024] = ".";

void reciv_set_dirs(const char *game, const char *save)
{
    snprintf(game_dir, sizeof game_dir, "%s", game);
    snprintf(save_dir, sizeof save_dir, "%s", save);
}

/* DOS names may carry a drive ("A:CIVIL0.SVE"); reciv has one directory. */
static const char *base_name(const char *name)
{
    const char *colon = strchr(name, ':');
    return colon ? colon + 1 : name;
}

/* Find `name` in `dir` ignoring case; writes the full path to `out`. */
static int find_in(const char *dir, const char *name, char *out, size_t size)
{
#ifdef _WIN32
    snprintf(out, size, "%s\\%s", dir, name);
    return _access(out, 0) == 0;
#else
    DIR *d = opendir(dir);
    struct dirent *e;
    int found = 0;
    if (!d)
        return 0;
    while (!found && (e = readdir(d)) != NULL) {
        const char *a = e->d_name, *b = name;
        while (*a && tolower((unsigned char) *a) == tolower((unsigned char) *b))
            a++, b++;
        if (!*a && !*b) {
            snprintf(out, size, "%s/%s", dir, e->d_name);
            found = 1;
        }
    }
    closedir(d);
    return found;
#endif
}

/* Path for reading `name`, or NULL. */
static const char *read_path(const char *name, char *out, size_t size)
{
    name = base_name(name);
    if (find_in(save_dir, name, out, size) || find_in(game_dir, name, out, size))
        return out;
    return NULL;
}

/* Path for writing `name`: an existing file of that name in the save
 * directory keeps its spelling; otherwise the name in upper case, as DOS
 * stored it. */
static const char *write_path(const char *name, char *out, size_t size)
{
    char upper[256];
    size_t i;
    name = base_name(name);
    if (find_in(save_dir, name, out, size))
        return out;
    for (i = 0; name[i] && i + 1 < sizeof upper; i++)
        upper[i] = (char) toupper((unsigned char) name[i]);
    upper[i] = 0;
    snprintf(out, size, "%s/%s", save_dir, upper);
    return out;
}

FILE *reciv_fopen(const char *name, const char *mode)
{
    char path[1300], host_mode[8];
    int writing = strchr(mode, 'w') || strchr(mode, 'a');
    const char *p = writing ? write_path(name, path, sizeof path) : read_path(name, path, sizeof path);
    /* Text mode is handled by the callers (crt_fscanf drops CR). */
    snprintf(host_mode, sizeof host_mode, "%cb%s", mode[0], strchr(mode, '+') ? "+" : "");
    return p ? fopen(p, host_mode) : NULL;
}

/* ------------------------------------------------------------- handles */

#define FIRST_HANDLE 5
#define MAX_HANDLES 20
static FILE *handles[MAX_HANDLES];

static int16_t new_handle(FILE *f)
{
    for (int i = FIRST_HANDLE; i < MAX_HANDLES; i++)
        if (!handles[i]) {
            handles[i] = f;
            return (int16_t) i;
        }
    fclose(f);
    return -1;
}

static FILE *handle(int16_t fd)
{
    return fd >= FIRST_HANDLE && fd < MAX_HANDLES ? handles[fd] : NULL;
}

/* msc_flags: O_RDONLY 0, O_WRONLY 1, O_RDWR 2, O_CREAT 100h, O_TRUNC 200h. */
int16_t reciv_open(const char *name, uint16_t msc_flags)
{
    char path[1300];
    const char *p;
    FILE *f;
    int access = msc_flags & 3;
    if (msc_flags & 0x0200 || ((msc_flags & 0x0100) && !read_path(name, path, sizeof path)))
        return reciv_create(name);
    p = access == 0 ? read_path(name, path, sizeof path) : write_path(name, path, sizeof path);
    if (!p || !(f = fopen(p, access == 0 ? "rb" : "r+b")))
        return -1;
    return new_handle(f);
}

int16_t reciv_create(const char *name)
{
    char path[1300];
    FILE *f = fopen(write_path(name, path, sizeof path), "w+b");
    return f ? new_handle(f) : -1;
}

int16_t reciv_close(int16_t fd)
{
    FILE *f = handle(fd);
    if (!f)
        return -1;
    handles[fd] = NULL;
    return (int16_t) (fclose(f) ? -1 : 0);
}

int32_t reciv_read(int16_t fd, void *buf, uint16_t n)
{
    FILE *f = handle(fd);
    size_t count;
    if (!f)
        return -1;
    count = fread(buf, 1, n, f);
    return ferror(f) ? -1 : (int32_t) count;
}

int32_t reciv_write(int16_t fd, const void *buf, uint16_t n)
{
    FILE *f = handle(fd);
    size_t count;
    if (!f)
        return -1;
    if (n == 0) {
        /* DOS: writing zero bytes truncates the file here. */
        if (fflush(f))
            return -1;
        return truncate_at(f, ftell(f)) ? -1 : 0;
    }
    count = fwrite(buf, 1, n, f);
    return ferror(f) ? -1 : (int32_t) count;
}

uint16_t crt_dos_read(int16_t fd, void *buf, uint16_t n, uint16_t *count)
{
    int32_t result = reciv_read(fd, buf, n);
    if (result < 0)
        return 6;
    *count = (uint16_t) result;
    return 0;
}

uint16_t crt_dos_write(int16_t fd, const void *buf, uint16_t n, uint16_t *count)
{
    int32_t result = reciv_write(fd, buf, n);
    if (result < 0)
        return 6;
    *count = (uint16_t) result;
    return 0;
}

int32_t reciv_lseek(int16_t fd, int32_t offset, int16_t whence)
{
    FILE *f = handle(fd);
    if (!f || fseek(f, offset, whence))
        return -1;
    return (int32_t) ftell(f);
}

/* --------------------------------------------------------- diagnostics */

void reciv_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("reciv: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

void reciv_fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("reciv: fatal: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(2);
}

void reciv_unimplemented(const char *what)
{
    reciv_fatal("not implemented yet: %s", what);
}

/* Whether a file can be opened for reading (FCHECK's file_exists). */
int reciv_file_found(const char *name)
{
    char path[1300];
    return read_path(name, path, sizeof path) != NULL;
}
