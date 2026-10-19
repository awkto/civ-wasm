/* io.h: MSC 5.1 low-level file I/O (crt_* names, see crtnames.h). */
#ifndef RECIV_IO_H
#define RECIV_IO_H
int open(const char *, int, ...);
int read(int, void *, unsigned int);
int write(int, const void *, unsigned int);
int close(int);
long lseek(int, long, int);
#endif
