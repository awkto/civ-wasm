/* Device-independent instrument/pitch output using synthetic driver data. */
#include <stdio.h>
#include "asound-voice.h"

static uint8_t data[65536];
static struct { uint8_t reg, value; } writes[1024];
static unsigned count;
static void write_opl(void *context, uint8_t reg, uint8_t value)
{
    (void) context;
    if (count < 1024) { writes[count].reg = reg; writes[count].value = value; }
    count++;
}
#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #expr); return 1; \
} } while (0)

int main(void)
{
    struct asound_sequence s;
    struct asound_voice v;
    int16_t request;
    /* Voice zero's real modulator/carrier mapping and C's F-number. */
    data[0x73c6] = 0; data[0x73c7] = 3;
    data[0x73d8] = 0; data[0x73db] = 3;
    data[0x73a8] = 0; data[0x73a9] = 2;
    data[0] = 0xfc; data[1] = 0;   /* Instrument zero: all fields clear. */
    data[2] = 0xf5; data[3] = 40;
    data[4] = 60; data[5] = 2;
    asound_sequence_init(&s, data, NULL, NULL);
    asound_voice_init(&v, &s, write_opl, NULL);
    asound_sequence_start(&s, 0, 0);
    CHECK(asound_voice_tick(&v, &request));
    CHECK(request == -1 && v.quiet_ticks == 1);
    CHECK(count == 24);
    CHECK(writes[0].reg == 0xb0 && writes[0].value == 0);
    CHECK(writes[1].reg == 0x40 && writes[1].value == 63);
    CHECK(writes[10].reg == 0x43 && writes[10].value == 63);
    CHECK(writes[19].reg == 0x43 && writes[19].value == 23);
    CHECK(writes[20].reg == 0x43 && writes[20].value == 23);
    CHECK(writes[21].reg == 0xa0 && writes[21].value == 0);
    CHECK(writes[22].reg == 0xb0 && writes[22].value == 0x16);
    CHECK(writes[23].reg == 0xb0 && writes[23].value == 0x36);
    CHECK(v.shadow[0x43] == 23 && v.shadow[0xb0] == 0x36);
    count = 0;
    CHECK(asound_voice_tick(&v, &request) && request == 0 && count == 0);
    CHECK(asound_voice_tick(&v, &request) && count == 2);
    CHECK(v.shadow[0xb0] == 0x16);

    /* A sweep instrument requests fast mode. Both initially unassigned
     * sweep voices are zero, so the original chooses B for voice zero. */
    data[100] = 0xfc; data[101] = 1;
    data[102] = 60; data[103] = 2;
    data[0x695a + 44 + 14] = 3;
    data[0x695a + 44 + 19] = 2;
    data[0x695a + 44 + 20] = 1;
    asound_sequence_start(&s, 0, 100);
    CHECK(asound_voice_tick(&v, &request));
    CHECK(request == 1 && v.sweeps[1].count == 2);
    count = 0;
    asound_voice_fast_tick(&v);
    CHECK(count == 2 && writes[0].reg == 0xa0 && writes[0].value == 1);
    CHECK(writes[1].reg == 0xb0 && writes[1].value == 0x22);
    CHECK(asound_voice_tick(&v, &request) && request == 0);
    CHECK(asound_voice_tick(&v, &request) && request == 0);
    CHECK(v.sweeps[1].count == 0);
    CHECK(asound_voice_tick(&v, &request) && request == -1);
    puts("AdLib instruments, register ordering/shadow, note release and sweep rate requests passed");
    return 0;
}
