/* mem.c: real-mode memory, DOS's allocator, and far pointers.
 *
 * reciv_mem is the PC's first megabyte. DOS memory blocks, pages, sprites
 * and loaded drivers live in it at real segment numbers, so segment
 * arithmetic (sprites packed paragraph by paragraph in an arena, freeing
 * an arena through its first sprite) works as on DOS.
 *
 * The game's own data is ordinary C variables. FP_SEG of a native pointer
 * gives a segment number from C000h up, standing for a host block in a
 * table; MK_FP and movedata accept both
 * kinds. The range C000h-CFFFh is ROM space on a PC, never allocated. */
#include <string.h>

#include "reciv.h"

uint8_t reciv_mem[0x110000];

/* ------------------------------------------------------- DOS memory */

/* Where CIV.EXE sits. DOS loads the program's PSP at PSP_SEG; MSC's
 * start-up code keeps the block up to the end of DGROUP (load segment +
 * 2B01h for DGROUP + F0Dh paragraphs of DGROUP and stack, from CIV.MAP)
 * and frees the rest. */
#define PSP_SEG 0x01A0
#define PROGRAM_PARAS (0x10 + 0x2B01 + 0x0F0D)
#define MEMORY_TOP 0xA000               /* first paragraph past conventional memory */

static uint8_t *mcb(uint16_t seg) { return REAL(seg, 0); }

static void mcb_set(uint16_t seg, char kind, uint16_t owner, uint16_t size)
{
    uint8_t *m = mcb(seg);
    m[0] = (uint8_t) kind;
    poke16(m + 1, owner);
    poke16(m + 3, size);
}

void reciv_mem_init(void)
{
    uint16_t program_mcb = PSP_SEG - 1;
    uint16_t free_mcb = PSP_SEG + PROGRAM_PARAS;
    memset(reciv_mem, 0, sizeof reciv_mem);
    mcb_set(program_mcb, 'M', PSP_SEG, PROGRAM_PARAS);
    mcb_set(free_mcb, 'Z', 0, (uint16_t) (MEMORY_TOP - free_mcb - 1));
    /* BIOS data area: the keyboard buffer pointers and equipment word. */
    poke16(REAL(0x40, 0x10), 0x0026);   /* equipment: 80x25 colour, no floppies beyond A: */
}

/* Merge free blocks that follow the free block at `seg`. */
static void coalesce(uint16_t seg)
{
    uint8_t *m = mcb(seg);
    while (m[0] == 'M') {
        uint16_t next = (uint16_t) (seg + peek16(m + 3) + 1);
        uint8_t *n = mcb(next);
        if ((n[0] != 'M' && n[0] != 'Z') || peek16(n + 1) != 0)
            break;
        mcb_set(seg, (char) n[0], 0, (uint16_t) (peek16(m + 3) + peek16(n + 3) + 1));
    }
}

uint16_t reciv_dos_alloc(uint16_t paragraphs, uint16_t *seg, uint16_t *max)
{
    uint16_t largest = 0;
    for (uint16_t s = PSP_SEG - 1;;) {
        uint8_t *m = mcb(s);
        if (m[0] != 'M' && m[0] != 'Z')
            return 7;                   /* memory control blocks destroyed */
        if (peek16(m + 1) == 0) {
            coalesce(s);
            uint16_t size = peek16(m + 3);
            if (size >= paragraphs) {
                if (size > paragraphs) {
                    mcb_set((uint16_t) (s + paragraphs + 1), (char) m[0], 0, (uint16_t) (size - paragraphs - 1));
                    m[0] = 'M';
                }
                poke16(m + 1, PSP_SEG);
                poke16(m + 3, paragraphs);
                *seg = (uint16_t) (s + 1);
                return 0;
            }
            if (size > largest)
                largest = size;
        }
        if (m[0] == 'Z')
            break;
        s = (uint16_t) (s + peek16(m + 3) + 1);
    }
    if (max)
        *max = largest;
    return 8;
}

static int valid_block(uint16_t seg)
{
    uint8_t *m = mcb((uint16_t) (seg - 1));
    return seg > 0 && (m[0] == 'M' || m[0] == 'Z');
}

uint16_t reciv_dos_free(uint16_t seg)
{
    if (!valid_block(seg))
        return 9;                       /* invalid memory block address */
    poke16(mcb((uint16_t) (seg - 1)) + 1, 0);
    return 0;
}

uint16_t reciv_dos_resize(uint16_t seg, uint16_t paragraphs, uint16_t *max)
{
    uint16_t s = (uint16_t) (seg - 1), size;
    uint8_t *m;
    if (!valid_block(seg))
        return 9;
    m = mcb(s);
    size = peek16(m + 3);
    if (paragraphs > size && m[0] == 'M') {
        uint16_t next = (uint16_t) (s + size + 1);
        if (peek16(mcb(next) + 1) == 0) {
            coalesce(next);
            mcb_set(s, (char) mcb(next)[0], peek16(m + 1), (uint16_t) (size + peek16(mcb(next) + 3) + 1));
            size = peek16(m + 3);
        }
    }
    if (paragraphs > size) {
        if (max)
            *max = size;
        return 8;
    }
    if (paragraphs < size) {
        mcb_set((uint16_t) (s + paragraphs + 1), (char) m[0], 0, (uint16_t) (size - paragraphs - 1));
        m[0] = 'M';
        poke16(m + 3, paragraphs);
        coalesce((uint16_t) (s + paragraphs + 1));
    }
    return 0;
}

/* ---------------------------------------------------- native segments */

#define NATIVE_FIRST 0xC000
#define NATIVE_COUNT 0x1000
#define NATIVE_SPAN 0x10000

static struct native {
    unsigned char *base;
} natives[NATIVE_COUNT];

static uint16_t native_register(unsigned char *base)
{
    for (int i = 0; i < NATIVE_COUNT; i++)
        if (!natives[i].base) {
            natives[i].base = base;
            return (uint16_t) (NATIVE_FIRST + i);
        }
    reciv_fatal("native segment table full");
}

static int native_find(const unsigned char *p)
{
    for (int i = 0; i < NATIVE_COUNT && natives[i].base; i++)
        if (p >= natives[i].base && p < natives[i].base + NATIVE_SPAN)
            return i;
    return -1;
}

uint16_t reciv_seg_wrap(void *base, uint32_t size)
{
    (void) size;
    return native_register(base);
}

void *reciv_seg_ptr(uint16_t seg, uint16_t off)
{
    if (seg >= NATIVE_FIRST && seg < NATIVE_FIRST + NATIVE_COUNT) {
        if (!natives[seg - NATIVE_FIRST].base)
            reciv_fatal("segment %04X is not in use", seg);
        return natives[seg - NATIVE_FIRST].base + off;
    }
    return REAL(seg, off);
}

static int in_real(const unsigned char *p)
{
    return p >= reciv_mem && p < reciv_mem + sizeof reciv_mem;
}

uint16_t crt_fp_seg(void *p)
{
    int i;
    if (in_real(p))
        return (uint16_t) (((unsigned char *) p - reciv_mem) >> 4);
    i = native_find(p);
    return i >= 0 ? (uint16_t) (NATIVE_FIRST + i) : native_register(p);
}

uint16_t crt_fp_off(void *p)
{
    int i;
    if (in_real(p))
        return (uint16_t) (((unsigned char *) p - reciv_mem) & 15);
    i = native_find(p);
    return i >= 0 ? (uint16_t) ((unsigned char *) p - natives[i].base) : 0;
}

void *crt_mk_fp(uint16_t seg, uint16_t off)
{
    return reciv_seg_ptr(seg, off);
}

void crt_movedata(uint16_t sseg, uint16_t soff, uint16_t dseg, uint16_t doff, uint16_t n)
{
    /* REP MOVSB: a forward byte copy. */
    unsigned char *d = reciv_seg_ptr(dseg, doff);
    const unsigned char *s = reciv_seg_ptr(sseg, soff);
    while (n--)
        *d++ = *s++;
}

uint16_t crt_dos_freemem(uint16_t seg)
{
    return reciv_dos_free(seg);
}
