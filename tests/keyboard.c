/* Exercise the production keyboard queue and DOS/BIOS entry points. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include "reciv.h"

extern uint16_t reciv_key_code(SDL_Scancode sc, SDL_Keymod mod);
extern int16_t key_none(void), read_key(void), key_echo(void);
extern int16_t crt_kbhit(void), crt_getch(void), locks_off(void);
static unsigned char echoed[32];
static int echo_count;

void __wrap_reciv_spin(void) {}
void __wrap_reciv_poll(void) {}
void __wrap_reciv_wait(void) { fputs("unexpected blocking keyboard read\n", stderr); exit(1); }
void __wrap_reciv_require_slot(int slot) { (void) slot; }
void __wrap_reciv_text_teletype(uint8_t c) { echoed[echo_count++] = c; }
_Noreturn void reciv_exit(int status) { exit(status); }

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #expr); return 1; \
} } while (0)

int main(void)
{
    CHECK(reciv_key_code(SDL_SCANCODE_A, 0) == 0x1e61);
    CHECK(reciv_key_code(SDL_SCANCODE_A, SDL_KMOD_SHIFT) == 0x1e41);
    CHECK(reciv_key_code(SDL_SCANCODE_A, SDL_KMOD_CAPS) == 0x1e41);
    CHECK(reciv_key_code(SDL_SCANCODE_A, SDL_KMOD_CAPS | SDL_KMOD_SHIFT) == 0x1e61);
    CHECK(reciv_key_code(SDL_SCANCODE_S, SDL_KMOD_SHIFT) == 0x1f53);
    CHECK(reciv_key_code(SDL_SCANCODE_1, SDL_KMOD_SHIFT) == 0x0221);
    CHECK(reciv_key_code(SDL_SCANCODE_G, SDL_KMOD_ALT) == 0x2200);
    CHECK(reciv_key_code(SDL_SCANCODE_F1, SDL_KMOD_SHIFT) == 0x5400);
    CHECK(reciv_key_code(SDL_SCANCODE_F1, SDL_KMOD_CTRL) == 0x5e00);
    CHECK(reciv_key_code(SDL_SCANCODE_KP_9, 0) == 0x4900);
    CHECK(reciv_key_code(SDL_SCANCODE_TAB, SDL_KMOD_SHIFT) == 0x0f00);
    CHECK(key_none() == -1 && crt_kbhit() == 0);

    /* Polls must leave buffered keys available, in order. */
    for (int i = 0; i < 16; i++)
        reciv_key_event((uint16_t) (0x1e61 + i));
    CHECK(key_none() == 0 && key_none() == 0);
    for (int i = 0; i < 15; i++)
        CHECK((uint16_t) read_key() == (uint16_t) (0x1e61 + i));
    CHECK(key_none() == -1);  /* The sixteenth key overflowed the BIOS queue. */
    CHECK(echo_count == 0);

    /* DOS splits extended keys across reads; kbhit sees the pending scan. */
    reciv_key_event(0x4800);
    CHECK(crt_getch() == 0);
    CHECK(crt_kbhit() == -1);
    CHECK(crt_getch() == 0x48);
    CHECK(crt_kbhit() == 0);
    CHECK(echo_count == 0);

    /* MISC slot 92 uses DOS function 1, which echoes setup choices. */
    reciv_key_event(0x0231);
    CHECK(key_echo() == 0x0131);
    CHECK(echo_count == 1 && echoed[0] == '1');
    reciv_key_event(0x1c0d);
    CHECK(key_echo() == 0x010d);
    CHECK(echo_count == 2 && echoed[1] == '\r');

    /* The recovered MISC routine clears the low nibble, despite its name. */
    *REAL(0, 0x417) = 0xad;
    CHECK(locks_off() == 0xa0 && *REAL(0, 0x417) == 0xa0);
    puts("Keyboard translation, BIOS queue, extended DOS reads and echoed input passed");
    return 0;
}
