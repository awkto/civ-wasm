/* skin.c: civ-wasm's WinCiv skin. Civilization for Windows keeps all its
 * map art in one picture, SPR32X32.GIF (CvPc resource 200 in civdata4.rsc,
 * a classic Mac resource fork). It is the DOS game's sprite pictures drawn
 * again at twice the size, at the same places: SP257.PIC (x, y) is the
 * sheet's (2x, 2y), TER257.PIC (x, y) is (640 + 2x, 2y) and SP299.PIC
 * (x, y) is (832 + 2x, 2y - 100). When the game loads one of those
 * pictures, the parts the sheet has go into the page's skin layer
 * (mgraphic.c), and from there to every tile the game cuts and draws.
 *
 * CvPc: u16 width, u16 height, u8 depth (the LZW minimum code size),
 * u8 colours - 1, RGB palette, then GIF image data sub-blocks. */
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "reciv.h"

#define SHEET_W 1472
#define SHEET_H 400
#define SHEET_KEY 29                    /* the transparent colour */

static uint8_t *sheet;
int reciv_skin_colours;
uint32_t reciv_skin_palette[128];
uint8_t reciv_skin_washed[128];         /* a sentry's colours, by skin byte */

/* MAP.C greys a sentry by turning its black and dark grey (DOS 5 and 8)
 * light grey. In the skin: every colour but the civilization's becomes a
 * light one of the sheet's greys (colours 0 to 8: 0, 32, ... 255). */
static void wash(void)
{
    for (int b = 1; b < 128; b++) {
        uint32_t c = reciv_skin_palette[b - 1];
        int lum = (int) ((c >> 16 & 255) * 3 + (c >> 8 & 255) * 6 + (c & 255)) / 10;
        reciv_skin_washed[b] = b - 1 == 12 || b - 1 == 13 ? (uint8_t) b : (uint8_t) ((128 + lum / 2 + 16) / 32 + 1);
    }
}

static uint32_t be32(const uint8_t *p) { return (uint32_t) p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static uint16_t be16(const uint8_t *p) { return (uint16_t) (p[0] << 8 | p[1]); }

/* The body of resource `type` `id` in a resource fork, or NULL. */
static const uint8_t *resource(const uint8_t *f, size_t size, const char *type, int id, uint32_t *length)
{
    uint32_t data, map, types, count;
    if (size < 16)
        return NULL;
    data = be32(f);
    map = be32(f + 4);
    if (map + 30 > size)
        return NULL;
    types = map + be16(f + map + 24);
    if (types + 2 > size)
        return NULL;
    count = be16(f + types) + 1u;
    for (uint32_t t = 0; t < count && types + 2 + t * 8 + 8 <= size; t++) {
        const uint8_t *e = f + types + 2 + t * 8;
        uint32_t n = be16(e + 4) + 1u, refs = types + be16(e + 6);
        if (memcmp(e, type, 4))
            continue;
        for (uint32_t r = 0; r < n && refs + r * 12 + 12 <= size; r++) {
            const uint8_t *ref = f + refs + r * 12;
            uint32_t at = data + (be32(ref + 4) & 0xFFFFFF);
            if ((int16_t) be16(ref) != id || at + 4 > size)
                continue;
            *length = be32(f + at);
            return at + 4 + *length <= size ? f + at + 4 : NULL;
        }
    }
    return NULL;
}

/* GIF LZW, codes least significant bit first, into out[n]. */
static int lzw(const uint8_t *in, size_t len, int min_bits, uint8_t *out, size_t n)
{
    static uint16_t prefix[4096];
    static uint8_t suffix[4096], first[4096];
    static uint8_t stack[4096];
    int clear = 1 << min_bits, size = min_bits + 1, next = clear + 2, prev = -1;
    uint32_t bits = 0;
    int nbits = 0;
    size_t i = 0, o = 0;
    for (int c = 0; c < clear; c++)
        suffix[c] = first[c] = (uint8_t) c;
    while (o < n) {
        int c, k, sp = 0;
        while (nbits < size) {
            if (i >= len)
                return 0;
            bits |= (uint32_t) in[i++] << nbits;
            nbits += 8;
        }
        c = (int) (bits & ((1u << size) - 1));
        bits >>= size;
        nbits -= size;
        if (c == clear) {
            size = min_bits + 1;
            next = clear + 2;
            prev = -1;
            continue;
        }
        if (c == clear + 1)
            break;
        if (c < next && (c < clear || c > clear + 1))
            k = c;
        else if (c == next && prev >= 0) {
            stack[sp++] = first[prev];
            k = prev;
        } else
            return 0;
        while (k >= clear) {
            if (sp >= 4095)
                return 0;
            stack[sp++] = suffix[k];
            k = prefix[k];
        }
        stack[sp++] = (uint8_t) k;
        while (sp && o < n)
            out[o++] = stack[--sp];
        if (prev >= 0 && next < 4096) {
            prefix[next] = (uint16_t) prev;
            suffix[next] = first[c == next ? prev : c];
            first[next] = first[prev];
            if (++next == 1 << size && size < 12)
                size++;
        }
        prev = c;
    }
    return o == n;
}

/* --skin PATH: read the sheet from civdata4.rsc. False if it is not one. */
int reciv_skin_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint8_t *file, *stream;
    const uint8_t *pic;
    uint32_t length, p, n = 0;
    long size;
    int ok = 0;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    file = malloc((size_t) size);
    if (!file || fread(file, 1, (size_t) size, f) != (size_t) size) {
        fclose(f);
        free(file);
        return 0;
    }
    fclose(f);
    pic = resource(file, (size_t) size, "CvPc", 200, &length);
    if (pic && length > 6 && be16(pic) == SHEET_W && be16(pic + 2) == SHEET_H && pic[4] >= 2 && pic[4] <= 8) {
        int colours = pic[5] + 1;
        p = 6 + (uint32_t) colours * 3;
        stream = malloc(length);
        sheet = malloc(SHEET_W * SHEET_H);
        if (stream && sheet && colours <= 128) {
            for (int i = 0; i < colours; i++) {
                const uint8_t *c = pic + 6 + i * 3;
                reciv_skin_palette[i] = (uint32_t) (c[0] << 16 | c[1] << 8 | c[2]);
            }
            reciv_skin_colours = colours;
            while (p < length && pic[p]) {
                uint32_t k = pic[p++];
                if (p + k > length)
                    break;
                memcpy(stream + n, pic + p, k);
                n += k;
                p += k;
            }
            ok = lzw(stream, n, pic[4], sheet, SHEET_W * SHEET_H);
        }
        free(stream);
    }
    free(file);
    if (!ok) {
        free(sheet);
        sheet = NULL;
        reciv_log("%s: no WinCiv SPR32X32 picture; playing without the skin", path);
        return 0;
    }
    wash();
    reciv_skin_enable();
    return 1;
}

/* The parts of each picture the sheet has, in the picture's pixels: x, y,
 * w, h. The rest (the DOS GO cursor, mouse pointers, city walls, status
 * bars, the 16-tile ocean row) the sheet leaves out or draws elsewhere. */
struct part { int16_t x, y, w, h; };
static const struct part ter257[] = {
    { 0, 0, 256, 160 },                 /* landscape: 10 terrains x 16 neighbour masks */
    { 0, 176, 192, 16 },                /* coast quarters, river mouths */
    { 0, 0, 0, 0 }
};
static const struct part sp257[] = {
    { 48, 32, 64, 16 },                 /* village, irrigation, mine, pollution */
    { 0, 48, 128, 16 },                 /* roads */
    { 16, 64, 240, 32 },                /* rivers, both sets */
    { 0, 96, 256, 16 },                 /* dotted pieces, railways */
    { 0, 112, 256, 16 },                /* specials, city, walls, fortress, hut */
    { 0, 128, 144, 16 },                /* citizens, unexplored edges */
    { 0, 160, 320, 32 },                /* units */
    { 0, 0, 0, 0 }
};
static const struct part sp299[] = {
    { 224, 100, 96, 16 },               /* the six full ocean tiles */
    { 224, 120, 96, 16 },               /* land base and city ground */
    { 0, 0, 0, 0 }
};

/* FILEIO.C's load_screen, after the picture is on page `page`. */
void civwasm_skin_picture(int16_t page, const char *name)
{
    const struct part *parts;
    int ox, oy;
    const char *base = strrchr(name, '\\');
    base = base ? base + 1 : name;
    if (!sheet || page < 0)
        return;
    if (!strcasecmp(base, "ter257.pic"))
        parts = ter257, ox = 640, oy = 0;
    else if (!strcasecmp(base, "sp257.pic"))
        parts = sp257, ox = 0, oy = 0;
    else if (!strcasecmp(base, "sp299.pic"))
        parts = sp299, ox = 832, oy = -100;
    else
        return;
    for (; parts->w; parts++)
        for (int y = 0; y < parts->h * 2; y++) {
            int sy = oy + parts->y * 2 + y;
            uint8_t row[640];
            if (sy < 0 || sy >= SHEET_H)
                continue;
            for (int x = 0; x < parts->w * 2; x++) {
                uint8_t c = sheet[sy * SHEET_W + ox + parts->x * 2 + x];
                row[x] = c == SHEET_KEY ? 0 : (uint8_t) (c + 1);
            }
            reciv_skin_row((uint16_t) page, parts->x * 2, parts->y * 2 + y, parts->w * 2, row);
        }
}
