/* ASOUND's mono AdLib register output, independent of a synthesizer/device. */
#ifndef RECIV_ASOUND_VOICE_H
#define RECIV_ASOUND_VOICE_H
#include "asound-sequence.h"

typedef void (*asound_write)(void *, uint8_t reg, uint8_t value);
struct asound_sweep { uint16_t frequency, step, mask; uint8_t count, voice; };
struct asound_voice {
    struct asound_sequence *sequence;
    uint8_t shadow[256], sweep_length[9], next_sweep;
    uint16_t sweep_mask[9], sweep_frequency[9], sweep_step[9];
    struct asound_sweep sweeps[2];
    int16_t rate_now, rate_request;
    uint16_t quiet_ticks;
    asound_write write;
    void *context;
};
void asound_voice_init(struct asound_voice *, struct asound_sequence *, asound_write, void *);
void asound_voice_event(struct asound_sequence *, unsigned, enum asound_event, int16_t);
/* Music step, followed by sweep lifetimes; status 0 means invalid bytecode. */
int asound_voice_tick(struct asound_voice *, int16_t *rate_request);
void asound_voice_fast_tick(struct asound_voice *);
#endif
