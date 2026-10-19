/* dos.h: MSC 5.1 declarations. DOS and BIOS services are emulated by
 * src/platform; segments are handles into its segment table. */
#ifndef RECIV_DOS_H
#define RECIV_DOS_H
#include "crtdefs.h"

struct WORDREGS { unsigned int ax, bx, cx, dx, si, di, cflag; };
struct BYTEREGS { unsigned char al, ah, bl, bh, cl, ch, dl, dh; };
union REGS { struct WORDREGS x; struct BYTEREGS h; };
struct SREGS { unsigned int es, cs, ss, ds; };


int int86(int, union REGS *, union REGS *);
int intdos(union REGS *, union REGS *);
unsigned int _dos_open(const char *, unsigned int, int *);
unsigned int _dos_creat(const char *, unsigned int, int *);
unsigned int _dos_close(int);
unsigned int _dos_read(int, void *, unsigned int, unsigned int *);
unsigned int _dos_write(int, const void *, unsigned int, unsigned int *);
unsigned int _dos_freemem(unsigned int);
void _dos_getdrive(unsigned int *);

#define FP_SEG(p) crt_fp_seg((void *) (p))
#define FP_OFF(p) crt_fp_off((void *) (p))
#define MK_FP(s, o) crt_mk_fp((s), (o))
unsigned int crt_fp_seg(void *);
unsigned int crt_fp_off(void *);
void *crt_mk_fp(unsigned int, unsigned int);
#endif
