/* CIV.H: shared game records recovered from the resident executable.
 *
 * Field names are reconstructions; offsets, sizes and signedness are what
 * the compiled code requires. Only units that have been checked against the
 * original bytes include this header so far.
 */

/* DGROUP 0282: 12 terrain types, 19 bytes each. */
struct terrain_type {
    char name[12];
    unsigned char move_cost;
    signed char defense;        /* defense multiplier, in halves */
    unsigned char food, shields, trade, f5, f6;
};

/* DGROUP 112A: 28 unit types, 34 bytes each. */
struct unit_type {
    char name[12];
    int obsolete_by;
    int domain;                 /* 0 land, 1 air, 2 sea */
    int moves;
    int fuel;
    int attack;
    int defense;
    int cost;
    int sight;
    int capacity;
    int role;
    int prereq;
};

/* DGROUP 70EC: 128 cities, 28 bytes each. */
struct city {
    long buildings;             /* bit (n-1) set: improvement n is built */
    unsigned char x, y;
    char status;
    signed char size;
    char visible_size;
    char production;
    char trade;
    signed char owner;
    char rest[16];
};

/* City improvement bits (improvement number minus one). */
#define HAS_PALACE      0x00000001L
#define HAS_CITY_WALLS  0x00000080L
#define HAS_SDI         0x00010000L

/* DGROUP 81D4: 128 units per civilization, 12 bytes each. */
struct unit {
    char status;                /* 0x08 fortified, 0x20 veteran */
    unsigned char x, y;
    signed char type;
    signed char moves;          /* remaining moves, in thirds */
    char fuel;
    char goto_x;
    char goto_y;
    char f8;
    signed char visibility;     /* one bit per civilization */
    signed char next;           /* next unit in the same stack, or -1 */
    char home_city;
};

#define UNIT_FORTIFIED  0x08
#define UNIT_VETERAN    0x20
