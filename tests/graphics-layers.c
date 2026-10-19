/* Presentation-layer bookkeeping without the game or SDL. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reciv.h"

uint8_t reciv_mem[0x110000];
uint8_t reciv_dr_pending, reciv_dr_deferred;

struct page16 {
    int16_t number, left, top, right, bottom, active, colour, text_colour, font;
};

extern void set_page_memory(int16_t, int16_t);
extern int16_t page_string(struct page16 *, int16_t, int16_t, const char *);
extern void recolour_rect(const struct page16 *, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t);
extern void blit(const struct page16 *, int16_t, int16_t, int16_t, int16_t,
                 const struct page16 *, int16_t, int16_t);
extern void draw_scanline(const uint8_t *, int16_t, int16_t, int16_t, int16_t);
extern void set_page(int16_t);

void reciv_require_slot(int slot) { (void) slot; }
void reciv_video_dirty(void) {}
void reciv_mouse_driver_poll(void) {}
void reciv_dac_write(int index, int count, const uint8_t *rgb)
{ (void) index; (void) count; (void) rgb; }
uint16_t reciv_dos_alloc(uint16_t paragraphs, uint16_t *seg, uint16_t *max)
{ (void) paragraphs; (void) seg; (void) max; return 1; }
uint16_t reciv_dos_free(uint16_t seg) { (void) seg; return 0; }
uint16_t reciv_dos_resize(uint16_t seg, uint16_t paragraphs, uint16_t *max)
{ (void) seg; (void) paragraphs; (void) max; return 0; }
void reciv_wait(void) {}
uint32_t reciv_vsync_count(void) { return 0; }
void reciv_set_start_address(uint16_t start) { (void) start; }
void reciv_video_set_mode(int mode) { (void) mode; }
_Noreturn void reciv_fatal(const char *format, ...)
{ (void) format; abort(); }

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "graphics layers: %s\n", message);
        exit(1);
    }
}

static void install_font(void)
{
    uint8_t *fonts = REAL(0x2000, 0x630);
    uint8_t *glyph;
    reciv_mgraphic_install(0x2000);
    poke16(fonts, 1);
    poke16(fonts + 2, 0x20);
    glyph = fonts + 0x20;
    glyph[-8] = 'A';
    glyph[-7] = 'A';
    glyph[-6] = 2;               /* two bytes per glyph row */
    glyph[-5] = 2;               /* fixed two-pixel advance */
    glyph[-4] = 2;               /* two rows */
    glyph[-3] = 0;
    glyph[-2] = 0;
    glyph[0] = 0x80; glyph[1] = 0;  /* foreground, background */
    glyph[2] = 0x40; glyph[3] = 0;  /* background, foreground */
}

int main(void)
{
    struct page16 screen = { 0, 0, 0, 319, 199, 0, 5, 7, 1 };
    struct page16 page1 = screen;
    const uint8_t *background, *mask;
    uint8_t row[2] = { 9, 10 };
    page1.number = 1;

    memset(SCREEN, 3, 0x10000);
    reciv_page_layers_reset(0);
    set_page_memory(1, 0x3000);
    install_font();

    page_string(&screen, 1, 1, "A");
    background = reciv_page_background(0);
    mask = reciv_page_text_mask(0);
    check(SCREEN[321] == 5 && SCREEN[642] == 5, "transparent glyph pixels were not drawn");
    check(mask[321] && mask[642], "transparent glyph pixels were not tagged");
    check(background[321] == 3 && background[642] == 3, "text destroyed its retained background");

    recolour_rect(&screen, 1, 1, 1, 1, 5, 6);
    check(SCREEN[321] == 6 && mask[321] && background[321] == 3,
          "recolour did not preserve text provenance");

    blit(&screen, 1, 1, 2, 2, &page1, 10, 10);
    check(REAL(0x3000, 0)[3210] == 6 && reciv_page_text_mask(1)[3210],
          "blit did not copy text provenance");
    check(reciv_page_background(1)[3210] == 3, "blit did not copy retained background");

    reciv_raster_page(0);
    reciv_raster_colour(12, 0);
    reciv_raster_plot(1, 1);
    check(SCREEN[321] == 12 && !mask[321] && background[321] == 12,
          "non-text overwrite did not remove the text layer");

    screen.active = 1;
    page_string(&screen, 20, 1, "A");
    check(mask[340] && !mask[341] && background[340] == 7,
          "opaque text did not separate foreground and background");
    check(SCREEN[341] == 7 && background[341] == 7, "opaque text background was not retained");

    draw_scanline(row, 0, 20, 1, 2);
    check(SCREEN[340] == 9 && SCREEN[341] == 10 && !mask[340] && !mask[341],
          "scanline overwrite did not clear text provenance");
    check(background[340] == 9 && background[341] == 10,
          "scanline overwrite did not update the background");

    screen.active = 0;
    page_string(&screen, 30, 1, "A");
    {
        uint8_t *sprite = REAL(0x4000, 0);
        poke16(sprite, 1); poke16(sprite + 2, 1);
        poke16(sprite + 4, 1); poke16(sprite + 6, 0); sprite[8] = 13;
        reciv_cursor_set(30, 1, 0x4000);
        check(SCREEN[350] == 13 && !mask[350] && background[350] == 13,
              "cursor did not cover the text layer");
        reciv_cursor_erase();
        check(SCREEN[350] == 5 && mask[350] && background[350] == 3,
              "cursor did not restore text provenance");
    }

    set_page(1);
    check(SCREEN[3210] == 6 && reciv_page_text_mask(0)[3210] &&
          reciv_page_background(0)[3210] == 3,
          "set_page did not copy the layer state");

    background = reciv_page_background(0);
    mask = reciv_page_text_mask(0);
    for (int i = 0; i < 64000; ++i)
        check((mask[i] ? SCREEN[i] : background[i]) == SCREEN[i],
              "layer composite differs from the canonical page");
    return 0;
}
