/* civwasm.c: civ-wasm's additions to the platform: a screen sized to the
 * browser window, resized while the game waits for an order, and an
 * autosave written at those same waits.
 *
 * The game calls civwasm_idle from UNITS.C's blink_unit, where it blinks
 * the active unit (or "End of Turn") until a key or click arrives. That is
 * the one point where nothing is half drawn and every order so far is
 * applied, so a resize (redrawing the map, as a scroll does) and a save
 * (SAVE.C autosave, the game's own silent writer) are both safe there. */
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define CIVWASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define CIVWASM_EXPORT
#endif

#include "reciv.h"

void reciv_pages_resize(int old_w, int old_h);
void reciv_video_resize(void);

extern int16_t view_cols, view_rows, screen_w, screen_h;
extern int16_t screen_page[9], page1[9], page2[9], page3[9];

/* Whole tiles right of the 80-pixel panel and under the 8-pixel menu bar;
 * the page records clip lines and sprites at their right and bottom
 * (struct page: number, left, top, right, bottom). */
void civwasm_set_screen(int w, int h)
{
    int16_t *records[] = { screen_page, page1, page2, page3 };
    w = w < 320 ? 320 : w > 2048 ? 2048 : w;
    h = h < 200 ? 200 : h > 2048 ? 2048 : h;
    view_cols = (int16_t) ((w - 80) / 16 > 80 ? 80 : (w - 80) / 16);
    view_rows = (int16_t) ((h - 8) / 16 > 50 ? 50 : (h - 8) / 16);
    reciv_screen_w = screen_w = (int16_t) (80 + 16 * view_cols);
    reciv_screen_h = screen_h = (int16_t) (8 + 16 * view_rows);
    for (int r = 0; r < 4; r++) {
        records[r][3] = (int16_t) (reciv_screen_w - 1);
        records[r][4] = (int16_t) (reciv_screen_h - 1);
    }
}

/* ------------------------------------------------------------- resize */

static int pending_w, pending_h;

/* The page asks for a new screen size when the window changes. */
CIVWASM_EXPORT void civwasm_request_screen(int w, int h)
{
    pending_w = w;
    pending_h = h;
}

/* The page's Enemy moves button, at any time: 0 shown, 1 fast, 2 skipped. */
CIVWASM_EXPORT void civwasm_set_enemy_moves(int mode)
{
    extern int16_t civwasm_enemy_moves;
    civwasm_enemy_moves = (int16_t) (mode < 0 || mode > 2 ? 0 : mode);
}

/* --------------------------------------------------------- settings */

void reciv_video_set_filter(int index);
void reciv_video_show_skin(int shown);
extern int civwasm_autosave_seconds;

/* The page's settings overlay, while playing. Each only sets a value the
 * game or the presenter reads as it goes. */
CIVWASM_EXPORT void civwasm_set_filter(int index)      /* nearest, sharp, bilinear, hq4x */
{
    reciv_video_set_filter(index);
}

CIVWASM_EXPORT void civwasm_show_skin(int shown)       /* the WinCiv skin, if loaded */
{
    reciv_video_show_skin(shown);
}

CIVWASM_EXPORT void civwasm_set_anim_speed(int tenths)
{
    reciv_anim_speed = tenths < 10 ? 10 : tenths > 100 ? 100 : tenths;
}

CIVWASM_EXPORT void civwasm_set_city_blink(int on)
{
    extern int16_t civwasm_city_blink;
    civwasm_city_blink = (int16_t) !!on;
}

CIVWASM_EXPORT void civwasm_set_autosave(int seconds)
{
    civwasm_autosave_seconds = seconds < 0 ? 0 : seconds;
}

CIVWASM_EXPORT void civwasm_set_refuse_emissaries(int on)
{
    extern int16_t civwasm_refuse_emissaries;
    civwasm_refuse_emissaries = (int16_t) !!on;
}

CIVWASM_EXPORT void civwasm_set_better_default(int on)
{
    extern int16_t civwasm_better_default;
    civwasm_better_default = (int16_t) !!on;
}

/* Write the autosave at the next wait for an order, whatever the
 * autosave setting: the page is about to leave the game. */
static int save_now;
CIVWASM_EXPORT void civwasm_save_now(void)
{
    save_now = 1;
}

static int resize_pending(void)
{
    int w = pending_w < 320 ? 320 : pending_w, h = pending_h < 200 ? 200 : pending_h;
    if (!pending_w)
        return 0;
    /* A size that rounds to the same tiles changes nothing. */
    return (80 + 16 * (((w - 80) / 16) > 80 ? 80 : (w - 80) / 16)) != reciv_screen_w ||
           (8 + 16 * (((h - 8) / 16) > 50 ? 50 : (h - 8) / 16)) != reciv_screen_h;
}

/* The game's call, with the mouse cursor hidden: apply the new size. The
 * game then redraws the map. */
void civwasm_resize(void)
{
    int old_w = reciv_screen_w, old_h = reciv_screen_h;
    civwasm_set_screen(pending_w, pending_h);
    pending_w = pending_h = 0;
    reciv_pages_resize(old_w, old_h);
    reciv_video_resize();
}

/* The game's animation clock runs --anim-speed times as fast; a wait that
 * should keep the original pace (blink_unit) asks for this many ticks. */
int16_t civwasm_real_ticks(int16_t ticks)
{
    return (int16_t) ((ticks * reciv_anim_speed + 5) / 10);
}

/* ----------------------------------------------------------- autosave */

int civwasm_autosave_seconds;           /* 0: off (--autosave SECONDS, or the page's settings) */
static int changed;
static uint64_t last_save;

/* The game's call at each pass of its wait for an order. `entering` is
 * true on the first pass: an order was just carried out, so the game has
 * changed. Returns bit 0 for a resize to apply, bit 1 for an autosave to
 * write now (at most one per civwasm_autosave_seconds). */
int16_t civwasm_idle(int16_t entering)
{
    int16_t result = 0;
    uint64_t now = reciv_clock_ns();
    if (entering)
        changed = 1;
    if (resize_pending())
        result |= 1;
    if (save_now) {
        save_now = 0;
        last_save = now;
        result |= 2;
        return result;
    }
    if (civwasm_autosave_seconds && changed &&
        now - last_save >= (uint64_t) civwasm_autosave_seconds * 1000000000ull) {
        changed = 0;
        last_save = now;
        result |= 2;
    }
    return result;
}
