/* resident.c: CIV.EXE's resident assembly helpers, in C: the raster
 * routines (RASTER.INC), the mouse (MOUSE.INC), BIOS text output
 * (STRBIOS.ASM), the file check (FCHECK.INC) and the driver loader
 * (LOADER.INC, LOADTAIL.INC). Ported from civd's sources; the picture
 * codec (PICTURE.INC) is in picture.c and the timer in timer.c. */
#include <stdlib.h>
#include <string.h>

#include "reciv.h"

/* F0DATA's names the game reads: mouse buttons, x and y, and the archive
 * handle (resident_names.c). */
extern int16_t mouse_info_a, mouse_info_b, mouse_info_c, archive_handle;

struct page16 {
    int16_t number, left, top, right, bottom, active, colour, text_colour, font;
};

void reciv_resident_data_init(void)
{
    archive_handle = -1;
}

/* ------------------------------------------------------------- raster */

/* The edge tables rect_fill fills for raster_spans: left x per row, then
 * right x per row (civ-wasm: one row per screen row, reciv_screen_h). */
static int16_t edges[2 * 4096];
static int16_t first_row = -1, last_row;
static int edges_ready;

static void reset_edges(void)
{
    if (!edges_ready) {
        for (int i = 0; i < reciv_screen_h; i++)
            edges[i] = -1;
        edges_ready = 1;
    }
    if (first_row < 0)
        return;
    for (int i = first_row; i <= last_row && i < reciv_screen_h; i++) {
        edges[i] = -1;
        edges[reciv_screen_h + i] = 0;
    }
    first_row = -1;
    last_row = 0;
}

/* rect_fill(page, x, y, w, h, colour): fill a rectangle, offset by the
 * page's origin but not clipped. The colour's high byte is the raster
 * operation. Returns what the routine left in AX (MAP.C's world_draw
 * returns it): 0 for an empty rectangle, else the colour byte doubled, or
 * the first row if that is negative. */
int16_t rect_fill(const struct page16 *pg, int16_t x, int16_t y, int16_t w, int16_t h, int16_t colour)
{
    int16_t row, left, right;
    if (w <= 0 || h <= 0)
        return 0;
    /* civ-wasm: a fill to the classic screen's right and bottom edges
     * reaches the larger screen's; clearing all of it starts a 320x200
     * picture. */
    if (x + pg->left + w == 320 && y + pg->top + h == 200 && w >= 200 && h >= 100) {
        w = (int16_t) (reciv_screen_w - x - pg->left);
        h = (int16_t) (reciv_screen_h - y - pg->top);
    }
    if (x + pg->left <= 0 && y + pg->top <= 0 && x + pg->left + w >= 320 && y + pg->top + h >= 200)
        reciv_page_wide((uint16_t) pg->number, 0);
    reset_edges();
    reciv_raster_page((uint16_t) pg->number);
    row = (int16_t) (y + pg->top);
    first_row = row;
    left = (int16_t) (x + pg->left);
    right = (int16_t) (x + pg->left + w - 1);
    for (int16_t i = 0; i < h; i++)
        if ((uint16_t) (row + i) < (uint16_t) reciv_screen_h) {
            edges[row + i] = left;
            edges[reciv_screen_h + row + i] = right;
        }
    reciv_raster_colour((uint8_t) colour, (uint8_t) ((uint16_t) colour >> 8));
    last_row = (int16_t) (first_row + h - 1);
    reciv_raster_spans(first_row, last_row < reciv_screen_h ? last_row : (int16_t) (reciv_screen_h - 1), edges);
    if (first_row < 0 || first_row > last_row)
        return first_row;
    return (int16_t) ((colour & 0xFF) * 0x101);
}

/* Line clipping (rs_clip_line): Cohen-Sutherland against 0..xmax,
 * 0..ymax with rounded intersections. A divide overflow is caught by an
 * INT 0 handler that returns +-7F00h. */
static int16_t line_x0, line_y0, line_x1, line_y1;
static int16_t clip_xmax = 319, clip_ymax = 111;
static int16_t clip_dx, clip_dy, clip_dx_half, clip_dy_half;

static uint8_t outcode(int16_t x, int16_t y)
{
    uint8_t al = 0x0F;
    if (x >= 0)
        al &= 0xF7;
    if (x <= clip_xmax)
        al &= 0xFE;
    if (y >= 0)
        al &= 0xFB;
    if (y <= clip_ymax)
        al &= 0xFD;
    return al;
}

/* IMUL then IDIV with the clip routine's rounding: (a * b) / d, rounded
 * to nearest, where `half` is d / 2. */
static int16_t muldiv(int16_t a, int16_t b, int16_t d, int16_t half)
{
    int32_t product = (int32_t) a * b;
    uint16_t hi = (uint16_t) ((uint32_t) product >> 16);
    uint8_t bh = (uint8_t) (hi >> 8);
    int16_t q, r;
    int32_t q32 = d ? product / d : 0;
    if (!d || q32 > 32767 || q32 < -32768) {
        q = (int16_t) ((int16_t) (hi ^ (uint16_t) d) < 0 ? -0x7F00 : 0x7F00);
        r = 0;
    } else {
        q = (int16_t) q32;
        r = (int16_t) (product % d);
    }
    if ((int8_t) (bh ^ (uint8_t) ((uint16_t) d >> 8)) < 0) {
        r = (int16_t) -r;
        q--;
    }
    r = (int16_t) (r - half);
    if ((int8_t) ((uint8_t) ((uint16_t) r >> 8) ^ bh) >= 0)
        q++;
    return q;
}

static void clip_line(void)
{
    int16_t cx = line_x0, dx = line_y0, si = line_x1, di = line_y1, bp, bx, ax;
    uint8_t flags, al;
    int32_t ddx, ddy;
    flags = outcode(cx, dx);
    al = outcode(si, di);
    if (!al) {
        if (!flags)
            goto draw;
        int16_t t = cx; cx = si; si = t;
        t = dx; dx = di; di = t;
        uint8_t u = flags; flags = al; al = u;
        line_x0 = cx;
        line_y0 = dx;
    }
    if (flags & al)
        return;
    bp = dx;
    ddx = (int32_t) si - cx;
    ddy = (int32_t) di - bp;
    if (ddx != (int16_t) ddx) {
        clip_dx = (int16_t) (ddx >> 1);
        clip_dx_half = (int16_t) (clip_dx >> 1);
        clip_dy = (int16_t) (ddy >> 1);
    } else if (ddy != (int16_t) ddy) {
        clip_dx = (int16_t) ((int16_t) ddx >> 1);
        clip_dx_half = (int16_t) (((int16_t) ddx >> 1) >> 1);
        clip_dy = (int16_t) (ddy >> 1);
    } else {
        clip_dx = (int16_t) ddx;
        clip_dx_half = (int16_t) (clip_dx >> 1);
        clip_dy = (int16_t) ddy;
    }
    clip_dy_half = (int16_t) (clip_dy >> 1);
    for (;;) {
        if (al & 0x09) {
            bx = si < 0 ? 0 : clip_xmax;
            ax = (int16_t) (muldiv((int16_t) (bx - cx), clip_dy, clip_dx, clip_dx_half) + bp);
            if (ax < 0)
                goto top;
            if (ax <= clip_ymax)
                goto point;
        }
        bx = clip_ymax;
        if (!(di > bx))
top:        bx = 0;
        ax = (int16_t) (muldiv((int16_t) (bx - bp), clip_dx, clip_dy, clip_dy_half) + cx);
        if (ax < 0 || ax > clip_xmax)
            return;
        { int16_t t = ax; ax = bx; bx = t; }
point:
        if (!flags) {
            line_y1 = ax;
            line_x1 = bx;
            goto draw;
        }
        line_y0 = ax;
        line_x0 = bx;
        { int16_t t = cx; cx = si; si = t; t = bp; bp = di; di = t; }
        al = flags;
        flags = 0;
    }
draw:
    reciv_raster_line(line_x0, line_y0, line_x1, line_y1);
}

/* page_line(page, x0, y0, x1, y1, colour): a line clipped to the page. */
void page_line(const struct page16 *pg, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t colour)
{
    clip_xmax = pg->right;
    clip_ymax = pg->bottom;
    reciv_raster_target(&pg->number);
    line_x0 = x0;
    line_y0 = y0;
    line_x1 = x1;
    line_y1 = y1;
    reciv_raster_colour((uint8_t) colour, (uint8_t) ((uint16_t) colour >> 8));
    clip_line();
}

/* world_draw(page, x, y, colour): one pixel, if inside the page. */
void world_draw(const struct page16 *pg, int16_t x, int16_t y, int16_t colour)
{
    if (x < 0 || x > pg->right || y < 0 || y > pg->bottom)
        return;
    reciv_raster_target(&pg->number);
    reciv_raster_colour((uint8_t) colour, (uint8_t) ((uint16_t) colour >> 8));
    reciv_raster_plot(x, y);
}

/* -------------------------------------------------------------- mouse */

/* The mouse driver calls mouse_event on every movement or button change.
 * While the cursor is shown (dr_deferred), a movement redraws it, unless
 * a drawing call is under way, which then redraws it when it ends. */
uint8_t reciv_dr_pending, reciv_dr_deferred;
static uint16_t cursor_picture, mouse_events;
static int16_t cursor_hot_x, cursor_hot_y;
static uint8_t mouse_shift, mouse_active;

void reciv_mouse_warp(int x, int y);

void reciv_mouse_driver_poll(void)
{
    reciv_cursor_erase();
    reciv_cursor_set((int16_t) (mouse_info_b - cursor_hot_x), (int16_t) (mouse_info_c - cursor_hot_y), cursor_picture);
}

void reciv_mouse_event(int x, int y, int buttons)
{
    uint16_t condition = 0;
    if (!mouse_active)
        return;
    if (x != mouse_info_b || y != mouse_info_c)
        condition |= 1;
    if ((buttons ^ mouse_info_a) & 1)
        condition |= buttons & 1 ? 2 : 4;
    if ((buttons ^ mouse_info_a) & 2)
        condition |= buttons & 2 ? 8 : 16;
    if (!condition)
        return;
    mouse_events |= (uint16_t) buttons;
    mouse_info_a = (int16_t) buttons;
    mouse_info_b = (int16_t) x;
    mouse_info_c = (int16_t) y;
    if ((condition & 1) && reciv_dr_deferred) {
        uint8_t was = reciv_dr_pending;
        reciv_dr_pending = 2;
        if (!was) {
            reciv_mouse_driver_poll();
            reciv_dr_pending = 0;
        }
    }
}

/* init_mouse: reset the driver, install the handler and read the
 * position. The driver reports x in a 640-wide space; the shift found from
 * the centre position (320) halves it. Returns -1 with a mouse. */
int16_t init_mouse(void)
{
    mouse_shift = 1;
    mouse_info_b = 160;
    mouse_info_c = 100;
    mouse_info_a = 0;
    mouse_active = 0xFF;
    reciv_mouse_warp(160, 100);
    return -1;
}

void mouse_close(void)
{
    mouse_active = 0;
}

void set_mousecur(int16_t x, int16_t y, int16_t picture)
{
    cursor_hot_x = x;
    cursor_hot_y = y;
    cursor_picture = (uint16_t) picture;
}

/* set_text_pos(x, y): move the mouse pointer. */
void set_text_pos(int16_t x, int16_t y)
{
    mouse_info_b = x;
    mouse_info_c = y;
    if (mouse_active)
        reciv_mouse_warp(x, y);
}

/* mouse_poll: the buttons pressed since the last call. */
int16_t mouse_poll(void)
{
    uint16_t events;
    reciv_spin();
    events = mouse_events;
    mouse_events = 0;
    return (int16_t) events;
}

/* civ-wasm: touch controls draw no pointer (civwasm_set_touch). */
int16_t civwasm_no_cursor;

void mouse_show(void)
{
    if (!cursor_picture || reciv_dr_deferred || civwasm_no_cursor)
        return;
    reciv_cursor_set((int16_t) (mouse_info_b - cursor_hot_x), (int16_t) (mouse_info_c - cursor_hot_y), cursor_picture);
    reciv_dr_deferred = 1;
}

void mouse_hide(void)
{
    if (!cursor_picture || !reciv_dr_deferred)
        return;
    reciv_dr_deferred = 0;
    reciv_cursor_erase();
}

/* ----------------------------------------------------------- BIOS text */

void reciv_text_write(uint8_t c, uint8_t attribute, int count);
void reciv_text_teletype(uint8_t c);

/* print_at(row, col, s): print with BIOS calls at a position (unless both
 * are FFh). A byte FFh in the string sets the attribute, which persists. */
static uint8_t text_attribute = 0x0F;

void print_at(int16_t row, int16_t col, const char *s)
{
    if (((uint8_t) row << 8 | (uint8_t) col) != 0xFFFF)
        reciv_text_cursor((uint8_t) row, (uint8_t) col);
    for (;;) {
        uint8_t c = (uint8_t) *s++;
        if (!c)
            break;
        if (c == 0xFF) {
            c = (uint8_t) *s++;
            if (!c)
                break;
            text_attribute = c;
            continue;
        }
        reciv_text_write(' ', text_attribute, 1);
        reciv_text_teletype(c);
    }
    reciv_video_dirty();
}

/* dos_print(s): DOS function 9, a string ended by '$'. */
void dos_print(const char *s)
{
    while (*s != '$')
        reciv_text_teletype((uint8_t) *s++);
}

/* --------------------------------------------------------- file check */

int reciv_file_found(const char *name);

/* file_exists(name, drive): 0 if the file is there, else -1. The original
 * searches with an FCB and tries the other floppy drive. */
int16_t file_exists(const char *name, int16_t drive)
{
    (void) drive;
    return reciv_file_found(name) ? 0 : -1;
}

/* ------------------------------------------------------------ loader */

/* The drivers CIV loads are built in, but load_program still loads the
 * driver file into memory as DOS EXEC (function 4B03h) did, appending
 * FONTS.CV for the graphics driver, so memory use and the driver's data
 * (default palette, fonts) are as on DOS. run_program reads the header the
 * slot table came from and enables the matching built-in routines. */
static uint8_t slots_ready[111];

void reciv_require_slot(int slot)
{
    static int trace = -1;
    if (trace < 0)
        trace = getenv("RECIV_TRACE") != NULL;
    if (trace)
        reciv_log("slot %d", slot);
    if (!slots_ready[slot])
        reciv_fatal("driver slot %d called before its driver was loaded", slot);
}

static _Noreturn void driver_fatal(int code, const char *message)
{
    reciv_fatal("load_program: %d - %s", code, message);
}

static uint16_t load_capacity;
static uint16_t sound_overlay_segment;
static char sound_overlay_name[13];

static void exec_overlay(const char *name, uint16_t seg)
{
    uint8_t header[0x1C];
    int16_t fd = reciv_open(name, 0);
    uint32_t image, size;
    uint16_t relocs, table;
    if (fd < 0)
        driver_fatal(2, "Filename not found");
    if (reciv_read(fd, header, sizeof header) != (int16_t) sizeof header || header[0] != 'M' || header[1] != 'Z')
        driver_fatal(11, "Overlay load failed for some reason");
    size = (uint32_t) peek16(header + 4) * 512 - (peek16(header + 2) ? 512 - peek16(header + 2) : 0);
    image = (uint32_t) peek16(header + 8) * 16;
    if (size < image || ((size - image + 15) >> 4) > load_capacity)
        driver_fatal(8, "Not enough memory to load file");
    reciv_lseek(fd, (int32_t) image, 0);
    for (uint32_t done = 0; done < size - image;) {
        uint32_t n = size - image - done > 0x8000 ? 0x8000 : size - image - done;
        int32_t got = reciv_read(fd, reciv_mem + ((uint32_t) seg << 4) + done, (uint16_t) n);
        if (got <= 0)
            driver_fatal(11, "Overlay image is truncated or unreadable");
        done += (uint32_t) got;
    }
    relocs = peek16(header + 6);
    table = peek16(header + 0x18);
    for (uint16_t i = 0; i < relocs; i++) {
        uint8_t entry[4];
        reciv_lseek(fd, table + i * 4, 0);
        reciv_read(fd, entry, 4);
        uint8_t *p = REAL((uint16_t) (seg + peek16(entry + 2)), peek16(entry));
        poke16(p, (uint16_t) (peek16(p) + seg));
    }
    reciv_close(fd);
    if (peek16(REAL(seg, 0x2E)) == 100) {
        const char *leaf = name;
        for (const char *p = name; *p; p++)
            if (*p == '/' || *p == '\\') leaf = p + 1;
        sound_overlay_segment = seg;
        snprintf(sound_overlay_name, sizeof sound_overlay_name, "%s", leaf);
    }
}

/* driver_append: read a file to the paragraph `seg`; returns the
 * paragraph after it. */
static uint16_t driver_append(const char *name, uint16_t seg)
{
    int16_t fd = reciv_open(name, 0);
    int32_t n;
    if (fd < 0)
        driver_fatal(2, "Filename not found");
    while ((n = reciv_read(fd, REAL(seg, 0), 0x8000)) == 0x8000)
        seg = (uint16_t) (seg + 0x800);
    if (n < 0)
        driver_fatal(2, "Filename not found");
    reciv_close(fd);
    return (uint16_t) (seg + ((uint32_t) (uint16_t) n + 15) / 16);
}

int16_t load_program(const char *name, const char *append)
{
    uint16_t max = 0, seg, end;
    if (!reciv_dos_alloc(0xFFFF, &seg, &max))
        driver_fatal(1, "Allocated 1Mb of space????");
    load_capacity = (uint16_t) (max - 0x100);
    if (reciv_dos_alloc(load_capacity, &seg, &max))
        driver_fatal(8, "MS-DOS lied to us about how much memory was available");
    exec_overlay(name, seg);
    /* The paragraph after the driver's data: header word 2Ah (data segment)
     * plus word 2Ch (bytes of data) rounded up. */
    end = (uint16_t) (peek16(REAL(seg, 0x2A)) + (((uint32_t) peek16(REAL(seg, 0x2C)) + 15) >> 4));
    if (append)
        end = driver_append(append, end);
    end = (uint16_t) (end - seg + 8);
    if (end > load_capacity)
        driver_fatal(8, "Overlay has overrun allocated memory");
    if (reciv_dos_resize(seg, end, &max))
        driver_fatal(8, "Unable to shrink overlay memory allocation");
    return (int16_t) seg;
}

/* run_program(seg): install a loaded driver's slots. */
void run_program(int16_t seg)
{
    char id[13];
    uint16_t first = peek16(REAL(seg, 0x2E)), count = peek16(REAL(seg, 0x30));
    memcpy(id, REAL(seg, 0x10), 12);
    id[12] = 0;
    if (!strncmp(id, "MGRAPHIC.EXE", 12))
        reciv_mgraphic_install(peek16(REAL(seg, 0x2A)));
    else if (!strncmp(id, "MISCOVR.EXE", 11))
        reciv_misc_install();
    else if (first == 100)
        reciv_sound_install(sound_overlay_segment == (uint16_t) seg ? sound_overlay_name : id,
                           peek16(REAL(seg, 0x2A)), peek16(REAL(seg, 0x2C)));
    else
        reciv_fatal("run_program: driver %s is not supported (reciv has MCGA/VGA graphics only)", id);
    for (uint16_t i = first; i < first + count && i < sizeof slots_ready; i++)
        slots_ready[i] = 1;
}

void reciv_misc_install(void)
{
}
