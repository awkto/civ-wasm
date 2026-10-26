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
extern int civwasm_enemy_moves;         /* --enemy-moves: 0 shown, 1 fast, 2 skipped (set while playing too) */
extern int civwasm_tap_moves;           /* --touch: a click next to the active unit moves it */
extern int civwasm_tap_plain;           /* ...unless the page marked this click plain (a long press) */
extern int civwasm_city_blink;          /* cities needing a new build blink (UNITS.C; set while playing too) */
extern int civwasm_refuse_emissaries;   /* other civilizations' emissaries turned away unasked (AUDIENCE.C; set while playing too) */
extern char civwasm_road_to[8][128];     /* settlers on Road To (PATH.C, saved by SAVE.C) */
extern int road_to_goal(int, int);
extern int civwasm_road_cursor;         /* its cursor, ROAD for GoTo's GO */
extern int civwasm_road_cursor_cut(int);    /* mgraphic.c */
extern char civwasm_auto_work[8][128];  /* settlers on Auto Irrigate ('i') or Auto Mine ('m') (PATH.C) */
extern unsigned char civwasm_auto_x[8][128], civwasm_auto_y[8][128];
extern int auto_work_goal(int, int);
extern int civwasm_better_default;      /* new cities build the best defender (CITY.C; set while playing too) */
extern int civwasm_browser_saves;       /* --browser-saves: the page keeps the saves; no AutoSave */
/* How much further a centred view's corner sits from its centre than in
 * the original 15 x 12 view: reveal(civ, x - 7 - WIDE_DX, y - 6 - WIDE_DY). */
#define WIDE_DX (view_cols / 2 - 7)
#define WIDE_DY (view_rows / 2 - 6)
extern void civwasm_skin_picture(int, char *);   /* skin.c: a picture loaded */
extern void civwasm_skin_sprite(struct page *, int, int, int);  /* mgraphic.c: a sprite's skin alone */
extern void civwasm_skin_fill(struct page *, int, int, int, int, int);  /* mgraphic.c: skin colour alone */
extern void civwasm_skin_recolour(struct page *, int, int, int, int, int, int);  /* and recoloured alone */
#define SKIN_PANEL 80                   /* WinCiv city screen: cream (RECIV_SKIN_PANEL) */
#define SKIN_FRAME 81                   /* and dark red (RECIV_SKIN_FRAME) */
#define SKIN_GREY 6                     /* the sheet's greys, 0 to 8 */
#endif
