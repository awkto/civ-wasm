/* Regression: DOS byte counts are unsigned words, separate from status.
 * Run through CTest; argv[1] is a dedicated directory in the build tree. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #expr); \
        return 1; \
    } \
} while (0)

int main(int argc, char **argv)
{
    static const uint16_t sizes[] = { 0, 1, 32767, 32768, 65535 };
    static unsigned char source[65535], result[65535];
    char path[1300];
    uint16_t count;
    int16_t fd;
    CHECK(argc == 2);
    reciv_set_dirs(argv[1], argv[1]);
    for (size_t i = 0; i < sizeof source; i++)
        source[i] = (unsigned char) (i * 17 + i / 256);

    for (size_t i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        uint16_t n = sizes[i];
        fd = reciv_create("C:byte-count.bin");
        CHECK(fd >= 5);
        count = 123;
        CHECK(crt_dos_write(fd, source, n, &count) == 0);
        CHECK(count == n);
        CHECK(reciv_close(fd) == 0);

        /* Lookup must retain DOS's case-insensitive filenames. */
        fd = reciv_open("c:BYTE-count.BIN", 2);
        CHECK(fd >= 5);
        CHECK(reciv_lseek(fd, 0, SEEK_END) == n);
        CHECK(reciv_lseek(fd, 0, SEEK_SET) == 0);
        memset(result, 0, sizeof result);
        CHECK(crt_dos_read(fd, result, 65535, &count) == 0);
        CHECK(count == n);
        CHECK(memcmp(source, result, n) == 0);
        CHECK(crt_dos_read(fd, result, 1, &count) == 0);
        CHECK(count == 0);       /* EOF is success with no bytes. */

        /* A zero-length DOS write truncates at the current position. */
        CHECK(reciv_lseek(fd, n / 2, SEEK_SET) == n / 2);
        count = 123;
        CHECK(crt_dos_write(fd, source, 0, &count) == 0);
        CHECK(count == 0);
        CHECK(reciv_lseek(fd, 0, SEEK_END) == n / 2);
        CHECK(reciv_close(fd) == 0);
    }

    count = 123;
    CHECK(crt_dos_read(-1, result, 65535, &count) == 6);
    CHECK(count == 123);
    CHECK(crt_dos_write(-1, source, 65535, &count) == 6);
    CHECK(count == 123);
    CHECK(reciv_read(-1, result, 65535) == -1);
    CHECK(reciv_write(-1, source, 65535) == -1);

    snprintf(path, sizeof path, "%s/BYTE-COUNT.BIN", argv[1]);
    CHECK(remove(path) == 0);
    puts("DOS reads, writes and truncation: 0..65535-byte boundaries passed");
    return 0;
}
