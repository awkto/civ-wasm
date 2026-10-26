/* main.c: start-up. Opens the window, sets up the DOS machine, points the
 * file layer at the player's Civilization directory and runs the game's
 * main (game_main, renamed by src/period/prelude.h). */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL_main.h>
#ifdef __GLIBC__
#include <execinfo.h>
#include <unistd.h>
#endif

#include "reciv.h"

/* The game's main: int main(int argc, char **argv) in period types. */
#ifdef __EMSCRIPTEN__
/* civ-wasm: GAME.C defines it as int main(void), and wasm will not link a
 * call that disagrees. */
extern int16_t game_main(void);
#define game_main(argc, argv) game_main()
#else
extern int16_t game_main(int16_t argc, char **argv);
#endif

void reciv_video_close(void);
void reciv_resident_data_init(void);
void civwasm_set_screen(int w, int h);
extern int civwasm_autosave_seconds;
extern int16_t reciv_skip_quiz;

_Noreturn void reciv_exit(int status)
{
    reciv_video_close();
    exit(status);
}

#ifdef __GLIBC__
/* RECIV_DEBUG: SIGUSR1 prints a backtrace (where a hung game is). */
void reciv_backtrace(void)
{
    void *frames[8];
    int n = backtrace(frames, 8);
    backtrace_symbols_fd(frames, n, 2);
}

static void backtrace_signal(int sig)
{
    void *frames[64];
    int n = backtrace(frames, 64);
    backtrace_symbols_fd(frames, n, 2);
    if (sig == SIGSEGV)
        _exit(139);
    {
        extern int16_t story_clock, story_line, story_wait, story_mode, story_skip;
        extern unsigned char msgtext[];
        reciv_log("story mode %d line %d clock %d wait %d skip %d", story_mode, story_line, story_clock, story_wait, story_skip);
        reciv_log("msgtext %p: %02X %02X %02X %02X %02X %02X %02X %02X", (void *) msgtext,
                 msgtext[0], msgtext[1], msgtext[2], msgtext[3], msgtext[4], msgtext[5], msgtext[6], msgtext[7]);
    }
}
#endif

static void usage(void)
{
    fputs("usage: reciv [-d GAME_DIR] [-s SAVE_DIR] [--skip-quiz] [--filter NAME | --no-filter] [--text-filter NAME]\n"
          "  GAME_DIR holds the original Civilization files (default .)\n"
          "  SAVE_DIR receives saved games (default GAME_DIR)\n"
          "  --skip-quiz skips the copy-protection quiz\n"
          "  --filter NAME: hq4x (default), nearest, sharp, or bilinear\n"
          "  --text-filter NAME: sharp (default), nearest, bilinear, or same\n"
          "  --no-filter selects nearest pixels\n"
          "  --skin FILE: WinCiv map art from civdata4.rsc\n"
          "  --enemy-moves MODE: show (default), fast, or skip\n"
          "  --touch: no mouse pointer; a tap next to the active unit moves it\n"
          "  --own-moves MODE: your GoTo and automatic moves: show (default), fast, or skip\n"
          "  --skin-hidden: load the --skin but show the DOS map art\n"
          "  --no-city-blink: cities needing a new build don't blink on the map\n"
          "  --refuse-emissaries: say No to other civilizations' emissaries without asking\n"
          "  --better-default-unit: new cities build the best defender, not Militia\n", stderr);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *game_dir = ".", *save_dir = NULL;
    const char *skin_path = NULL;
    char *game_argv[16];
    int game_argc = 0;

    game_argv[game_argc++] = "CIV.EXE";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d") && i + 1 < argc)
            game_dir = argv[++i];
        else if (!strcmp(argv[i], "-s") && i + 1 < argc)
            save_dir = argv[++i];
        else if (!strcmp(argv[i], "--filter")) {
            if (i + 1 >= argc || !reciv_video_filter(argv[++i]))
                usage();
        } else if (!strncmp(argv[i], "--filter=", 9)) {
            if (!reciv_video_filter(argv[i] + 9))
                usage();
        } else if (!strcmp(argv[i], "--text-filter")) {
            if (i + 1 >= argc || !reciv_video_text_filter(argv[++i]))
                usage();
        } else if (!strncmp(argv[i], "--text-filter=", 14)) {
            if (!reciv_video_text_filter(argv[i] + 14))
                usage();
        } else if (!strcmp(argv[i], "--no-filter"))
            reciv_video_filter("nearest");
        else if (!strcmp(argv[i], "--screen") && i + 1 < argc) {
            /* civ-wasm: a larger screen, WxH, for a wider map. */
            int w, h;
            if (sscanf(argv[++i], "%dx%d", &w, &h) != 2 || w < 320 || h < 200 || w > 2048 || h > 2048)
                usage();
            civwasm_set_screen(w, h);
        } else if (!strcmp(argv[i], "--autosave") && i + 1 < argc) {
            /* civ-wasm: save slot 9 at most this often, while waiting for orders. */
            civwasm_autosave_seconds = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--continue")) {
            /* civ-wasm: skip the introduction and load a save without
             * asking: --continue [0-9], or the autosave (CIVILA) by default. */
            extern int16_t civwasm_continue;
            civwasm_continue = 18;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9' && !argv[i + 1][1])
                civwasm_continue = (int16_t) (argv[++i][0] - '0' + 1);
        } else if (!strcmp(argv[i], "--anim-speed") && i + 1 < argc) {
            /* civ-wasm: full-screen animations, wipes and fades, this many times as fast. */
            double speed = atof(argv[++i]);
            reciv_anim_speed = speed < 1 ? 10 : speed > 10 ? 100 : (int) (speed * 10 + 0.5);
        } else if (!strcmp(argv[i], "--browser-saves")) {
            /* civ-wasm: the page keeps each game's saves and history, so
             * the game's own AutoSave (every 50 turns, slots 4-9) is off. */
            extern int16_t civwasm_browser_saves;
            civwasm_browser_saves = 1;
        } else if (!strcmp(argv[i], "--quick-setup")) {
            /* civ-wasm: AdLib and mouse, without the setup questions. */
            extern int16_t civwasm_quick_setup;
            civwasm_quick_setup = 1;
        } else if (!strcmp(argv[i], "--enemy-moves") && i + 1 < argc) {
            /* civ-wasm: other civilizations' moves shown, fast or skipped. */
            extern int16_t civwasm_enemy_moves;
            i++;
            civwasm_enemy_moves = (int16_t) (!strcmp(argv[i], "fast") ? 1 : !strcmp(argv[i], "skip") ? 2 : 0);
        } else if (!strcmp(argv[i], "--touch")) {
            /* civ-wasm: the page's touch controls (civwasm_set_touch). */
            extern void civwasm_set_touch(int on);
            civwasm_set_touch(1);
        } else if (!strcmp(argv[i], "--own-moves") && i + 1 < argc) {
            /* civ-wasm: the player's GoTo, Road To and Auto moves shown, fast or skipped. */
            extern int16_t civwasm_own_moves;
            i++;
            civwasm_own_moves = (int16_t) (!strcmp(argv[i], "fast") ? 1 : !strcmp(argv[i], "skip") ? 2 : 0);
        } else if (!strcmp(argv[i], "--skin-hidden")) {
            /* civ-wasm: load the skin but show the DOS art (the page can
             * show it while playing). */
            extern void reciv_video_show_skin(int shown);
            reciv_video_show_skin(0);
        } else if (!strcmp(argv[i], "--no-city-blink")) {
            /* civ-wasm: cities needing a new build don't blink (UNITS.C). */
            extern int16_t civwasm_city_blink;
            civwasm_city_blink = 0;
        } else if (!strcmp(argv[i], "--refuse-emissaries")) {
            /* civ-wasm: AUDIENCE.C answers "Will you receive him?" with No. */
            extern int16_t civwasm_refuse_emissaries;
            civwasm_refuse_emissaries = 1;
        } else if (!strcmp(argv[i], "--better-default-unit")) {
            /* civ-wasm: CITY.C starts new cities on the best defender. */
            extern int16_t civwasm_better_default;
            civwasm_better_default = 1;
        } else if (!strcmp(argv[i], "--xray")) {
            /* civ-wasm: the game's debug view, the whole map (for tests). */
            extern int16_t xray_on;
            xray_on = 1;
        } else if (!strcmp(argv[i], "--skin") && i + 1 < argc) {
            /* civ-wasm: WinCiv map art from civdata4.rsc (skin.c). */
            skin_path = argv[++i];
        } else if (!strcmp(argv[i], "--skip-quiz"))
            reciv_skip_quiz = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
            usage();
        else if (game_argc < 15)
            game_argv[game_argc++] = argv[i];
    }
    game_argv[game_argc] = NULL;
    reciv_set_dirs(game_dir, save_dir ? save_dir : game_dir);

#ifdef __GLIBC__
    if (getenv("RECIV_DEBUG")) {
        signal(SIGUSR1, backtrace_signal);
        signal(SIGSEGV, backtrace_signal);
    }
#endif
    reciv_mem_init();
    reciv_pages_init();
    if (skin_path)
        reciv_skin_load(skin_path);
    reciv_resident_data_init();
    reciv_video_open();
    reciv_video_set_mode(3);             /* DOS starts in 80x25 text */
    reciv_timer_init();
    {
        /* civ-wasm: --continue without --quick-setup types what a player
         * would at the setup questions: 2 (AdLib) and 1 (mouse). INTRO.C
         * then skips the introduction. */
        extern int16_t civwasm_continue, civwasm_quick_setup;
        if (civwasm_continue && !civwasm_quick_setup) {
            reciv_key_event(0x0332);
            reciv_key_event(0x0231);
        }
    }

    reciv_exit(game_main((int16_t) game_argc, game_argv));
}
