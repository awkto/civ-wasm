/* platform.h: reciv's host services, shared by src/crt and src/platform.
 * Modern C; period values cross as int16_t/uint16_t/int32_t. */
#ifndef RECIV_PLATFORM_H
#define RECIV_PLATFORM_H
#include <stdint.h>
#include <stdio.h>

/* Diagnostics. reciv_fatal does not return. */
#ifdef __GNUC__
#define RECIV_PRINTF __attribute__((format(printf, 1, 2)))
#else
#define RECIV_PRINTF
#endif
void reciv_log(const char *fmt, ...) RECIV_PRINTF;
_Noreturn void reciv_fatal(const char *fmt, ...) RECIV_PRINTF;
_Noreturn void reciv_unimplemented(const char *what);
_Noreturn void reciv_exit(int status);

/* Files. The game names files as on DOS ("SP299.PIC", "civil0.sve");
 * reads look in the save directory, then the game directory, ignoring
 * case; writes go to the save directory. */
void reciv_set_dirs(const char *game_dir, const char *save_dir);
FILE *reciv_fopen(const char *name, const char *mode);
int16_t reciv_open(const char *name, uint16_t msc_flags);
int16_t reciv_create(const char *name);
int16_t reciv_close(int16_t fd);
/* Host results must distinguish all 0..65535 byte counts from -1. */
int32_t reciv_read(int16_t fd, void *buf, uint16_t n);
int32_t reciv_write(int16_t fd, const void *buf, uint16_t n);
int32_t reciv_lseek(int16_t fd, int32_t offset, int16_t whence);

/* DOS returns status separately from its unsigned 16-bit byte count. */
uint16_t crt_dos_read(int16_t fd, void *buf, uint16_t n, uint16_t *count);
uint16_t crt_dos_write(int16_t fd, const void *buf, uint16_t n, uint16_t *count);
#endif
