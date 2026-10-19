/* ASOUND's bytecode interpreter. Sequence data belongs to the player's driver. */
#ifndef RECIV_ASOUND_SEQUENCE_H
#define RECIV_ASOUND_SEQUENCE_H
#include <stdint.h>

enum asound_event { ASOUND_INSTRUMENT, ASOUND_LEVEL, ASOUND_NOTE, ASOUND_RELEASE, ASOUND_PITCH_STEP };
struct asound_sequence;
typedef void (*asound_emit)(struct asound_sequence *, unsigned, enum asound_event, int16_t);
struct asound_sequence {
    uint8_t *data;                      /* writable 64 KB driver data segment */
    uint8_t channels[9][30];           /* original channel record layout */
    uint16_t random_state;
    uint8_t level_dirty;               /* channel_step's reused stack scratch */
    asound_emit emit;
    void *context;
};
void asound_sequence_init(struct asound_sequence *s, uint8_t *data, asound_emit emit, void *context);
void asound_sequence_start(struct asound_sequence *s, unsigned channel, uint16_t offset);
/* Returns 0 on malformed data (a command-only infinite loop), else 1. */
int asound_sequence_tick(struct asound_sequence *s);
uint16_t asound_sequence_random(struct asound_sequence *s);
#endif
