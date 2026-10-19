#include <stdio.h>
#include <string.h>
#include "asound-sequence.h"

static uint8_t data[65536];
static unsigned notes[9], releases[9];
static int wrong_note;

static void emit(struct asound_sequence *s, unsigned channel, enum asound_event e, int16_t value)
{
    (void) s;
    if (e == ASOUND_NOTE) {
        notes[channel]++;
        if (value != (int) (60 + channel))
            wrong_note = 1;
    }
    if (e == ASOUND_RELEASE)
        releases[channel]++;
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #expr); return 1; \
} } while (0)

int main(void)
{
    struct asound_sequence s;
    asound_sequence_init(&s, data, emit, NULL);
    for (unsigned ch = 0; ch < 9; ch++) {
        data[ch * 4] = (uint8_t) (60 + ch);
        data[ch * 4 + 1] = 2;
        asound_sequence_start(&s, ch, (uint16_t) (ch * 4));
    }
    CHECK(asound_sequence_tick(&s));
    CHECK(!wrong_note);
    for (unsigned ch = 0; ch < 9; ch++)
        CHECK(notes[ch] == 1 && releases[ch] == 0 && s.channels[ch][0] == 2);
    CHECK(asound_sequence_tick(&s));
    for (unsigned ch = 0; ch < 9; ch++)
        CHECK(notes[ch] == 1 && releases[ch] == 0);
    CHECK(asound_sequence_tick(&s));
    for (unsigned ch = 0; ch < 9; ch++)
        CHECK(notes[ch] == 1 && releases[ch] == 2 && s.channels[ch][0] == 0);
    CHECK(asound_sequence_tick(&s));
    for (unsigned ch = 0; ch < 9; ch++)
        CHECK(releases[ch] == 2);

    /* Word pointers wrap across the end of the driver's data segment. */
    asound_sequence_init(&s, data, NULL, NULL);
    data[65535] = 60;
    data[0] = 2;
    asound_sequence_start(&s, 0, 65535);
    CHECK(asound_sequence_tick(&s));
    CHECK(s.channels[0][16] == 1 && s.channels[0][17] == 0);
    CHECK(s.channels[0][4] == 60 && s.channels[0][0] == 2);

    s.channels[0][5] = 7;
    s.channels[0][8] = 9;
    asound_sequence_start(&s, 0, 100);
    CHECK(s.channels[0][5] == 7 && s.channels[0][8] == 9);
    data[100] = 0xfd; /* Restarting a sequence containing only restart must terminate. */
    CHECK(!asound_sequence_tick(&s));
    puts("Nine AdLib channels, note release, word-pointer wrapping and command-loop guard passed");
    return 0;
}
