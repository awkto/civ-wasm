/* Sound driver slots. AdLib register writes go to an emulated OPL chip
 * (Nuked-OPL3, third_party/), whose samples go to an SDL audio stream.
 * Other driver types retain silent slots. */
#include <string.h>

#include <SDL3/SDL.h>

#include "reciv.h"
#include "asound-command.h"
#include "opl3.h"

static char driver[13];
static uint16_t ticks;                  /* NSOUND: ticks since the last command */
static int adlib;
static uint8_t sound_data[65536];
static struct asound_sequence sequence;
static struct asound_voice voice;
static struct asound_command commands;

/* ------------------------------------------------------------ output */

/* The chip runs at its own rate (the AdLib's 14.318 MHz / 288); SDL
 * resamples. Samples are made on the main thread as the cooperative timer
 * reaches each tick (reciv_sound_render), so a tick's register writes land
 * at that tick's time, as the timer interrupt's did. */
#define OPL_RATE 49716
#define PREFILL (OPL_RATE / 25)         /* 40 ms queued after an underrun */
#define MAX_QUEUED (OPL_RATE * 3 / 20)  /* 150 ms: drop output beyond this */
#define MAX_BEHIND (OPL_RATE / 4)       /* after a stall, make at most 250 ms */

static opl3_chip chip;
static SDL_AudioStream *stream;
static uint64_t origin;                 /* clock time of sample 0 */
static uint64_t made;                   /* chip samples made since origin */

static void opl_write(void *context, uint8_t reg, uint8_t value)
{
    (void) context;
    OPL3_WriteReg(&chip, reg, value);
}

/* Open the device once; without one the game plays silently. */
static int audio_open(void)
{
    static int tried;
    SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, OPL_RATE };
    if (tried)
        return stream != NULL;
    tried = 1;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)
        || !(stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL))) {
        fprintf(stderr, "reciv: no sound: %s\n", SDL_GetError());
        return 0;
    }
    SDL_ResumeAudioStreamDevice(stream);
    origin = reciv_clock_ns();
    return 1;
}

static void put_samples(const int16_t *buf, int n)
{
    int queued = SDL_GetAudioStreamQueued(stream) / 4;
    if (queued <= 0) {
        static const int16_t silence[2 * PREFILL];
        SDL_PutAudioStreamData(stream, silence, sizeof silence);
    } else if (queued > MAX_QUEUED)
        return;                         /* the host clock ran ahead of the device's */
    SDL_PutAudioStreamData(stream, buf, n * 4);
}

/* Make the chip's samples up to clock time t. */
void reciv_sound_render(uint64_t t)
{
    int16_t buf[2 * 512];
    uint64_t want;
    if (!stream)
        return;
    if (t < origin) {
        origin = t;
        made = 0;
        return;
    }
    want = (t - origin) * OPL_RATE / 1000000000u;
    if (want > made + MAX_BEHIND)
        made = want - MAX_BEHIND;
    while (made < want) {
        int n = want - made < 512 ? (int) (want - made) : 512;
        OPL3_GenerateStream(&chip, buf, (uint32_t) n);
        put_samples(buf, n);
        made += (uint64_t) n;
    }
    if (made >= OPL_RATE) {             /* keep the products small */
        origin += 1000000000u;
        made -= OPL_RATE;
    }
}

void reciv_sound_install(const char *name, uint16_t data_segment, uint16_t data_size)
{
    snprintf(driver, sizeof driver, "%s", name);
    adlib = !SDL_strcasecmp(driver, "ASOUND.CVL");
    if (adlib) {
        /* The recovered routes and tables describe the 474.05 driver. */
        if (data_size != 0x76a0)
            reciv_fatal("Unsupported AdLib driver data size: %u", data_size);
        memset(sound_data, 0, sizeof sound_data);
        /* Header size includes BSS; copy only the initialized data. */
        memcpy(sound_data, REAL(data_segment, 0), 0x7403);
        asound_sequence_init(&sequence, sound_data, NULL, NULL);
        OPL3_Reset(&chip, OPL_RATE);
        asound_voice_init(&voice, &sequence, audio_open() ? opl_write : NULL, NULL);
        asound_command_init(&commands, &voice);
    }
}

/* Slot 100 */
void sound_init(void)
{
    reciv_require_slot(100);
    ticks = 0;
    if (adlib) {
        uint16_t quiet = voice.quiet_ticks;
        asound_command_play(&commands, 0, 0);
        voice.quiet_ticks = quiet;
    }
}

/* Slot 101: play command n (anthems, motifs, effects). */
void sound_command(int16_t command, int16_t argument)
{
    reciv_require_slot(101);
    ticks = 0;
    if (adlib) asound_command_play(&commands, command, argument);
}

/* Slot 102 */
void sound_shutdown(void)
{
    reciv_require_slot(102);
    if (adlib) {
        uint16_t quiet = voice.quiet_ticks;
        asound_command_play(&commands, 0, 0);
        voice.quiet_ticks = quiet;
        voice.rate_request = 0;
    }
}

/* Slot 105: ticks since the last command. */
int16_t sound_quiet_ticks(void)
{
    reciv_require_slot(105);
    reciv_spin();
    return (int16_t) (adlib ? voice.quiet_ticks : ticks);
}

/* Slot 103, from the timer: nonzero asks for a faster (positive) or the
 * normal (negative) tick rate. */
int16_t reciv_sound_tick(void)
{
    ticks++;
    if (adlib) {
        int16_t request = 0;
        asound_command_tick(&commands);
        if (!asound_voice_tick(&voice, &request))
            reciv_fatal("AdLib sequence contains an endless command loop");
        return request;
    }
    return 0;
}

/* Slot 104, at the fast tick rate. */
void reciv_sound_fast_tick(void)
{
    if (adlib) asound_voice_fast_tick(&voice);
}
