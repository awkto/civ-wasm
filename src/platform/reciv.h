/* reciv.h: shared declarations of the platform layer (host C).
 *
 * The platform stands in for the DOS machine: real-mode memory, DOS's
 * memory allocator, the VGA, the timer interrupt, the keyboard and mouse,
 * and the resident assembly and drivers the game calls. Values cross to and
 * from the game as int16_t (the game's int) and int32_t (its long). */
#ifndef RECIV_RECIV_H
#define RECIV_RECIV_H
#include <stdint.h>

#include "platform.h"
#include "dosemu.h"

/* ------------------------------------------------------------- memory */

/* The real-mode address space (1 MB, plus the 64 KB a segment can reach
 * past it). Conventional memory, the BIOS data area at 0040:0000 and the
 * VGA window at A000:0000 are all here. */
extern uint8_t reciv_mem[0x110000];
#define REAL(seg, off) (reciv_mem + ((uint32_t) (uint16_t) (seg) << 4) + (uint16_t) (off))
#define VGA_SEG 0xA000
#define SCREEN (reciv_mem + 0xA0000)

static inline uint16_t peek16(const uint8_t *p) { return (uint16_t) (p[0] | p[1] << 8); }
static inline void poke16(uint8_t *p, uint16_t v) { p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); }

/* DOS memory (functions 48h, 49h, 4Ah) over a chain of memory control
 * blocks in reciv_mem, as DOS keeps them. Return DOS error codes (0, 7, 8,
 * 9); on error 8 *max holds the largest block available. */
void reciv_mem_init(void);
uint16_t reciv_dos_alloc(uint16_t paragraphs, uint16_t *seg, uint16_t *max);
uint16_t reciv_dos_free(uint16_t seg);
uint16_t reciv_dos_resize(uint16_t seg, uint16_t paragraphs, uint16_t *max);

/* ------------------------------------------------------------- video */

extern uint8_t reciv_dac[256][3];        /* 6-bit RGB */
void reciv_video_open(void);
int reciv_video_filter(const char *name); /* select before open; zero if unknown */
int reciv_video_text_filter(const char *name); /* optional split text presentation */
void reciv_video_present(int force);
void reciv_video_dirty(void);
void reciv_set_start_address(uint16_t start);  /* CRTC 0Ch/0Dh */
const uint8_t *reciv_page_background(uint16_t page);
const uint8_t *reciv_page_text_mask(uint16_t page);
/* civ-wasm: the WinCiv skin (skin.c, mgraphic.c). */
int reciv_skin_load(const char *path);
void reciv_skin_enable(void);
/* civ-wasm: skin colours the sheet leaves unused (72 to 126), for WinCiv's
 * city screen (CITYMGT.C, WIDE.H): its cream panels and dark red frame. */
#define RECIV_SKIN_PANEL 80
#define RECIV_SKIN_FRAME 81
void reciv_skin_row(uint16_t page, int x, int y, int w, const uint8_t *row);
const uint8_t *reciv_page_skin(uint16_t page);
extern int reciv_skin_colours;
extern uint32_t reciv_skin_palette[128];
extern uint8_t reciv_skin_washed[128];
uint16_t reciv_fade(uint8_t rgb[3]);
void reciv_fade_clear(void);
int reciv_cycle_step(int16_t n);
void reciv_page_layers_reset(uint16_t page);
/* civ-wasm: the size of the screen and of every page (default 320 x 200). */
extern int reciv_screen_w, reciv_screen_h;
void reciv_pages_init(void);
void reciv_page_wide(uint16_t page, int on);  /* the page holds the wide map view */
uint8_t *reciv_screen_pixels(void);

/* ------------------------------------------------------------- time */

/* The timer interrupt, run cooperatively: reciv_poll runs the ticks that
 * are due by the real clock, pumps window events and presents the screen.
 * Every routine that waits for time or input calls it. */
void reciv_timer_init(void);
void reciv_poll(void);
void reciv_wait(void);           /* reciv_poll, then sleep briefly */
void reciv_spin(void);           /* reciv_poll for a polling query */
uint32_t reciv_vsync_count(void);
extern int reciv_anim_speed;     /* civ-wasm: animation speed in tenths (10 = original) */

/* -------------------------------------------------------------- input */

void reciv_key_event(uint16_t code);             /* BIOS keystroke (scan << 8 | ascii) */
void reciv_mouse_event(int x, int y, int buttons);       /* 320x200 coordinates */

/* ------------------------------------------------- driver slot tables */

void reciv_mgraphic_install(uint16_t data_seg);
void reciv_misc_install(void);
uint64_t reciv_clock_ns(void);
void reciv_sound_install(const char *name, uint16_t data_segment, uint16_t data_size);
void reciv_require_slot(int slot);

/* Graphics driver entries used by the resident routines. */
void reciv_raster_page(uint16_t page);
void reciv_raster_target(const int16_t *page);
void reciv_raster_colour(uint8_t colour, uint8_t op);
void reciv_raster_plot(int16_t x, int16_t y);
void reciv_raster_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void reciv_raster_spans(int16_t first, int16_t last, const int16_t *edges);
void reciv_cursor_draw(uint16_t page);
void reciv_cursor_erase(void);
void reciv_cursor_set(int16_t x, int16_t y, uint16_t sprite);

/* Resident state the driver gateway shares with the mouse code. */
extern uint8_t reciv_dr_pending, reciv_dr_deferred;
void reciv_mouse_driver_poll(void);
void reciv_dac_write(int index, int count, const uint8_t *rgb);
void reciv_dac_read(int index, int count, uint8_t *rgb);

/* Sound driver timer entries (slots 103, 104). */
int16_t reciv_sound_tick(void);
void reciv_sound_fast_tick(void);
/* Make the AdLib's samples up to clock time t (reciv_clock_ns). */
void reciv_sound_render(uint64_t t);
#endif
