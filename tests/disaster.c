/* Exercise CITY.C itself with deterministic disaster and building draws.
 * Expected masks come from CITY.COD's SHL AX / NOT AX / CWD sequence. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern unsigned char townbuf[128][28], msgtext[], map_visible_to[];
extern int16_t civnow, human_set;
extern void city_disaster(int16_t city);

static const int16_t *draws;
static int draw_count, draw_next, invalid_draw;

int16_t __wrap_rand_scaled(int16_t bound)
{
    if (draw_next >= draw_count) {
        fputs("city_disaster exhausted the scripted random draws\n", stderr);
        exit(1);
    }
    if (bound != (draw_next ? 24 : 10))
        invalid_draw = 1;
    return draws[draw_next++];
}

int16_t __wrap_terrain_at(int16_t x, int16_t y)
{
    (void) x;
    (void) y;
    return 4;   /* Hills permit earthquakes. */
}

int16_t __wrap_has_tech(int16_t civ, int16_t advance)
{
    (void) civ;
    (void) advance;
    return 1;   /* Fire is possible once aqueducts are known. */
}

void __wrap_append_place_name(int16_t city)
{
    (void) city;
    strcat((char *) msgtext, "Test City");
}

void __wrap_named_msg(char *key)
{
    (void) key;
    strcpy((char *) msgtext, "Fire!");
}

_Noreturn void reciv_exit(int status)
{
    exit(status);
}

static int check(const int16_t *sequence, int count, uint32_t expected)
{
    uint32_t buildings = 0x0001c002; /* barracks, manufacturing, nuclear, SDI */
    memset(townbuf, 0, sizeof townbuf);
    memcpy(townbuf[0], &buildings, sizeof buildings);
    memset(map_visible_to, 0, 4000);
    civnow = 1;
    human_set = 0;  /* No UI: the test city is not visible to the human. */
    draws = sequence;
    draw_count = count;
    draw_next = invalid_draw = 0;
    city_disaster(0);
    memcpy(&buildings, townbuf[0], sizeof buildings);
    if (invalid_draw || draw_next != count || buildings != expected) {
        fprintf(stderr, "disaster %d: %d/%d draws, buildings %08x vs %08x\n",
                sequence[0], draw_next, count, buildings, expected);
        return 1;
    }
    return 0;
}

int main(void)
{
    static const int16_t earthquake[] = { 0, 16, 23, 15 };
    static const int16_t fire[] = { 5, 16, 23, 15 };
    static const int16_t lower_bit[] = { 0, 14 };
    static const int16_t barracks[] = { 5, 1 };
    if (check(earthquake, 4, 0x00004002)
        || check(fire, 4, 0x00004002)
        || check(lower_bit, 2, 0x00018002)
        || check(barracks, 2, 0x0001c000))
        return 1;
    puts("Earthquake and fire preserve the DOS word-shift and sign-extension behavior");
    return 0;
}
