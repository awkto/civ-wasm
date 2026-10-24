/* mgraphic.c: MGRAPHIC.EXE, the MCGA/VGA graphics driver, in C.
 *
 * Ported from civd's src/MGRAPHIC.ASM routine by routine; the comments
 * name the slot and the assembly label. The driver's file is still loaded
 * into memory (loader.c), and its data segment there holds the default
 * palette and, appended by the loader, FONTS.CV; this code reads those
 * from it as the assembly did, including the font table quirk of
 * font_address. The driver's variables are C statics.
 *
 * Pages are 64,000-byte blocks of real-mode memory (page 0 is the VGA at
 * A000h). A page record (GRAPHICS.H struct page) is nine words: number,
 * left, top, right, bottom, active, colour, text colour, font. */
#include <stdlib.h>
#include <string.h>

#include "reciv.h"

struct page16 {
    int16_t number, left, top, right, bottom, active, colour, text_colour, font;
};

static uint16_t data_seg;                       /* the driver's DATA paragraph */
#define DRV(off) REAL(data_seg, off)
#define DEFAULT_PALETTE 0x50
#define FONTS 0x630

static uint16_t page_segs[8] = { VGA_SEG, VGA_SEG, VGA_SEG, VGA_SEG, VGA_SEG, VGA_SEG, VGA_SEG, VGA_SEG };
/* civ-wasm: pages are host buffers of reciv_screen_w x reciv_screen_h
 * pixels (320 x 200 unless --screen asks for more), addressed by 32-bit
 * offsets. The DOS blocks the game allocates for them are kept but not
 * drawn in. An offset outside the page, which wrapped within the segment
 * on DOS, reaches a scratch byte. page_mem[n] is page 0's buffer until the
 * game gives page n memory of its own, as page_segs pointed at the VGA. */
int reciv_screen_w = 320, reciv_screen_h = 200;
static uint8_t *page_mem[8];
static uint8_t *page_own[8];
static uint8_t page_scratch;
/* Presentation-only layer state.  The game's page remains the authoritative
 * composited image.  background_pages retains what is below transparent text,
 * while text_pages marks pixels whose last visible writer was a glyph. */
static uint8_t *background_pages[8];
static uint8_t *text_pages[8];
static uint8_t layer_scratch[2];
static uint16_t raster_page;
static int16_t raster_left, raster_top, raster_right = 319;
static uint8_t raster_colour_value, raster_op;

#define SW ((uint32_t) reciv_screen_w)
#define PAGE_BYTES ((uint32_t) reciv_screen_w * (uint32_t) reciv_screen_h)

void reciv_pages_init(void)
{
    for (int i = 0; i < 8; i++) {
        background_pages[i] = calloc(PAGE_BYTES, 1);
        text_pages[i] = calloc(PAGE_BYTES, 1);
    }
    page_own[0] = calloc(PAGE_BYTES, 1);
    if (!page_own[0] || !background_pages[7] || !text_pages[7])
        reciv_fatal("no memory for %dx%d pages", reciv_screen_w, reciv_screen_h);
    for (int i = 0; i < 8; i++)
        page_mem[i] = page_own[0];
}

uint8_t *reciv_screen_pixels(void)
{
    return page_mem[0];
}

static uint8_t *page_at(uint16_t page, uint32_t offset)
{
    return offset < PAGE_BYTES ? page_mem[page & 7] + offset : &page_scratch;
}

static uint8_t *background_at(uint16_t page, uint32_t offset)
{
    return offset < PAGE_BYTES ? background_pages[page & 7] + offset : &layer_scratch[0];
}

static uint8_t *text_at(uint16_t page, uint32_t offset)
{
    return offset < PAGE_BYTES ? text_pages[page & 7] + offset : &layer_scratch[1];
}

/* civ-wasm: whether each page holds a wide picture (the map) or a classic
 * 320x200 one; the screen's flag picks what video.c shows. */
static uint8_t page_wide[8];
extern int reciv_wide_view;

void reciv_page_wide(uint16_t page, int on)
{
    page_wide[page & 7] = (uint8_t) on;
    if (page_mem[page & 7] == page_mem[0] && reciv_wide_view != on) {
        page_wide[0] = (uint8_t) on;
        reciv_wide_view = on;
        reciv_video_dirty();
    }
}

/* The game's call (WIDE.H): the map was drawn on the screen record's page. */
struct page16;
extern struct page16 *scrbuf;
void wide_view(int16_t on)
{
    reciv_page_wide((uint16_t) *(int16_t *) scrbuf, on);
}

static uint32_t row_offset(int16_t y)
{
    return (uint32_t) ((int32_t) y * (int32_t) reciv_screen_w);
}

/* The store instruction raster_colour patches: MOV, AND, OR or XOR. */
static uint8_t raster_result(uint8_t old, uint8_t colour)
{
    switch (raster_op) {
    case 0: return colour;
    case 1: return old & colour;
    case 2: return old | colour;
    default: return old ^ colour;
    }
}

static void store_nontext(uint16_t page, uint32_t offset, uint8_t colour)
{
    uint8_t *pixel = page_at(page, offset);
    uint8_t result = raster_result(*pixel, colour);
    *pixel = result;
    *background_at(page, offset) = result;
    *text_at(page, offset) = 0;
}

static void store_text(uint16_t page, uint32_t offset, uint8_t colour)
{
    uint8_t *pixel = page_at(page, offset);
    *pixel = raster_result(*pixel, colour);
    *text_at(page, offset) = 1;
}

const uint8_t *reciv_page_background(uint16_t page)
{
    return background_pages[page & 7];
}

const uint8_t *reciv_page_text_mask(uint16_t page)
{
    return text_pages[page & 7];
}

void reciv_page_layers_reset(uint16_t page)
{
    page &= 7;
    memcpy(background_pages[page], page_mem[page], PAGE_BYTES);
    memset(text_pages[page], 0, PAGE_BYTES);
}

void reciv_mgraphic_install(uint16_t seg)
{
    data_seg = seg;
}

/* ------------------------------------------------------------ palette */

/* dac_write: the assembly spreads the writes over vertical retraces. */
static void dac_write(int index, int count, const uint8_t *rgb)
{
    reciv_dac_write(index, count, rgb);
}

static void palette_reset(void)
{
    dac_write(0, 0x80, DRV(DEFAULT_PALETTE));
}

/* Slot 28: load colours rec[4]..rec[5] from an "M0" palette record. The
 * record is a near pointer; a null one reads DGROUP's first bytes, MSC's
 * null-pointer check area, which is never a record. */
void apply_palette(const uint8_t *record)
{
    reciv_require_slot(28);
    if (!record || record[0] != 'M' || record[1] != '0')
        return;
    dac_write(record[4], (uint16_t) (record[5] - record[4] + 1), record + 6);
}

/* Slot 27: set the 16 EGA colours from an EGA palette (16 default-palette
 * numbers); byte 16, if it is one of them, also picks the border. */
void load_ega_palette(const uint8_t *ega)
{
    uint8_t colours[48];
    reciv_require_slot(27);
    for (int i = 0; i < 16; i++)
        memcpy(colours + i * 3, DRV(DEFAULT_PALETTE + ega[i] * 3), 3);
    memcpy(DRV(0x1D0), colours, 48);    /* ega_colours */
    dac_write(0, 16, colours);
}

/* ------------------------------------------------------------- pages */

/* Slot 0: segment for page n: the screen for 0, else a new 64,000-byte
 * block (0 if there is no memory). */
int16_t alloc_page_memory(int16_t n)
{
    uint16_t seg, max;
    reciv_require_slot(0);
    if (n == 0)
        return (int16_t) page_segs[0];
    return reciv_dos_alloc(0xFA0, &seg, &max) ? 0 : (int16_t) seg;
}

/* Slot 1: the largest free DOS block, in paragraphs. */
int16_t free_paragraphs(void)
{
    uint16_t seg, max = 0;
    reciv_require_slot(1);
    if (!reciv_dos_alloc(0xFFFF, &seg, &max))
        reciv_fatal("allocated 1 MB");
    return (int16_t) max;
}

/* Slot 25: use `seg` for page n. */
void set_page_memory(int16_t n, int16_t seg)
{
    reciv_require_slot(25);
    page_segs[n & 7] = (uint16_t) seg;
    if ((uint16_t) seg == VGA_SEG)
        page_mem[n & 7] = page_own[0];
    else {
        if (!page_own[n & 7] && !(page_own[n & 7] = calloc(PAGE_BYTES, 1)))
            reciv_fatal("no memory for page %d", n);
        page_mem[n & 7] = page_own[n & 7];
    }
    reciv_page_layers_reset((uint16_t) n);
}

/* Slot 17: a status word, read from byte n of the status table. */
int16_t page_ready(int16_t n)
{
    static const uint8_t status[10] = { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t bytes[26];
    reciv_require_slot(17);
    memcpy(bytes, status, 10);
    for (int i = 0; i < 8; i++)
        poke16(bytes + 10 + i * 2, page_segs[i]);
    return (uint16_t) n < 25 ? (int16_t) peek16(bytes + n) : 0;
}

/* Slot 14: colour of one pixel. */
int16_t pixel_at(int16_t page, int16_t x, int16_t y)
{
    reciv_require_slot(14);
    return *page_at((uint16_t) page, row_offset(y) + (uint32_t) x);
}

/* Slot 13: copy part of a page row into a near buffer. */
void page_read_row(uint8_t *buffer, int16_t page, int16_t x, int16_t y, int16_t n)
{
    uint32_t off = row_offset(y) + (uint32_t) x;
    reciv_require_slot(13);
    for (uint16_t i = 0; i < (uint16_t) n; i++)
        buffer[i] = *page_at((uint16_t) page, off + i);
}

/* Slot 19: copy a near buffer into a page row, a word at a time. */
void draw_scanline(const uint8_t *buffer, int16_t page, int16_t x, int16_t y, int16_t n)
{
    uint32_t off = row_offset(y) + (uint32_t) x;
    uint16_t bytes = (uint16_t) (((uint16_t) n + 1) >> 1) * 2;
    reciv_require_slot(19);
    if (n == -1)
        bytes = 0;              /* (FFFFh + 1) >> 1: no words */
    if (!x && !y && n >= 320)
        reciv_page_wide((uint16_t) page, 0);    /* a full-screen picture */
    for (uint16_t i = 0; i < bytes; i++) {
        uint32_t at = off + i;
        *page_at((uint16_t) page, at) = buffer[i];
        *background_at((uint16_t) page, at) = buffer[i];
        *text_at((uint16_t) page, at) = 0;
    }
}

/* Slot 3: in a rectangle, replace colour `from` with `to`. */
void recolour_rect(const struct page16 *pg, int16_t x, int16_t y, int16_t w, int16_t h, int16_t from, int16_t to)
{
    uint32_t di = row_offset((int16_t) (y + pg->top)) + (uint32_t) (int32_t) (x + pg->left);
    uint16_t rows = (uint16_t) h;
    reciv_require_slot(3);
    do {
        uint16_t cx = (uint16_t) w;
        do {
            uint8_t *p = page_at((uint16_t) pg->number, di);
            if (*p == (uint8_t) from) {
                *p = (uint8_t) to;
                /* Recolouring transforms a visible glyph, but not the hidden
                 * background that preceded it. */
                if (!*text_at((uint16_t) pg->number, di))
                    *background_at((uint16_t) pg->number, di) = (uint8_t) to;
            }
            di++;
        } while (--cx);
        di = di + SW - (uint16_t) w;
    } while (--rows);
}

/* Slot 20: copy a rectangle from one page to another. */
void blit(const struct page16 *src, int16_t sx, int16_t sy, int16_t w, int16_t h,
          const struct page16 *dst, int16_t dx, int16_t dy)
{
    uint32_t si, di;
    uint16_t rows;
    reciv_require_slot(20);
    /* civ-wasm: a copy that reaches the classic screen's right and bottom
     * edges at the same place on both pages (saving or restoring the
     * screen, or the map area) reaches the larger screen's edges. */
    if (sx == dx && sy == dy && src->left == dst->left && src->top == dst->top &&
        src->left + sx + w == 320 && src->top + sy + h == 200 && w >= 200 && h >= 100) {
        w = (int16_t) (reciv_screen_w - src->left - sx);
        h = (int16_t) (reciv_screen_h - src->top - sy);
        if (src->left + sx <= 0 && src->top + sy <= 0)
            reciv_page_wide((uint16_t) dst->number, page_wide[src->number & 7]);
    }
    si = row_offset((int16_t) (src->top + sy)) + (uint32_t) (int32_t) (src->left + sx);
    di = row_offset((int16_t) (dst->top + dy)) + (uint32_t) (int32_t) (dst->left + dx);
    rows = (uint16_t) h;
    if (!w || !h)
        return;
    do {
        uint16_t sp = (uint16_t) src->number & 7, dp = (uint16_t) dst->number & 7;
        for (uint16_t i = 0; i < (uint16_t) w; i++) {
            uint32_t so = si + i, dest = di + i;
            *page_at(dp, dest) = *page_at(sp, so);
            *background_at(dp, dest) = *background_at(sp, so);
            *text_at(dp, dest) = *text_at(sp, so);
        }
        si += SW;
        di += SW;
    } while (--rows);
}

/* ---------------------------------------------------- drawing gateway */

/* DRIVER.INC's deferred slots (DRCALL): while the mouse cursor is shown,
 * a call marks the driver busy, and any mouse movement during it redraws
 * the cursor afterwards (mouse_driver_poll). */
#define DRCALL(call) do { \
        if (!reciv_dr_deferred) { call; break; } \
        reciv_dr_pending = 1; \
        call; \
        while (--reciv_dr_pending) \
            reciv_mouse_driver_poll(); \
    } while (0)

/* Slot 31 (set_page): draw the cursor on page n, then copy page n to the
 * screen. */
static void set_page_now(int16_t n)
{
    if (!n)
        return;
    reciv_cursor_draw((uint16_t) n);
    reciv_page_wide(0, page_wide[n & 7]);
    memmove(page_mem[0], page_mem[n & 7], PAGE_BYTES);
    memmove(background_pages[0], background_pages[n & 7], PAGE_BYTES);
    memmove(text_pages[0], text_pages[n & 7], PAGE_BYTES);
    reciv_video_dirty();
}

void set_page(int16_t n)
{
    reciv_require_slot(31);
    DRCALL(set_page_now(n));
}

/* --------------------------------------------------------- dissolve */

/* Slot 10: copy page n to the screen a pixel at a time, in the order of a
 * 16-bit Galois LFSR (taps B400h), so the whole copy takes `time`. The
 * assembly paces the copy by calibrated busy loops so that 8800 / time
 * pixels go per video frame; reciv shows the copy frame by frame at that
 * rate. Time 0 copies at full speed. */
void page_dissolve(int16_t n, int16_t time)
{
    uint16_t si = 1, per_frame;
    uint8_t *src, *dst;
    uint32_t frame;
    reciv_require_slot(10);
    /* Wait for a BIOS tick: the diskette motor count runs down at 18.2 Hz. */
    *REAL(0x40, 0x40) = 1;
    while (*REAL(0x40, 0x40) == 1)
        reciv_wait();
    if (!n)
        return;
    src = page_mem[n & 7];
    dst = page_mem[0];
    reciv_page_wide(0, page_wide[n & 7]);
    per_frame = time ? (uint16_t) (0x2260 / (uint16_t) time) : 0;
    if (time && !per_frame)
        per_frame = 1;
    frame = reciv_vsync_count();
    for (uint16_t left = 0xFA00, done = 0; left; left--) {
        do {
            uint16_t carry = si & 1;
            si >>= 1;
            if (carry)
                si ^= 0xB400;
        } while (si > 0xFA00);
        {
            /* civ-wasm: cell si - 1 of the 320 x 200 order covers a block
             * of a larger screen. */
            uint32_t cx = (uint32_t) (si - 1) % 320, cy = (uint32_t) (si - 1) / 320;
            uint32_t x0 = cx * SW / 320, x1 = (cx + 1) * SW / 320;
            uint32_t y0 = cy * (uint32_t) reciv_screen_h / 200, y1 = (cy + 1) * (uint32_t) reciv_screen_h / 200;
            for (uint32_t yy = y0; yy < y1; yy++)
                for (uint32_t xx = x0; xx < x1; xx++) {
                    uint32_t at = yy * SW + xx;
                    dst[at] = src[at];
                    background_pages[0][at] = background_pages[n & 7][at];
                    text_pages[0][at] = text_pages[n & 7][at];
                }
        }
        if (per_frame && ++done >= per_frame) {
            done = 0;
            reciv_video_dirty();
            while (reciv_vsync_count() == frame)
                reciv_wait();
            frame = reciv_vsync_count();
        }
    }
    reciv_video_dirty();
}

/* ------------------------------------------------------------- sprites */

/* A sprite: width, height, then per row a count n, a lead of transparent
 * pixels, and n pixels (0 transparent). Sprites are built in an arena, the
 * largest free DOS block, a paragraph-aligned sprite after another. */
static uint16_t arena_next, arena_seg, arena_base, arena_end;
static uint16_t sprite_room, sprite_rows, sprite_width;

/* Slot 18 */
void sprite_arena_init(void)
{
    uint16_t paras = (uint16_t) free_paragraphs(), seg = 0, max;
    reciv_require_slot(18);
    if (!paras || reciv_dos_alloc(paras, &seg, &max)) {
        seg = 0;
        paras = 0;
    }
    arena_next = 0;
    arena_seg = seg;
    arena_base = seg;
    arena_end = (uint16_t) (paras + seg);
}

/* Slot 7: shrink the arena to what was used, or free it if unused. */
int16_t sprite_arena_release(void)
{
    uint16_t base = arena_base, used = (uint16_t) (arena_seg - arena_base), result = base, max;
    reciv_require_slot(7);
    if (arena_seg < arena_base)
        result = 0;
    else if ((used ? reciv_dos_resize(base, used, &max) : reciv_dos_free(base)) != 0)
        result = 0;
    arena_seg = arena_end = arena_base = 0;
    return (int16_t) result;
}

/* Slot 35: start a sprite of w x h in the arena; returns its segment, or
 * 0 if the arena is full or missing. */
int16_t sprite_begin(int16_t w, int16_t h)
{
    uint16_t room = (uint16_t) (arena_end - arena_seg);
    reciv_require_slot(35);
    if (arena_end <= arena_seg) {
        arena_next = 0;
        arena_seg = 0;
        return 0;
    }
    if (room >= 0x1000)
        room = 0xFFF;
    sprite_room = (uint16_t) (room << 4);
    poke16(REAL(arena_seg, arena_next), (uint16_t) w);
    sprite_width = (uint16_t) w;
    poke16(REAL(arena_seg, arena_next + 2), (uint16_t) h);
    sprite_rows = (uint16_t) h;
    arena_next = (uint16_t) (arena_next + 4);
    return (int16_t) arena_seg;
}

/* Slot 33: finish the sprite; returns its segment and moves the arena on
 * to the next paragraph. */
int16_t sprite_end(void)
{
    uint16_t used = arena_next, seg = arena_seg;
    reciv_require_slot(33);
    arena_next = 0;
    arena_seg = (uint16_t) ((uint16_t) (used + 15) >> 4);   /* ADD AX,0FH drops the carry */
    arena_seg = (uint16_t) (arena_seg + seg);
    return (int16_t) seg;
}

/* Slot 34: encode one row, trimming transparent pixels at both ends. */
int16_t sprite_encode_row(const uint8_t *row)
{
    uint16_t di = arena_next, start, lead = 0, trail = 0, run = 0;
    reciv_require_slot(34);
    if (!arena_seg)
        return 0;
    if ((uint16_t) (sprite_width + 4 + di) > sprite_room) {
        /* No room: drop the sprite's last row and close the arena. */
        poke16(REAL(arena_seg, 2), (uint16_t) (peek16(REAL(arena_seg, 2)) - 1));
        arena_end = 0;
        return 0;
    }
    start = di;
    di = (uint16_t) (di + 4);
    for (uint16_t cx = sprite_width; cx; cx--) {
        uint8_t c = *row++;
        if (c)
            trail = 0;
        else if (!run) {
            lead++;
            continue;
        } else
            trail++;
        run++;
        *REAL(arena_seg, di++) = c;
    }
    if (trail) {
        run = (uint16_t) (run - trail);
        di = (uint16_t) (di - trail);
    }
    poke16(REAL(arena_seg, start), run);
    poke16(REAL(arena_seg, start + 2), lead);
    arena_next = di;
    return (int16_t) arena_seg;
}

/* Slot 2: encode a page rectangle as a sprite; returns its segment. */
int16_t cut_tile(int16_t page, int16_t x, int16_t y, int16_t w, int16_t h)
{
    reciv_require_slot(2);
    if (sprite_begin(w, h) && sprite_rows && sprite_width) {
        uint32_t si = row_offset(y) + (uint32_t) x;
        uint16_t number = (uint16_t) (page < 0 ? -page : page);
        do {
            uint8_t row[0x10000 / 16];
            for (uint16_t i = 0; i < sprite_width && i < sizeof row; i++)
                row[i] = *page_at(number, si + i);
            sprite_encode_row(row);
            si += SW;
        } while (--sprite_rows);
    }
    return sprite_end();
}

static int16_t clip_top, clip_left, clip_bottom, clip_right;
static int16_t dst_width, dst_height;
static int16_t drawn_rect[4];

/* sprite_prepare: clear the clip state; false when the sprite is empty. */
static int sprite_prepare(uint16_t sprite)
{
    clip_top = clip_left = clip_bottom = clip_right = 0;
    memset(drawn_rect, 0, sizeof drawn_rect);
    return sprite && peek16(REAL(sprite, 0)) && peek16(REAL(sprite, 2));
}

/* sprite_clip: clip against the page's right and bottom (absolute values
 * compared with page-relative positions, as in the assembly) and the
 * page's origin; false when nothing is visible. */
static int sprite_clip(const struct page16 *pg, int16_t x, int16_t y)
{
    int16_t ax;
    if (y < 0) {
        if ((uint16_t) -y >= (uint16_t) dst_height)
            return 0;
        clip_top = (int16_t) -y;
    }
    ax = (int16_t) (y + dst_height - 1);
    if (pg->bottom < 0 || y > pg->bottom)
        return 0;
    ax = (int16_t) (ax - pg->bottom);
    if (ax > 0)
        clip_bottom = ax;
    if (x < 0) {
        if ((uint16_t) -x >= (uint16_t) dst_width)
            return 0;
        clip_left = (int16_t) -x;
    }
    ax = (int16_t) (x + dst_width - 1);
    if (pg->right < 0 || x > pg->right)
        return 0;
    ax = (int16_t) (ax - pg->right);
    if (ax > 0)
        clip_right = ax;
    drawn_rect[0] = (int16_t) (clip_left + x);
    drawn_rect[1] = (int16_t) (clip_top + y);
    drawn_rect[2] = (int16_t) (dst_width - clip_left - clip_right);
    drawn_rect[3] = (int16_t) (dst_height - clip_top - clip_bottom);
    return 1;
}

/* sprite_blit_rows: draw the clipped rows; 0 is transparent. */
static void sprite_blit_rows(const struct page16 *pg, int16_t x, int16_t y, uint16_t sprite)
{
    uint16_t page_number = (uint16_t) pg->number & 7;
    uint8_t *spr = REAL(sprite, 0);
    uint16_t si = 0, draw_cols, draw_rows;
    uint32_t bp;
    int16_t ax;
    x = (int16_t) (x + pg->left);
    y = (int16_t) (y + pg->top + clip_top);
    bp = row_offset(y) + (uint32_t) (int32_t) x;
    ax = (int16_t) (peek16(spr + si) - clip_right);
    si += 2;
    if (ax <= 0)
        return;
    draw_cols = (uint16_t) ax;
    ax = (int16_t) (peek16(spr + si) - clip_top - clip_bottom);
    si += 2;
    if (ax <= 0)
        return;
    draw_rows = (uint16_t) ax;
    for (int16_t skip = clip_top; skip > 0; skip--)
        si = (uint16_t) (si + peek16(spr + si) + 4);
    do {
        uint16_t n = peek16(spr + si), lead = peek16(spr + si + 2), cx, bx, dx;
        si = (uint16_t) (si + 4);
        dx = n;
        if (n) {
            bx = lead;
            cx = (uint16_t) (n + lead);
            if (!(lead > (uint16_t) clip_left)) {
                uint16_t d = (uint16_t) (clip_left - lead);
                si = (uint16_t) (si + d);
                bx = (uint16_t) clip_left;
                dx = (uint16_t) (dx - d);
            }
            if (cx > draw_cols)
                cx = draw_cols;
            if (cx > bx) {
                uint32_t di = bp + bx;
                cx = (uint16_t) (cx - bx);
                dx = (uint16_t) (dx - cx);
                while (cx--) {
                    uint8_t c = spr[si++];
                    if (c) {
                        *page_at(page_number, di) = c;
                        *background_at(page_number, di) = c;
                        *text_at(page_number, di) = 0;
                    }
                    di++;
                }
            }
        }
        bp += SW;
        si = (uint16_t) (si + dx);
    } while (--draw_rows);
}

/* Slot 6: draw a sprite clipped to the page; returns the rectangle drawn. */
static void sprite_draw_clipped_now(const struct page16 *pg, int16_t x, int16_t y, uint16_t sprite)
{
    if (sprite) {
        dst_width = (int16_t) peek16(REAL(sprite, 0));
        dst_height = (int16_t) peek16(REAL(sprite, 2));
    } else
        dst_width = dst_height = 0;
    if (sprite_prepare(sprite) && sprite_clip(pg, x, y))
        sprite_blit_rows(pg, x, y, sprite);
}

int16_t *sprite_draw_clipped(const struct page16 *pg, int16_t x, int16_t y, int16_t sprite)
{
    reciv_require_slot(6);
    DRCALL(sprite_draw_clipped_now(pg, x, y, (uint16_t) sprite));
    reciv_video_dirty();
    return drawn_rect;
}

/* Slot 32: draw a sprite without clipping. */
void sprite_draw(const struct page16 *pg, int16_t x, int16_t y, int16_t sprite)
{
    reciv_require_slot(32);
    DRCALL(if (sprite_prepare((uint16_t) sprite)) sprite_blit_rows(pg, x, y, (uint16_t) sprite));
    reciv_video_dirty();
}

/* -------------------------------------------------------- mouse cursor */

/* The cursor is a sprite drawn into the screen. The 512 bytes under it are
 * saved first, so a wide cursor saves fewer rows. */
static uint16_t cursor_sprite, cursor_page;
static int16_t cursor_x, cursor_y, under_x, under_y, under_width, under_height;
static int cursor_shown;
static uint8_t under_cursor[512];
static uint8_t under_cursor_background[512], under_cursor_text[512];

/* Slot 15 (cursor_restore, cursor_erase): put back what the cursor hid. */
void reciv_cursor_erase(void)
{
    if (cursor_shown) {
        uint32_t di = row_offset(under_y) + (uint32_t) under_x;
        uint16_t si = 0;
        for (int16_t r = under_height; r > 0; r--) {
            for (int16_t i = 0; i < under_width; i++) {
                uint32_t at = di + (uint32_t) i;
                uint16_t saved = si++ & 511;
                *page_at(0, at) = under_cursor[saved];
                *background_at(0, at) = under_cursor_background[saved];
                *text_at(0, at) = under_cursor_text[saved];
            }
            di += SW;
        }
        reciv_video_dirty();
    }
    cursor_shown = 0;
}

/* Slot 9: save what is under the cursor on page n and draw it there. */
void reciv_cursor_draw(uint16_t page)
{
    struct page16 clip = { 0, 0, 0, 319, 199, 0, 0, 0, 0 };
    uint32_t si;
    uint16_t di = 0;
    clip.right = (int16_t) (reciv_screen_w - 1);
    clip.bottom = (int16_t) (reciv_screen_h - 1);
    cursor_page = page;
    if (!cursor_shown)
        return;
    si = row_offset(under_y) + (uint32_t) under_x;
    for (int16_t r = under_height; r > 0; r--) {
        for (int16_t i = 0; i < under_width; i++) {
            uint32_t at = si + (uint32_t) i;
            uint16_t saved = di++ & 511;
            under_cursor[saved] = *page_at(page, at);
            under_cursor_background[saved] = *background_at(page, at);
            under_cursor_text[saved] = *text_at(page, at);
        }
        si += SW;
    }
    clip.number = (int16_t) cursor_page;
    sprite_draw_clipped_now(&clip, cursor_x, cursor_y, cursor_sprite);
    reciv_video_dirty();
}

/* Slot 30: set the cursor sprite and position, clip the saved area to
 * the screen, and draw it on the screen. */
void reciv_cursor_set(int16_t x, int16_t y, uint16_t sprite)
{
    uint16_t w, h, limit, cx;
    int16_t ax;
    cursor_sprite = sprite;
    cursor_x = x;
    w = peek16(REAL(sprite, 0));
    if (!w)
        goto hidden;
    ax = x;
    if (ax < 0) {
        ax = (int16_t) -ax;
        if (w <= (uint16_t) ax)
            goto hidden;
        w = (uint16_t) (w - ax);
        ax = 0;
    }
    cx = (uint16_t) (reciv_screen_w - ax);
    if (!cx || (int16_t) ax > reciv_screen_w)
        goto hidden;
    if (w > cx)
        w = cx;
    under_width = (int16_t) w;
    under_x = ax;
    limit = (uint16_t) (0x200 / w);
    cursor_y = y;
    h = peek16(REAL(sprite, 2));
    if (!h)
        goto hidden;
    ax = y;
    if (ax < 0) {
        ax = (int16_t) -ax;
        if (h <= (uint16_t) ax)
            goto hidden;
        h = (uint16_t) (h - ax);
        ax = 0;
    }
    cx = (uint16_t) (reciv_screen_h - ax);
    if (!cx || (int16_t) ax > reciv_screen_h)
        goto hidden;
    if (h > cx)
        h = cx;
    if (h > limit)
        h = limit;
    under_height = (int16_t) h;
    under_y = ax;
    cursor_shown = 1;
    reciv_cursor_draw(0);
    return;
hidden:
    cursor_shown = 0;
}

/* ------------------------------------------------------------- raster */

/* Slot 41: colour and operation (0 set, 1 and, 2 or, 3 xor). */
void reciv_raster_colour(uint8_t colour, uint8_t op)
{
    raster_colour_value = colour;
    raster_op = op & 3;
}

/* Slot 48 */
void reciv_raster_page(uint16_t page)
{
    raster_page = page & 7;
}

/* Slot 49: target page record (origin and right edge). */
void reciv_raster_target(const int16_t *page)
{
    raster_left = page[1];
    raster_top = page[2];
    raster_right = page[3];
    reciv_raster_page((uint16_t) page[0]);
}

/* Slot 38 */
void reciv_raster_plot(int16_t x, int16_t y)
{
    x = (int16_t) (x + raster_left);
    y = (int16_t) (y + raster_top);
    store_nontext(raster_page, row_offset(y) + (uint32_t) (int32_t) x, raster_colour_value);
    reciv_video_dirty();
}

/* Slot 42: a line from x0,y0 to x1,y1 (Bresenham). */
void reciv_raster_line(int16_t ax, int16_t bx, int16_t cx, int16_t dx)
{
    uint32_t di, si, bp, step, minor;
    int16_t err;
    ax = (int16_t) (ax + raster_left);
    cx = (int16_t) (cx + raster_left);
    bx = (int16_t) (bx + raster_top);
    dx = (int16_t) (dx + raster_top);
    if ((uint16_t) bx > (uint16_t) dx) {
        int16_t t = ax; ax = cx; cx = t;
        t = bx; bx = dx; dx = t;
    }
    reciv_video_dirty();
    if (bx == dx && ax == cx) {
        store_nontext(raster_page, row_offset(bx) + (uint32_t) (int32_t) ax, raster_colour_value);
        return;
    }
    di = row_offset(bx) + (uint32_t) (int32_t) ax;
    si = 1;
    bp = SW;
    dx = (int16_t) (dx - bx);
    cx = (int16_t) (cx - ax);
    if (cx < 0) {
        cx = (int16_t) -cx;
        si = (uint32_t) -1;
    }
    step = si + bp;                     /* diagonal */
    if ((uint16_t) cx < (uint16_t) dx) {
        uint32_t t = si; si = bp; bp = t;
        int16_t u = cx; cx = dx; dx = u;
    }
    /* Straight lines: a run along the axis. */
    uint32_t run_step;
    if (cx == dx) {
        run_step = step;
    } else if (!cx) {
        run_step = bp;
    } else if (!dx) {
        run_step = si;
    } else {
        minor = bp;
        err = (int16_t) -(((uint16_t) cx + 1) >> 1);
        for (int16_t n = cx;; ) {
            store_nontext(raster_page, di, raster_colour_value);
            if (--n < 0)
                return;
            di += si;
            err = (int16_t) (err + dx);
            if (err >= 0) {
                err = (int16_t) (err - cx);
                di += minor;
            }
        }
    }
    /* hline_store: cx | dx + 1 pixels stepping bx. */
    for (uint16_t n = (uint16_t) ((cx | dx) + 1); n; n--) {
        store_nontext(raster_page, di, raster_colour_value);
        di += run_step;
    }
}

/* Slot 39: fill rows first..last between the edge tables (left x at
 * edges[y], right x at edges[reciv_screen_h + y]). A one-pixel span at x 0 or at the
 * right edge is skipped. */
void reciv_raster_spans(int16_t first, int16_t last, const int16_t *edges)
{
    if (first < 0 || first > last)
        return;
    for (int16_t y = first; y <= last; y++) {
        int16_t left = edges[y], right = edges[reciv_screen_h + y];
        uint16_t n = (uint16_t) (right - left);
        if ((uint16_t) right < (uint16_t) left)
            continue;
        if (n == 0 && (left == 0 || left == raster_right))
            continue;
        n++;
        uint32_t offset = row_offset(y) + (uint32_t) (int32_t) left;
        if (offset >= PAGE_BYTES)
            continue;
        if (n > PAGE_BYTES - offset)
            n = (uint16_t) (PAGE_BYTES - offset);
        memset(page_mem[raster_page] + offset, raster_colour_value, n);
        memset(background_pages[raster_page] + offset, raster_colour_value, n);
        memset(text_pages[raster_page] + offset, 0, n);
    }
    reciv_video_dirty();
}

/* --------------------------------------------------------------- text */

/* FONTS.CV, appended at data offset 630h: a font count, then each font's
 * offset (from 630h). Nine bytes before a font's glyph data hold its
 * metrics: first and last character, row width as a shift + 1, fixed
 * width (0: proportional, using the width table before the metrics),
 * height, spacing and extra rows. Glyph rows are big-endian bit rows. */
static uint8_t *font_metrics(int16_t font)
{
    if ((uint16_t) font > peek16(DRV(FONTS)) || !font)
        return NULL;
    return DRV(FONTS + peek16(DRV(FONTS + font * 2)));
}

/* Slot 4 */
int16_t char_width(int16_t font, int16_t c)
{
    uint8_t al = 0, ah = 0, *g;
    reciv_require_slot(4);
    if ((int8_t) c >= 0 && (g = font_metrics(font)) != NULL && (uint8_t) c >= g[-8] && (uint8_t) c <= g[-7]) {
        ah = g[-3];
        al = g[-5];
        if (!al)
            al = g[-9 - (g[-7] - (uint8_t) c)];
    }
    return (int8_t) (al + ah);
}

/* Slot 12 */
int16_t font_height(int16_t font)
{
    uint8_t *g;
    reciv_require_slot(12);
    g = font_metrics(font);
    return g ? (uint8_t) (g[-4] + g[-2]) : 0;
}

/* Slot 11: far address of a font's glyph data. The assembly checks the
 * font number against the word at data offset 20h and adds the word at
 * 20h + 2n, inside the copyright notice, where 630h + 2n was meant. */
uint8_t *font_address(int16_t font)
{
    reciv_require_slot(11);
    if ((uint16_t) font > peek16(DRV(0x20)) || !font)
        return REAL(0, 0);
    return REAL(data_seg, FONTS + peek16(DRV(0x20 + font * 2)));
}

static struct {
    uint8_t colour, background, opaque, width;
    uint8_t *glyphs;
    uint8_t *widths;
    uint16_t row_bytes;
    int16_t x, y;
    uint8_t fixed, spacing, shift, first, last, skip, cut;
    uint16_t rows;
    const char *cut_at;
} text;

/* text_prepare: metrics and position; false if there is nothing to draw. */
static int text_prepare(const struct page16 *pg, int16_t x, int16_t y, const char *s)
{
    uint8_t *g;
    text.x = x;
    text.y = y;
    text.cut_at = NULL;
    text.skip = text.cut = 0;
    if (!*s || !(g = font_metrics(pg->font)))
        return 0;
    text.glyphs = g;
    text.rows = (uint16_t) (int8_t) (g[-4] + ((pg->active & 1) ? g[-2] : 0));
    text.spacing = g[-3];
    text.fixed = g[-5] ? (uint8_t) (g[-5] + g[-3]) : 0;
    text.first = g[-8];
    text.last = g[-7];
    text.shift = (uint8_t) (g[-6] - 1);
    text.widths = g - 9 - (uint8_t) (text.last - text.first);
    text.row_bytes = (uint16_t) ((uint16_t) ((uint8_t) (text.last - text.first) + 1) << text.shift);
    return 1;
}

/* Advance of character c, or -1 if the font has no glyph for it. */
static int advance(uint8_t c)
{
    if (c & 0x80 || c > text.last || c < text.first)
        return -1;
    return text.fixed ? text.fixed : (uint8_t) (text.widths[c - text.first] + text.spacing);
}

/* text_cut_right: end the string at the character crossing the right
 * edge, drawing that one with cut_cols columns fewer. */
static int text_cut_right(const struct page16 *pg, const char *s)
{
    int16_t cx = pg->right;
    if (cx < 0)
        return 0;
    cx = (int16_t) (cx - text.x);
    if (cx < 0)
        return 0;
    cx++;
    for (; *s; s++) {
        int w = advance((uint8_t) *s);
        if (w < 0)
            continue;
        cx = (int16_t) (cx - w);
        if (cx > 0)
            continue;
        text.cut = (uint8_t) -cx;
        text.cut_at = s + 1;
        break;
    }
    return 1;
}

/* text_skip_left: start the string at the first character reaching x 0,
 * skipping its hidden columns. Colour changes skipped over are lost. */
static int text_skip_left(const struct page16 *pg, const char **s)
{
    int16_t cx;
    if (pg->right < 0)
        return 0;
    if (text.x >= 0)
        return 1;
    cx = text.x;
    text.x = 0;
    for (const char *p = *s; *p; p++) {
        int w = advance((uint8_t) *p);
        if (w < 0)
            continue;
        cx = (int16_t) (cx + w);
        if (cx <= 0)
            continue;
        text.skip = (uint8_t) (w - cx);
        *s = p;
        return 1;
    }
    return 0;
}

/* text_cut_bottom: clip glyph rows to the top and bottom edges. */
static int text_cut_bottom(const struct page16 *pg)
{
    int16_t cx = pg->bottom;
    uint16_t ax;
    if (cx < 0)
        return 0;
    if (text.y < 0) {
        uint16_t hidden = (uint16_t) -text.y;
        if (text.rows <= hidden)
            return 0;
        text.rows = (uint16_t) (text.rows - hidden);
        text.y = 0;
        text.glyphs += (uint16_t) (hidden * text.row_bytes);
    }
    if ((uint16_t) text.y > (uint16_t) cx)
        return 0;
    ax = (uint16_t) (text.y + text.rows - 1);
    if (ax > (uint16_t) cx)
        text.rows = (uint16_t) (text.rows - (ax - cx));
    return 1;
}

/* text_render: draw from text.x, text.y. A byte of 80h or more sets the
 * colour to its low seven bits; the last colour is written back to the
 * page record's colour byte. */
static void text_render(struct page16 *pg, const char *s)
{
    uint16_t page_number = (uint16_t) pg->number & 7;
    uint32_t di;
    text.colour = (uint8_t) pg->colour;
    reciv_raster_colour((uint8_t) pg->colour, (uint8_t) ((uint16_t) pg->colour >> 8));
    text.background = (uint8_t) pg->text_colour;
    text.opaque = (uint8_t) pg->active;
    di = row_offset(text.y) + (uint32_t) (int32_t) text.x;
    for (;;) {
        uint8_t c = (uint8_t) *s, next;
        const uint8_t *glyph;
        uint8_t width, rows, skip;
        if (s == text.cut_at)
            c = 0;
        s++;
        if (!c)
            break;
        /* The driver loads the next byte before testing for the end; it is
         * never used then, so reciv reads it only within the string. */
        next = s == text.cut_at ? 0 : (uint8_t) *s;
        if (c & 0x80) {
            text.colour = c & 0x7F;
            continue;
        }
        if (c > text.last || c < text.first)
            continue;
        c = (uint8_t) (c - text.first);
        width = text.fixed ? text.fixed : (uint8_t) (text.widths[c] + text.spacing);
        if (!next)
            width = (uint8_t) (width - text.cut);
        width = (uint8_t) (width - text.skip);
        text.width = width;
        glyph = text.glyphs + ((uint16_t) c << text.shift);
        skip = text.skip;
        rows = (uint8_t) text.rows;
        if (text.opaque == 1) {
            uint32_t row = di;
            do {
                uint16_t bits = (uint16_t) (glyph[0] << 8 | glyph[1]) << skip;
                uint8_t n = text.width;
                uint32_t p = row;
                do {
                    if (bits & 0x8000) {
                        *page_at(page_number, p) = text.colour;
                        /* Opaque text replaces the whole cell.  Its background
                         * therefore also lies beneath foreground glyph bits. */
                        *background_at(page_number, p) = text.background;
                        *text_at(page_number, p) = 1;
                    } else {
                        *page_at(page_number, p) = text.background;
                        *background_at(page_number, p) = text.background;
                        *text_at(page_number, p) = 0;
                    }
                    p++;
                    bits <<= 1;
                } while (--n);
                row += SW;
                glyph += text.row_bytes;
            } while (--rows);
        } else {
            uint8_t cols = text.width;
            uint32_t row = di;
            if (next)
                cols = (uint8_t) (cols - text.spacing);
            do {
                uint16_t bits = (uint16_t) (glyph[0] << 8 | glyph[1]) << skip;
                uint8_t n = cols;
                uint32_t p = row;
                do {
                    if (bits & 0x8000)
                        store_text(page_number, p, text.colour);
                    bits <<= 1;
                    p++;
                } while (--n);
                row += SW;
                glyph += text.row_bytes;
            } while (--rows);
        }
        text.skip = 0;
        di += text.width;
        text.x = (int16_t) (text.x + text.width);
    }
    pg->colour = (int16_t) (((uint16_t) pg->colour & 0xFF00) | text.colour);
    reciv_video_dirty();
}

/* Slot 21 (text_clip_all): draw a string clipped on all sides; returns
 * the x after it, relative to the page. */
int16_t page_string(struct page16 *pg, int16_t x, int16_t y, const char *s)
{
    reciv_require_slot(21);
    if (text_prepare(pg, x, y, s) && text_cut_right(pg, s) && text_skip_left(pg, &s) && text_cut_bottom(pg)) {
        text.x = (int16_t) (text.x + pg->left);
        text.y = (int16_t) (text.y + pg->top);
        text_render(pg, s);
    }
    return (int16_t) (text.x - pg->left);
}

/* ----------------------------------------------------------- the rest */

/* Slot 37: set mode 13h (flag bit 7 keeps video memory) and load the
 * default colours. */
void video_init(int16_t flags)
{
    reciv_require_slot(37);
    reciv_video_set_mode((flags & 0x80) | 0x13);
    palette_reset();
}

/* Slot 51: the screen shake, stepped every tick. CIV never starts one
 * (shake_start, slot 29, is not called), so only the seed moves. */
static uint16_t shake_seed = 0x04D2;
static uint8_t shake_frames;

void reciv_shake_step(void)
{
    shake_seed = (uint16_t) (shake_seed * 5 + 1);
    if (shake_frames) {
        uint8_t start = (uint8_t) ((shake_seed >> 8) & 3);
        if (!--shake_frames)
            start = 0;
        reciv_set_start_address(start);
    }
}
