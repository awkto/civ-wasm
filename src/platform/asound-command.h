#ifndef RECIV_ASOUND_COMMAND_H
#define RECIV_ASOUND_COMMAND_H
#include "asound-voice.h"
struct asound_command {
    struct asound_voice *voice;
    uint8_t interlude_on, saved[8], growth;
    uint16_t interlude_next;
};
void asound_command_init(struct asound_command *, struct asound_voice *);
uint16_t asound_command_play(struct asound_command *, int16_t command, int16_t argument);
/* Restore paused channels before the next music step. */
void asound_command_tick(struct asound_command *);
#endif
