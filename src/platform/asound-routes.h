/* ASOUND.ASM command destinations and channel starts. No music bytes. */
struct asound_route { uint16_t offset; uint8_t channel, arrangement; };
static const struct asound_route routes[][9] = {
    { {0, 0, 0}}, /* 0: sound_reset */
    { {0, 0, 0}}, /* 1: music_end */
    { {0, 0, 0}}, /* 2: music_playing */
    { {0x4aa8, 0, 0}, {0x4bf8, 1, 0}, {0x4d1c, 2, 0}, {0x4dbe, 3, 0}, {0x4e78, 4, 0}, {0x4f30, 5, 0}, {0x4fba, 6, 0}, {0x5040, 7, 0}}, /* 3: title_theme */
    { {0x2da4, 0, 0}, {0x3014, 1, 0}, {0x3170, 2, 0}, {0x33f4, 3, 0}, {0x360e, 4, 0}, {0x38b6, 5, 0}, {0x3a2a, 6, 0}, {0x3cee, 7, 0}}, /* 4: creation_theme */
    { {0x297a, 0, 3}, {0x2a5e, 1, 2}, {0x2afa, 2, 1}, {0x2c76, 3, 0}}, /* 5: american_anthem */
    { {0x0540, 0, 3}, {0x05b0, 1, 2}, {0x066c, 2, 1}, {0x06a4, 3, 0}}, /* 6: aztec_anthem */
    { {0x0918, 4, 2}, {0x0742, 0, 2}, {0x080c, 1, 1}, {0x0864, 2, 1}, {0x08be, 3, 1}, {0x0928, 6, 0}}, /* 7: egyptian_anthem */
    { {0x0a02, 0, 3}, {0x0a96, 1, 2}, {0x0b1c, 2, 1}, {0x0ba6, 6, 0}}, /* 8: zulu_anthem */
    { {0x5e0e, 0, 3}, {0x5eac, 1, 3}, {0x5f48, 2, 2}, {0x6036, 3, 2}, {0x622c, 6, 1}, {0x615c, 5, 1}, {0x62f8, 7, 0}, {0x6090, 4, 0}}, /* 9: french_anthem */
    { {0x0d36, 0, 3}, {0x0dcc, 1, 2}, {0x1168, 5, 2}, {0x0fbc, 3, 1}, {0x1072, 7, 1}, {0x10ec, 4, 1}, {0x0f4e, 6, 0}, {0x0e8e, 2, 0}}, /* 10: roman_anthem */
    { {0x4100, 2, 2}, {0x41d0, 3, 2}, {0x424a, 4, 2}, {0x42ae, 5, 2}, {0x3fde, 0, 1}, {0x405c, 1, 1}, {0x4312, 6, 0}, {0x438a, 7, 0}}, /* 11: russian_anthem */
    { {0x1f6a, 0, 2}, {0x2148, 1, 2}, {0x26e4, 7, 1}, {0x232a, 2, 1}, {0x24e2, 3, 0}, {0x2562, 4, 0}, {0x25e2, 5, 0}, {0x268c, 6, 0}}, /* 12: greek_anthem */
    { {0x5886, 0, 2}, {0x5a1a, 1, 1}, {0x5d14, 4, 1}, {0x5b96, 2, 0}, {0x5c16, 3, 0}}, /* 13: english_anthem */
    { {0x133a, 0, 2}, {0x143a, 1, 1}, {0x1534, 2, 0}, {0x1586, 3, 0}, {0x15d4, 4, 0}, {0x161e, 5, 0}, {0x166a, 6, 0}}, /* 14: babylonian_anthem */
    { {0x02d0, 0, 3}, {0x0362, 1, 2}, {0x03ac, 6, 2}, {0x03fa, 2, 1}, {0x043e, 3, 0}, {0x0468, 4, 0}, {0x0490, 5, 0}}, /* 15: chinese_anthem */
    { {0x199e, 4, 2}, {0x19a0, 6, 2}, {0x1924, 2, 1}, {0x187c, 0, 0}, {0x196e, 5, 0}, {0x1970, 3, 0}, {0x18c6, 7, 0}, {0x18c8, 1, 0}}, /* 16: mongol_anthem */
    { {0x1a68, 0, 3}, {0x1c16, 1, 2}, {0x1dc2, 2, 1}, {0x1de0, 3, 1}, {0x1dfc, 4, 1}, {0x1e18, 5, 1}, {0x1e32, 6, 0}, {0x1e86, 7, 0}}, /* 17: indian_anthem */
    { {0x49a8, 5, 3}, {0x4504, 0, 2}, {0x4604, 1, 1}, {0x48d0, 4, 1}, {0x46f6, 2, 0}, {0x47f0, 3, 0}}, /* 18: german_anthem */
    { {0x2d18, 0, 0}, {0x2d3e, 1, 0}, {0x2d56, 2, 0}, {0x2d88, 3, 0}}, /* 19: american_motif */
    { {0x06e6, 0, 0}, {0x0700, 1, 0}, {0x0716, 2, 0}, {0x0730, 3, 0}}, /* 20: aztec_motif */
    { {0x0988, 0, 0}, {0x09aa, 1, 0}, {0x09cc, 2, 0}, {0x09f0, 6, 0}}, /* 21: egyptian_motif */
    { {0x0c84, 0, 0}, {0x0cb0, 1, 0}, {0x0cd8, 2, 0}, {0x0d00, 6, 0}}, /* 22: zulu_motif */
    { {0x632a, 0, 0}, {0x6346, 1, 0}, {0x6366, 2, 0}, {0x6382, 3, 0}, {0x639e, 4, 0}, {0x63ae, 5, 0}}, /* 23: french_motif */
    { {0x1222, 0, 0}, {0x1264, 1, 0}, {0x12aa, 2, 0}, {0x12e6, 3, 0}, {0x1310, 4, 0}}, /* 24: roman_motif */
    { {0x4448, 0, 0}, {0x445a, 1, 0}, {0x446c, 2, 0}, {0x447e, 3, 0}, {0x4498, 4, 0}, {0x44b2, 5, 0}, {0x44cc, 6, 0}, {0x44e6, 7, 0}}, /* 25: russian_motif */
    { {0x2870, 0, 0}, {0x2896, 1, 0}, {0x28bc, 2, 0}, {0x28e2, 3, 0}, {0x2908, 4, 0}, {0x292e, 5, 0}, {0x2954, 6, 0}}, /* 26: greek_motif */
    { {0x5d92, 0, 0}, {0x5dac, 1, 0}, {0x5dc6, 2, 0}, {0x5dd6, 3, 0}, {0x5dee, 4, 0}}, /* 27: english_motif */
    { {0x17c2, 0, 0}, {0x17d2, 1, 0}, {0x17e2, 2, 0}, {0x1804, 3, 0}, {0x181c, 4, 0}, {0x1834, 5, 0}, {0x184c, 6, 0}}, /* 28: babylonian_motif */
    { {0x04ba, 0, 0}, {0x04d8, 1, 0}, {0x04ec, 2, 0}, {0x04fe, 3, 0}, {0x050e, 4, 0}, {0x051c, 5, 0}, {0x052c, 6, 0}}, /* 29: chinese_motif */
    { {0x1a14, 0, 0}, {0x1a24, 1, 0}, {0x1a34, 2, 0}, {0x1a46, 3, 0}, {0x1a22, 4, 0}, {0x1a44, 5, 0}, {0x1a56, 6, 0}, {0x1a58, 7, 0}}, /* 30: mongol_motif */
    { {0x1eb6, 0, 0}, {0x1ec8, 1, 0}, {0x1eda, 2, 0}, {0x1eec, 3, 0}, {0x1f02, 4, 0}, {0x1f40, 5, 0}}, /* 31: indian_motif */
    { {0x4a2e, 0, 0}, {0x4a4c, 1, 0}, {0x4a6a, 2, 0}, {0x4a88, 3, 0}, {0x4a98, 4, 0}}, /* 32: german_motif */
    { {0, 0, 0}}, /* 33: audience_interlude */
    { {0x5156, 0, 0}, {0x51e6, 1, 0}, {0x5276, 2, 0}, {0x5456, 3, 0}, {0x54d6, 4, 0}, {0x5556, 5, 0}, {0x5736, 6, 0}, {0x5830, 7, 0}}, /* 34: celebration */
    { {0, 0, 0}}, /* 35: rapture_growth */
    { {0x32c2, 1, 0}, {0x34da, 2, 0}, {0x3804, 3, 0}, {0x3976, 4, 0}, {0x3bf6, 5, 0}, {0x3edc, 6, 0}}, /* 36: bad_news */
    { {0x68ee, 0, 0}}, /* 37: unit_move */
    { {0, 0, 0}}, /* 38: battle_won */
    { {0, 0, 0}}, /* 39: battle_lost */
    { {0x6870, 6, 0}}, /* 40: battle_won_heavy */
    { {0x687c, 2, 0}, {0x6864, 3, 0}}, /* 41: battle_lost_heavy */
    { {0x68c6, 4, 0}, {0x68b4, 5, 0}}, /* 42: nuclear_blast */
    { {0x68d2, 0, 0}, {0x68e2, 1, 0}}, /* 43: bomber_hit */
    { {0, 0, 0}}, /* 44: city_ambience */
};
static const uint16_t growth_parts[12] = { 0x63ce, 0x6446, 0x64a0, 0x64f4, 0x6542, 0x6590, 0x6604, 0x6676, 0x66f2, 0x674c, 0x67a4, 0x67f4 };
#define SOUND_RESET_UNUSED_PART1 0x5eaa
#define BATTLE_WON_PART1 0x6898
#define BATTLE_LOST_PART1 0x68a4
#define BATTLE_LOST_PART2 0x688c
#define CITY_AMBIENCE_PART1 0x68fa
#define CITY_AMBIENCE_PART2 0x6900
#define CITY_AMBIENCE_PART3 0x6906
#define CITY_AMBIENCE_PART4 0x690c
