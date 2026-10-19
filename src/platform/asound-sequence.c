/* Native channel_start/channel_step/channels_step from civd ASOUND.ASM.
 * All pointers inside sequence data are DOS words; arithmetic wraps there. */
#include <string.h>
#include "asound-sequence.h"

static uint16_t word(const uint8_t *p) { return (uint16_t) (p[0] | p[1] << 8); }
static void put(uint8_t *p, uint16_t v) { p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); }
static uint8_t byte(struct asound_sequence *s, uint16_t at) { return s->data[at]; }
static void event(struct asound_sequence *s, unsigned ch, enum asound_event e, int16_t v)
{
    if (s->emit)
        s->emit(s, ch, e, v);
}

uint16_t asound_sequence_random(struct asound_sequence *s)
{
    uint16_t r = (uint16_t) (s->random_state + 0x9248);
    s->random_state = (uint16_t) (r >> 3 | r << 13);
    return s->random_state;
}

void asound_sequence_init(struct asound_sequence *s, uint8_t *data, asound_emit emit, void *context)
{
    memset(s, 0, sizeof *s);
    s->data = data;
    s->random_state = 1234;
    s->emit = emit;
    s->context = context;
}

void asound_sequence_start(struct asound_sequence *s, unsigned channel, uint16_t offset)
{
    uint8_t *c;
    if (channel >= 9)
        return;
    c = s->channels[channel];
    /* The original retains note, instrument and release timer on restart. */
    put(c + 14, offset); put(c + 16, offset); put(c + 18, offset);
    put(c + 20, offset); put(c + 26, offset);
    c[10] = 255;
    c[1] = c[2] = c[3] = c[28] = c[29] = c[6] = c[9] = c[11] = c[7] = 0;
    put(c + 22, 0); put(c + 24, 0);
    c[13] = 64;
    c[0] = 1;
}

/* FF and FE maintain separate repeat points, with FE also resetting the
 * inner repeat point. The first encounter sets the count and jumps back. */
static void repeat(uint8_t *c, uint16_t pc, uint8_t operand, int outer)
{
    unsigned counter = outer ? 24 : 22, start = outer ? 20 : 18;
    uint16_t count = word(c + counter), next;
    if (!count) {
        if (!operand) {
            next = (uint16_t) (pc + 2);
            put(c + start, next);
            if (outer)
                put(c + 22, 0);
        } else {
            count = (uint16_t) (int16_t) (int8_t) operand;
            next = word(c + start);
        }
    } else {
        count--;
        next = count ? word(c + start) : (uint16_t) (pc + 2);
        if (!count)
            put(c + start, next);
    }
    put(c + counter, count);
    put(c + 16, next);
    if (outer)
        put(c + 18, word(c + 20));
}

static int step(struct asound_sequence *s, unsigned ch)
{
    uint8_t *c = s->channels[ch];
    int dirty = s->level_dirty;
    if (!c[0])
        return 1;
    if (c[8] && !--c[8])
        event(s, ch, ASOUND_RELEASE, 0);
    if (!--c[0]) {
        unsigned budget = 65536;
        for (;;) {
            uint16_t pc = word(c + 16);
            uint8_t op = byte(s, pc), arg = byte(s, (uint16_t) (pc + 1));
            if (op < 0xf3) {
                if (dirty)
                    event(s, ch, ASOUND_LEVEL, 0);
                c[4] = op;
                c[0] = arg;
                put(c + 16, (uint16_t) (pc + 2));
                if (!op || !arg)
                    event(s, ch, ASOUND_RELEASE, 0);
                else {
                    c[8] = (uint8_t) (arg - (int8_t) c[7]);
                    event(s, ch, ASOUND_NOTE, op);
                }
                break;
            }
            if (!budget--)
                return 0;
            dirty = 0;
            put(c + 16, (uint16_t) (pc + 2));
            switch (op) {
            case 0xff: repeat(c, pc, arg, 0); break;
            case 0xfe: repeat(c, pc, arg, 1); break;
            case 0xfd: {
                uint16_t start = word(c + 26);
                put(c + 14, start); put(c + 16, start);
                put(c + 18, start); put(c + 20, start);
                c[1] = c[2] = c[3] = c[6] = c[7] = c[9] = c[11] = c[28] = c[29] = 0;
                put(c + 22, 0); put(c + 24, 0); c[13] = 64;
                break;
            }
            case 0xfc: c[5] = arg; event(s, ch, ASOUND_INSTRUMENT, arg); break;
            case 0xfb: c[7] = arg; break;
            case 0xfa: c[1] = arg; break;
            case 0xf9: c[6] = (uint8_t) ((int8_t) arg >> 1); dirty = 1; break;
            case 0xf8:
                c[10] = arg; c[2] = byte(s, (uint16_t) (pc + 2)); c[9] = 1;
                put(c + 16, (uint16_t) (pc + 3)); break;
            case 0xf7: c[28] = arg; break;
            case 0xf6: {
                int16_t count = (int8_t) arg;
                uint16_t table = (uint16_t) (pc + 2);
                uint16_t pick;
                asound_sequence_random(s);
                pick = (uint16_t) ((count - 1) & s->random_state);
                uint16_t target = (uint16_t) (table + count + 1 + (int8_t) byte(s, (uint16_t) (table + count)));
                s->data[target] = byte(s, (uint16_t) (table + pick));
                put(c + 16, (uint16_t) (pc + count + 3)); break;
            }
            case 0xf5: c[29] = arg; dirty = 1; break;
            case 0xf4: c[13] = arg; dirty = 1; break;
            case 0xf3:
                c[12] = arg; c[3] = byte(s, (uint16_t) (pc + 2)); c[11] = 1;
                put(c + 16, (uint16_t) (pc + 3)); break;
            }
        }
    }
    if (c[1])
        event(s, ch, ASOUND_PITCH_STEP, (int8_t) c[1]);
    dirty = 0;
    if (c[9] || c[11]) {
        dirty = 0;
        if (!--c[9]) {
            c[9] = c[10];
            if (c[2]) {
                c[29] = (uint8_t) (c[29] + c[2]);
                dirty = 1;
                if ((int8_t) c[29] <= 0) { c[9] = c[29] = c[6] = 0; }
                if ((int8_t) c[29] >= 63) { c[9] = c[6] = 0; c[29] = 63; }
            }
        }
        if (!--c[11]) {
            c[11] = c[12];
            if (c[3]) { c[13] = (uint8_t) (c[13] + c[3]); dirty = 1; }
        }
        if (dirty)
            event(s, ch, ASOUND_LEVEL, 0);
    }
    s->level_dirty = (uint8_t) dirty;
    return 1;
}

int asound_sequence_tick(struct asound_sequence *s)
{
    for (unsigned ch = 0; ch < 9; ch++)
        if (!step(s, ch))
            return 0;
    return 1;
}
