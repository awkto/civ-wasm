/* dosemu.h: host-side view of the DOS machine the game expects. */
#ifndef RECIV_DOSEMU_H
#define RECIV_DOSEMU_H
#include <stdint.h>

/* The game's union REGS (MSC dos.h), with 16-bit fields. */
union reciv_regs {
    struct { uint16_t ax, bx, cx, dx, si, di, cflag; } x;
    struct { uint8_t al, ah, bl, bh, cl, ch, dl, dh; } h;
};

/* Segments: real-mode segments, or host blocks from C000h up (mem.c). */
uint16_t reciv_seg_wrap(void *base, uint32_t size);
void *reciv_seg_ptr(uint16_t seg, uint16_t off);

/* BIOS video services, implemented by the display. */
void reciv_video_set_mode(int mode);
void reciv_text_cursor(int row, int column);
void reciv_text_teletype(uint8_t c);
void reciv_dac_set(int index, int red, int green, int blue);
#endif
