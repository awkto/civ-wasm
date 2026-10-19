/* Sound timer requests through the real cooperative timer, no SDL window. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "reciv.h"

#define FRAME_NS 14268635ull
static uint64_t now;
static int music, sweeps, request = 1;
extern void resident_init(void), restore_timer(void);
extern int16_t timer_reset(void);

uint64_t __wrap_SDL_GetTicksNS(void) { return now; }
void __wrap_reciv_pump_events(void) {}
void __wrap_reciv_video_present(int force) { (void) force; }
void __wrap_reciv_shake_step(void) {}
int16_t __wrap_reciv_sound_tick(void) { music++; return (int16_t) request; }
void __wrap_reciv_sound_fast_tick(void) { sweeps++; }
_Noreturn void reciv_exit(int status) { exit(status); }

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "line %d: %s (music %d, sweeps %d)\n", __LINE__, #expr, music, sweeps); return 1; \
} } while (0)

static void advance(uint64_t time)
{
    now = time;
    reciv_poll();
}

int main(void)
{
    reciv_timer_init();
    resident_init();
    advance(FRAME_NS);
    CHECK(music == 1 && sweeps == 1); /* Positive request also sweeps immediately. */
    request = 0;
    advance(FRAME_NS + FRAME_NS / 4);
    CHECK(music == 1 && sweeps == 2);
    advance(FRAME_NS + FRAME_NS * 2 / 4);
    CHECK(sweeps == 3);
    advance(FRAME_NS + FRAME_NS * 3 / 4);
    CHECK(sweeps == 4);
    advance(FRAME_NS * 2);
    CHECK(music == 2 && sweeps == 5);
    request = -1;
    advance(FRAME_NS * 3);
    CHECK(music == 3 && sweeps == 8); /* Slow request suppresses this IRQ's sweep. */
    advance(FRAME_NS * 4);
    CHECK(music == 4 && sweeps == 8);
    request = 1;
    advance(FRAME_NS * 5);
    CHECK(music == 5 && sweeps == 9);
    request = 0;
    advance(FRAME_NS * 7);
    CHECK(music == 6 && sweeps == 17); /* Seventh music step skipped; sweeps continue. */
    CHECK(timer_reset() == 7);
    restore_timer();
    advance(FRAME_NS * 8);
    CHECK(music == 6 && sweeps == 17);
    CHECK(timer_reset() == 0);
    resident_init();
    advance(FRAME_NS * 9);
    CHECK(music == 7 && sweeps == 17); /* Reinstall starts at normal rate. */
    CHECK(timer_reset() == 1);
    CHECK(reciv_vsync_count() == 9);
    CHECK(peek16(REAL(0x40, 0x6c)) == 2); /* BIOS keeps its independent 18.2 Hz clock. */
    puts("Sound fast/slow requests, quarter-frame sweeps and independent game/BIOS clocks passed");
    return 0;
}
