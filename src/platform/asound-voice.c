/* ASOUND.ASM voice_level/pitch/note/release, instrument_select/load and
 * sweep processing. Table offsets are from the recovered 1991 ASOUND.CVL.
 * All music/instrument bytes come from the supplied driver data segment. */
#include <string.h>
#include "asound-voice.h"

static uint8_t get(struct asound_voice *v, unsigned at)
{
    return v->sequence->data[(uint16_t) at];
}
static uint16_t word(struct asound_voice *v, unsigned at)
{
    return (uint16_t) (get(v, at) | get(v, at + 1) << 8);
}
static void out(struct asound_voice *v, unsigned reg, unsigned value)
{
    v->shadow[(uint8_t) reg] = (uint8_t) value;
    if (v->write)
        v->write(v->context, (uint8_t) reg, (uint8_t) value);
}
static unsigned operator(struct asound_voice *v, unsigned ch, unsigned carrier)
{
    return get(v, 0x73d8 + get(v, 0x73c6 + ch * 2 + carrier));
}
static void release(struct asound_voice *v, unsigned ch)
{
    out(v, 0xb0 + ch, v->shadow[0xb0 + ch] & 0xdf);
}
static void level(struct asound_voice *v, unsigned ch)
{
    uint8_t *c = v->sequence->channels[ch];
    unsigned reg = 0x40 + operator(v, ch, 1);
    int amount = c[6] + (int8_t) c[29];
    if (amount < 0) amount = 0;
    if (amount > 63) amount = 63;
    out(v, reg, (v->shadow[reg] & 0xc0) | (63 - amount));
}
static void pitch(struct asound_voice *v, unsigned ch)
{
    uint8_t *c = v->sequence->channels[ch];
    uint16_t f = (uint16_t) (word(v, 0x73a8 + (c[4] % 12) * 2) + (int8_t) c[28]);
    out(v, 0xa0 + ch, f);
    out(v, 0xb0 + ch, (v->shadow[0xb0 + ch] & 0x20) | (f >> 8) | ((c[4] / 12) << 2));
}
static void pitch_step(struct asound_voice *v, unsigned ch)
{
    uint16_t f = (uint16_t) (((v->shadow[0xb0 + ch] & 0x1f) << 8)
        + v->shadow[0xa0 + ch] + (int8_t) v->sequence->channels[ch][1]);
    out(v, 0xa0 + ch, f);
    out(v, 0xb0 + ch, (v->shadow[0xb0 + ch] & 0x20) | (f >> 8));
}
static void instrument_half(struct asound_voice *v, unsigned ch, unsigned op, unsigned p)
{
    out(v, 0x40 + op, 63);
    out(v, 0xbd, (v->shadow[0xbd] & 63) | (get(v, 0x73c0) ? 128 : 0) | (get(v, 0x73c2) ? 64 : 0));
    out(v, 8, get(v, 0x73c4) ? 64 : 0);
    /* The original writes channel feedback for both halves, in this order. */
    out(v, 0xc0 + ch, (get(v, p + 10) << 1) | (get(v, p + 13) ? 0 : 1));
    out(v, 0x60 + op, (get(v, p) << 4) | (get(v, p + 1) & 15));
    out(v, 0x80 + op, (get(v, p + 2) << 4) | (get(v, p + 3) & 15));
    out(v, 0x20 + op, (get(v, p + 11) ? 128 : 0) + (get(v, p + 12) ? 64 : 0)
        + (get(v, p + 4) ? 32 : 0) + (get(v, p + 5) ? 16 : 0) + (get(v, p + 9) & 15));
    out(v, 0xe0 + op, get(v, p + 8) & 3);
    out(v, 0x40 + op, (63 - (get(v, p + 6) & 63)) | (get(v, p + 7) << 6));
}
static void instrument(struct asound_voice *v, unsigned ch, uint8_t number)
{
    unsigned p = 0x695a + number * 44;
    release(v, ch);
    instrument_half(v, ch, operator(v, ch, 0), p);
    v->sweep_length[ch] = get(v, p + 14);
    v->sweep_mask[ch] = word(v, p + 16);
    v->sweep_frequency[ch] = word(v, p + 18);
    v->sweep_step[ch] = word(v, p + 20);
    instrument_half(v, ch, operator(v, ch, 1), p + 22);
}
static void note(struct asound_voice *v, unsigned ch)
{
    level(v, ch);
    if (!v->sweep_length[ch]) {
        pitch(v, ch);
        out(v, 0xb0 + ch, v->shadow[0xb0 + ch] | 0x20);
    } else {
        struct asound_sweep *sweep;
        if (ch == v->sweeps[0].voice) v->next_sweep = 0;
        if (ch == v->sweeps[1].voice) v->next_sweep = 1;
        sweep = &v->sweeps[v->next_sweep ? 1 : 0];
        v->next_sweep = !v->next_sweep;
        /* Both original branches test A's count before releasing their
         * old voice, including when replacing B. */
        if (v->sweeps[0].count)
            release(v, sweep->voice);
        sweep->voice = (uint8_t) ch;
        sweep->count = v->sweep_length[ch];
        sweep->mask = v->sweep_mask[ch];
        sweep->frequency = v->sweep_frequency[ch];
        sweep->step = v->sweep_step[ch];
        if (v->rate_now != 1)
            v->rate_request = v->rate_now = 1;
    }
}

void asound_voice_event(struct asound_sequence *s, unsigned ch, enum asound_event event, int16_t value)
{
    struct asound_voice *v = s->context;
    if (ch >= 9)
        return;
    switch (event) {
    case ASOUND_INSTRUMENT: instrument(v, ch, (uint8_t) value); break;
    case ASOUND_LEVEL: level(v, ch); break;
    case ASOUND_NOTE: note(v, ch); break;
    case ASOUND_RELEASE: release(v, ch); break;
    case ASOUND_PITCH_STEP: pitch_step(v, ch); break;
    }
}

void asound_voice_init(struct asound_voice *v, struct asound_sequence *s, asound_write write, void *context)
{
    memset(v, 0, sizeof *v);
    v->sequence = s;
    v->write = write;
    v->context = context;
    s->emit = asound_voice_event;
    s->context = v;
}

int asound_voice_tick(struct asound_voice *v, int16_t *rate_request)
{
    struct asound_sweep *a = &v->sweeps[0], *b = &v->sweeps[1];
    asound_sequence_random(v->sequence);
    v->quiet_ticks++;
    if (!asound_sequence_tick(v->sequence))
        return 0;
    if (a->count || b->count) {
        if (a->count) {
            a->frequency = (uint16_t) (a->frequency + a->step);
            if (!--a->count && (!b->count || a->voice != b->voice)) {
                out(v, 0xa0 + a->voice, 0); out(v, 0xb0 + a->voice, 0);
            }
        }
        if (b->count) {
            b->frequency = (uint16_t) (b->frequency + b->step);
            /* Preserve the original B-sweep termination test, including
             * its asymmetry when A has expired on the same voice. */
            if (!--b->count && (a->count || b->voice != a->voice)) {
                out(v, 0xa0 + b->voice, 0); out(v, 0xb0 + b->voice, 0);
            }
        }
    } else if (v->rate_now != -1)
        v->rate_request = v->rate_now = -1;
    *rate_request = v->rate_request;
    v->rate_request = 0;
    return 1;
}

void asound_voice_fast_tick(struct asound_voice *v)
{
    uint16_t random = asound_sequence_random(v->sequence);
    for (unsigned i = 0; i < 2; i++) {
        struct asound_sweep *s = &v->sweeps[i];
        if (s->count) {
            uint16_t f = (uint16_t) (s->frequency + ((i ? random : (uint16_t) ~random) & s->mask));
            out(v, 0xa0 + s->voice, f);
            out(v, 0xb0 + s->voice, (f >> 8) | 0x20);
        }
    }
}
