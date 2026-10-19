/* Native ASOUND command routing, including its writable effect sequences. */
#include <string.h>
#include "asound-command.h"
#include "asound-routes.h"

static void put(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t) value; p[1] = (uint8_t) (value >> 8);
}
static void write_reg(struct asound_voice *v, uint8_t reg, uint8_t value)
{
    v->shadow[reg] = value;
    if (v->write) v->write(v->context, reg, value);
}
void asound_command_init(struct asound_command *c, struct asound_voice *v)
{
    memset(c, 0, sizeof *c);
    c->voice = v;
    c->interlude_next = 1;
}
void asound_command_tick(struct asound_command *c)
{
    struct asound_sequence *s = c->voice->sequence;
    if (c->interlude_on && !s->channels[8][0]) {
        for (unsigned i = 0; i < 8; i++) s->channels[i][0] = c->saved[i];
        put(s->channels[8] + 16, SOUND_RESET_UNUSED_PART1);
        c->interlude_on = 0;
    }
}
uint16_t asound_command_play(struct asound_command *c, int16_t command, int16_t argument)
{
    struct asound_voice *v = c->voice;
    struct asound_sequence *s = v->sequence;
    uint16_t result = 0;
    v->quiet_ticks = 0;
    if (command < 0 || command > 44) return 0;
    switch (command) {
    case 0:
        c->interlude_on = 0;
        for (unsigned i = 0; i < 9; i++) memset(s->channels[i], 0, 4);
        for (unsigned i = 0; i < 2; i++) {
            /* Reset preserves the previous sweep's voice selector. */
            v->sweeps[i].count = 0;
            v->sweeps[i].frequency = v->sweeps[i].step = v->sweeps[i].mask = 0;
        }
        for (int reg = 0x4f; reg >= 0x40; reg--) write_reg(v, reg, 63);
        for (int reg = 0xff; reg >= 0x60; reg--) write_reg(v, reg, 0);
        for (int reg = 0x3f; reg >= 1; reg--) write_reg(v, reg, 0);
        write_reg(v, 1, 0x20);
        break;
    case 1:
        for (unsigned i = 0; i < 9; i++) {
            uint8_t *ch = s->channels[i];
            put(ch + 16, SOUND_RESET_UNUSED_PART1);
            ch[2] = 255; ch[10] = ch[9] = 1;
        }
        break;
    case 2:
        for (unsigned i = 0; i < 9; i++) result |= s->channels[i][0];
        break;
    case 33: {
        static const uint16_t phrases[] = {0x208, 0x27e, 0x50, 0x98, 0xf2, 0x138, 0x1a4};
        static const uint8_t carriers[] = {0x43, 0x44, 0x45, 0x4b, 0x4c, 0x4d, 0x53, 0x54, 0x55};
        for (unsigned i = 0; i < 8; i++) {
            c->saved[i] = s->channels[i][0]; s->channels[i][0] = 0;
        }
        for (unsigned i = 0; i < 9; i++) write_reg(v, carriers[i], v->shadow[carriers[i]] | 63);
        c->interlude_on = 255;
        if (++c->interlude_next > 6) c->interlude_next = 0;
        asound_sequence_start(s, 8, phrases[c->interlude_next]);
        break;
    }
    case 35:
        c->growth ^= 255;
        for (unsigned i = 0; i < (c->growth ? 7u : 5u); i++)
            asound_sequence_start(s, i, growth_parts[i + (c->growth ? 0 : 7)]);
        break;
    case 38: case 39: {
        uint16_t offset = command == 38 ? BATTLE_WON_PART1 : BATTLE_LOST_PART2;
        if (command == 39) asound_sequence_start(s, 0, BATTLE_LOST_PART1);
        uint16_t r = (asound_sequence_random(s) & 0x0707) + 0x140c;
        s->data[offset + 5] = (uint8_t) r;
        s->data[offset + 7] = (uint8_t) (r >> 8);
        s->data[BATTLE_LOST_PART1 + 1] = (uint8_t) (r + (r >> 8));
        asound_sequence_start(s, 1, offset);
        break;
    }
    case 44: {
        static const uint16_t parts[] = {CITY_AMBIENCE_PART1, CITY_AMBIENCE_PART2, CITY_AMBIENCE_PART3};
        for (unsigned i = 0; i < 3; i++) {
            s->data[parts[i] + 1] = (asound_sequence_random(s) & 7) + 14 * (i + 1);
            asound_sequence_start(s, i, parts[i]);
        }
        for (unsigned i = 0; i < 3; i++) put(s->channels[i] + 18, CITY_AMBIENCE_PART4);
        asound_sequence_start(s, 3, CITY_AMBIENCE_PART4);
        break;
    }
    default:
        for (unsigned i = 0; i < 9 && routes[command][i].offset; i++) {
            const struct asound_route *r = &routes[command][i];
            if (r->arrangement <= ((uint16_t) argument & 3))
                asound_sequence_start(s, r->channel, r->offset);
        }
        break;
    }
    return result;
}
