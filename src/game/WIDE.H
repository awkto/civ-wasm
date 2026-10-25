/* civ-wasm: a map view larger than 15 x 12 tiles on a larger screen.
 * The platform sets these from --screen before the game starts; the
 * defaults are the original layout. view_cols x view_rows tiles of 16
 * pixels sit right of the 80-pixel panel and under the 8-pixel menu bar. */
#ifndef WIDE_H
#define WIDE_H
extern int view_cols, view_rows;        /* 15, 12 */
extern int screen_w, screen_h;          /* 320, 200 */
extern void wide_view(int on);          /* show the whole screen (the map) or the classic 320x200 */
/* Browser services (platform civwasm.c) and the autosave (SAVE.C). */
extern int civwasm_continue;            /* --continue: 1 + the slot to load at once (17: the autosave) */
extern int civwasm_real_ticks(int ticks);   /* ticks of the animation clock in original time */
extern int civwasm_quick_setup;         /* --quick-setup: AdLib and mouse, no questions */
extern int civwasm_idle(int entering);  /* bit 0: resize pending; bit 1: autosave now */
extern void civwasm_resize(void);
extern void autosave(void);
extern int civwasm_browser_saves;       /* --browser-saves: the page keeps the saves; no AutoSave */
/* How much further a centred view's corner sits from its centre than in
 * the original 15 x 12 view: reveal(civ, x - 7 - WIDE_DX, y - 6 - WIDE_DY). */
#define WIDE_DX (view_cols / 2 - 7)
#define WIDE_DY (view_rows / 2 - 6)
#endif
