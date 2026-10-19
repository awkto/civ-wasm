/* Call BATTLE.C's non-animated strength calculation using its real tables. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern unsigned char armydata[8][128][12], townbuf[128][28];
extern int16_t civnow, cur_tier, city_count[8];
extern int16_t battle(int16_t, int16_t, int16_t, int16_t, int16_t);
static int16_t terrain, flags, attacker = 1, defender = 2;

int16_t __wrap_terrain_at(int16_t x, int16_t y) { (void) x; (void) y; return terrain; }
int16_t __wrap_map_flags(int16_t x, int16_t y) { (void) x; (void) y; return flags; }
int16_t __wrap_city_at(int16_t x, int16_t y) { (void) x; (void) y; return 0; }
_Noreturn void reciv_exit(int status) { exit(status); }

static void reset(void)
{
    memset(armydata, 0, sizeof armydata);
    memset(townbuf, 0, sizeof townbuf);
    attacker = civnow = 1;
    defender = 2;
    cur_tier = terrain = flags = 0;
    city_count[1] = city_count[2] = 3;
    armydata[attacker][0][3] = 1;  /* Militia: attack 1. */
    armydata[attacker][0][4] = 3;  /* One full move. */
    armydata[defender][0][3] = 2;  /* Phalanx: defense 2. */
}

static int expect(const char *name, int expected)
{
    int actual = battle(attacker, 0, defender, 0, 0);
    if (actual != expected) {
        fprintf(stderr, "%s: strength %d vs %d\n", name, actual, expected);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failed = 0;
    reset();
    failed |= expect("flat terrain: 8 * 8 / (16 + 1)", 3);
    armydata[defender][0][0] = 8;
    failed |= expect("fortified phalanx: 64 / (24 + 1)", 2);
    reset();
    armydata[attacker][0][0] = 0x20;
    failed |= expect("veteran attacker: 96 / 17", 5);
    armydata[defender][0][0] = 0x20;
    failed |= expect("both veteran: 96 / 25", 3);
    armydata[attacker][0][0] = 0;
    failed |= expect("veteran defender: 64 / 25", 2);
    reset();
    armydata[attacker][0][4] = 2;
    failed |= expect("two thirds of a move: 40 / 17", 2);
    armydata[attacker][0][4] = 1;
    failed |= expect("one third of a move: 16 / 17", 0);
    reset();
    terrain = 4;
    failed |= expect("hills: 64 / 33", 1);
    terrain = 5;
    failed |= expect("mountains: 64 / 49", 1);
    reset();
    flags = 0x20;
    failed |= expect("fortress: 64 / 33", 1);
    flags = 1;
    townbuf[0][0] = 0x80; /* City walls. */
    failed |= expect("city walls: 64 / 49", 1);
    armydata[attacker][0][3] = 13;
    failed |= expect("artillery ignores city walls: 768 / 17", 45);
    reset();
    terrain = 5;
    armydata[defender][0][3] = 14;
    failed |= expect("air defender ignores terrain: 64 / 17", 3);
    armydata[defender][0][3] = 21;
    failed |= expect("naval defender ignores terrain: 64 / 97", 0);
    reset();
    memcpy(armydata[0][0], armydata[attacker][0], 12);
    attacker = 0;
    failed |= expect("barbarian against AI: 32 / 17", 1);
    memcpy(armydata[1][0], armydata[defender][0], 12);
    defender = 1;
    failed |= expect("barbarian against Chieftain human: 16 / 17", 0);
    cur_tier = 4;
    failed |= expect("barbarian against Emperor human: 80 / 17", 4);
    flags = 1;
    city_count[1] = 1;
    failed |= expect("barbarian cannot defeat the last human city", 0);
    if (!failed)
        puts("Combat terrain, fortification, veterans, movement, walls, domains and difficulty passed");
    return failed;
}
