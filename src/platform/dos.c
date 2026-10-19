/* dos.c: the DOS and BIOS services the game calls (memory is in mem.c).
 *
 * Interrupts. int86 handles the BIOS video calls the game makes (set
 * mode, cursor position, one DAC colour); intdos handles DOS 42h (seek). */
#include <stdlib.h>
#include <string.h>

#include "reciv.h"

/* ---------------------------------------------------------- DOS files */

uint16_t crt_dos_open(const char *name, uint16_t mode, int16_t *handle)
{
    int16_t fd = reciv_open(name, (uint16_t) (mode & 3));
    if (fd < 0)
        return 2;       /* file not found */
    *handle = fd;
    return 0;
}

uint16_t crt_dos_creat(const char *name, uint16_t attrib, int16_t *handle)
{
    int16_t fd = reciv_create(name);
    (void) attrib;
    if (fd < 0)
        return 5;       /* access denied */
    *handle = fd;
    return 0;
}

uint16_t crt_dos_close(int16_t handle)
{
    return reciv_close(handle) ? 6 : 0;
}

void crt_dos_getdrive(uint16_t *drive)
{
    *drive = 3;         /* C: */
}

/* The game installs Ctrl-Break handlers; nothing to do on the host. */
static void *vectors[256];

void *crt_dos_getvect(int16_t n)
{
    return vectors[n & 0xFF];
}

void crt_dos_setvect(int16_t n, void *handler)
{
    vectors[n & 0xFF] = handler;
}

/* The disk BIOS: the save code verifies and resets floppy drives. */
uint16_t crt_bios_disk(uint16_t service, void *info)
{
    (void) service;
    (void) info;
    return 0;
}

/* ---------------------------------------------------------- interrupts */

void reciv_text_write(uint8_t c, uint8_t attribute, int count);
void reciv_text_teletype(uint8_t c);
int reciv_video_mode(void);

int16_t crt_int86(int16_t n, union reciv_regs *in, union reciv_regs *out)
{
    union reciv_regs r = *in;
    switch ((n << 8) | r.h.ah) {
    case 0x1000:        /* set video mode AL */
        reciv_video_set_mode(r.h.al);
        break;
    case 0x1002:        /* set cursor position DH, DL */
        reciv_text_cursor(r.h.dh, r.h.dl);
        break;
    case 0x1009:        /* character AL, attribute BL, CX times */
        reciv_text_write(r.h.al, r.h.bl, r.x.cx);
        break;
    case 0x100E:        /* teletype AL */
        reciv_text_teletype(r.h.al);
        break;
    case 0x100F:        /* get video mode: AL mode, AH columns, BH page */
        r.h.al = (uint8_t) reciv_video_mode();
        r.h.ah = r.h.al == 0x13 ? 40 : 80;
        r.h.bh = 0;
        break;
    case 0x1010:        /* AL 10h: set DAC register BX to DH, CH, CL */
        if (r.h.al == 0x10)
            reciv_dac_set(r.x.bx, r.h.dh, r.h.ch, r.h.cl);
        break;
    default:
        reciv_log("int86(%02Xh) AX=%04X not handled", n, r.x.ax);
    }
    r.x.cflag = 0;
    *out = r;
    return (int16_t) r.x.ax;
}

int16_t crt_intdos(union reciv_regs *in, union reciv_regs *out)
{
    union reciv_regs r = *in;
    if (r.h.ah == 0x42) {       /* seek: BX handle, CX:DX offset, AL whence */
        int32_t pos = reciv_lseek((int16_t) r.x.bx, (int32_t) ((uint32_t) r.x.cx << 16 | r.x.dx), r.h.al);
        r.x.cflag = pos < 0;
        r.x.ax = (uint16_t) pos;
        r.x.dx = (uint16_t) ((uint32_t) pos >> 16);
    } else {
        reciv_log("intdos AX=%04X not handled", r.x.ax);
        r.x.cflag = 1;
    }
    *out = r;
    return (int16_t) r.x.ax;
}
