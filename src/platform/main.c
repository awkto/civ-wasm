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
extern int16_t game_main(int16_t argc, char **argv);

void reciv_video_close(void);
void reciv_resident_data_init(void);
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
          "  --no-filter selects nearest pixels\n", stderr);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *game_dir = ".", *save_dir = NULL;
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
        else if (!strcmp(argv[i], "--skip-quiz"))
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
    reciv_resident_data_init();
    reciv_video_open();
    reciv_video_set_mode(3);             /* DOS starts in 80x25 text */
    reciv_timer_init();

    reciv_exit(game_main((int16_t) game_argc, game_argv));
}
