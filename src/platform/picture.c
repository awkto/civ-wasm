/* picture.c: the picture codec (PICTURE.INC), in C.
 *
 * Pictures (.PIC) and maps are a chunk stream: "M0" palette chunks and
 * other tables, then an "X0"/"X1" image chunk whose pixels are run-length
 * coded (90h escapes a count) and then LZW coded (codes of 9 to 11 bits,
 * restarting when full). X1 pictures pack two 4-bit pixels per byte.
 *
 * Input is read through the game's stream: file_read points into a
 * 512-byte block that ends at image_buffer, and pic_reader refills it
 * (FILEIO.C). The decoder's string table is the game's msgtext block
 * (DGROUP BA06h), whose palette buffers it overwrites as in the original:
 * prefix word at 3n, byte at 3n + 2. */
#include <string.h>

#include "reciv.h"

extern char *file_read;                 /* the stream's read pointer */
extern char *image_buffer;              /* end of the input block */
extern int16_t (*pic_reader)(void);     /* refills the block */
extern int16_t picture_width, picture_height, picture_bytes;
void apply_palette(const uint8_t *record);

extern char msgtext[];                  /* the game's 6144-byte scratch block */
#define PICTURE_TABLE ((uint8_t *) msgtext)
#define PREFIX(code) (PICTURE_TABLE + (uint16_t) (code) * 3)
#define SUFFIX(code) (PICTURE_TABLE + 2 + (uint16_t) (code) * 3)

static uint16_t read_word(void)
{
    uint16_t w;
    if (file_read >= image_buffer)
        pic_reader();
    w = (uint16_t) ((uint8_t) file_read[0] | (uint8_t) file_read[1] << 8);
    file_read += 2;
    return w;
}

/* ------------------------------------------------------------ decoder */

static uint8_t nibbles, repeat_count, repeat_byte;
static uint8_t code_bits, code_limit, word_bits, first_byte;
static uint16_t code_mask, next_code, code_word, previous;
/* The output stack of the code being expanded (the original's is the
 * 512 bytes from DGROUP 68FAh; a longer chain overran it). */
static uint8_t stack[4096];
static int depth;

static void reset_codes(void)
{
    code_bits = 9;
    code_mask = 0x1FF;
    next_code = 0x100;
    for (int i = 0; i < 0x800; i++)
        poke16(PREFIX(i), 0xFFFF);
    for (int i = 0; i < 0x100; i++)
        *SUFFIX(i) = (uint8_t) i;
}

static void decode_init(void)
{
    uint16_t w;
    if (!(picture_width | picture_height))
        return;
    repeat_count = 0;
    repeat_byte = 0;
    depth = 0;
    w = read_word();
    code_limit = (uint8_t) w > 11 ? 11 : (uint8_t) w;
    code_word = w;
    word_bits = 8;
    reset_codes();
}

static void push(uint8_t b)
{
    if (depth < (int) sizeof stack)
        stack[depth++] = b;
}

/* Next byte of the LZW output. */
static uint8_t decode_next(void)
{
    uint16_t bx, code, cx, ax;
    uint8_t cl;
    if (depth)
        return stack[--depth];
    bx = (uint16_t) (code_word >> (16 - word_bits));
    cl = word_bits;
    while (cl < code_bits) {
        uint16_t w = read_word();
        code_word = w;
        bx |= (uint16_t) (w << cl);
        cl = (uint8_t) (cl + 16);
    }
    word_bits = (uint8_t) (cl - code_bits);
    code = bx & code_mask;
    cx = code;
    ax = code;
    if ((int16_t) code >= (int16_t) next_code) {
        /* A code not yet in the table: the previous string plus its first
         * byte. */
        cx = next_code;
        ax = previous;
        push(first_byte);
    }
    while (peek16(PREFIX(ax)) != 0xFFFF) {
        push(*SUFFIX(ax));
        ax = peek16(PREFIX(ax));
    }
    first_byte = *SUFFIX(ax);
    push(first_byte);
    *SUFFIX(next_code) = first_byte;
    poke16(PREFIX(next_code), previous);
    next_code++;
    if ((int16_t) next_code > (int16_t) code_mask) {
        code_bits++;
        code_mask = (uint16_t) (code_mask << 1 | 1);
    }
    if ((int8_t) code_bits > (int8_t) code_limit)
        reset_codes();
    previous = cx;
    return stack[--depth];
}

/* Decode `count` pixels to `out`, undoing the run-length coding. */
static void decode_bytes(uint8_t *out, uint16_t count)
{
    if (nibbles)
        count = (uint16_t) ((uint16_t) (count + 1) >> 1);
    do {
        uint8_t al;
        if (repeat_count) {
            al = repeat_byte;
            repeat_count--;
        } else {
            al = decode_next();
            if (al == 0x90) {
                al = decode_next();
                if (!al) {
                    al = 0x90;
                    repeat_byte = al;
                } else {
                    repeat_count = (uint8_t) (al - 1);
                    al = repeat_byte;
                    repeat_count--;
                }
            } else
                repeat_byte = al;
        }
        if (nibbles) {
            *out++ = al & 0x0F;
            *out++ = (uint8_t) (al >> 4);
        } else
            *out++ = al;
    } while (--count);
}

/* picture_target(mode): read chunks up to the image. An "M0" palette goes
 * to the picture table at BA06h and is applied (mode 1), or is kept at
 * BA08h unapplied (mode 0), or goes to the near buffer `mode` (from 2);
 * other tables go to BA06h. Then reads the image header. */
void picture_target(const char *mode)
{
    apply_palette(NULL);
    for (;;) {
        uint16_t tag = read_word(), length;
        uint8_t *dst = PICTURE_TABLE;
        if ((tag & 0xFF) == 0x58) {
            nibbles = (uint8_t) ((tag >> 8) & 1);
            picture_bytes = (int16_t) read_word();
            picture_width = (int16_t) read_word();
            picture_height = (int16_t) read_word();
            decode_init();
            return;
        }
        if (tag == 0x304D) {
            if (!mode)
                dst = PICTURE_TABLE + 2;
            else if (mode != (const char *) 1)
                dst = (uint8_t *) mode;
        }
        uint8_t *start = dst;
        poke16(dst, tag);
        dst += 2;
        length = read_word();
        poke16(dst, length);
        dst += 2;
        for (uint16_t n = length >> 1; n; n--) {
            poke16(dst, read_word());
            dst += 2;
        }
        if (start == PICTURE_TABLE)
            apply_palette(start);
    }
}

/* begin_picture: start a picture with no palette chunks (a stored
 * block): mode word and size, then the decoder. */
void begin_picture(void)
{
    nibbles = (uint8_t) (read_word() & 1);
    picture_bytes = (int16_t) read_word();
    decode_init();
}

/* copy_picture(offset, segment, count): decode to far memory. */
void copy_picture(int16_t offset, int16_t segment, int16_t count)
{
    decode_bytes(reciv_seg_ptr((uint16_t) segment, (uint16_t) offset), (uint16_t) count);
}

/* decode_scanline(row): decode one row of picture_width pixels. */
void decode_scanline(uint8_t *row)
{
    decode_bytes(row, (uint16_t) picture_width);
}

/* ------------------------------------------------------------ encoder */

/* image_begin, image_byte and image_finish write a picture file (IMAGE.C
 * saves the world map this way). The string table is a hash table of
 * 4091 entries (character, prefix code, code) in a DOS block; the string
 * being matched is built over the picture table at BA06h. Output goes
 * through file_read into the 512-byte block at D936h (diskbuffer), which
 * ends at image_buffer. */
extern unsigned char diskbuffer[];
#define OUTPUT_BUFFER ((uint8_t *) diskbuffer)
#define TABLE_SIZE 0xFFB

static int16_t image_handle;
static uint16_t encode_segment, encode_status, encode_previous, encode_next_code;
static uint16_t string_end;             /* offset in the picture table */
static uint8_t encode_bits, encode_byte, encode_width;

static uint8_t *entry(uint16_t i)
{
    return REAL(encode_segment, i * 6);
}

static void encode_init(void)
{
    uint16_t i;
    for (i = 0; i < 0x100; i++) {
        poke16(entry(i), i);
        poke16(entry(i) + 2, 0xFFFF);
        poke16(entry(i) + 4, i);
    }
    for (; i < TABLE_SIZE; i++)
        poke16(entry(i), 0xFFFF);
    encode_width = 9;
    encode_next_code = 0x101;
}

static void write_output(void)
{
    reciv_write(image_handle, OUTPUT_BUFFER, (uint16_t) ((uint8_t *) file_read - OUTPUT_BUFFER));
}

/* Append the low encode_width bits of `code`, least significant first. */
static void encode_bits_out(uint16_t code)
{
    uint8_t dl = encode_bits, dh = encode_byte;
    for (int8_t cl = (int8_t) encode_width; --cl >= 0;) {
        if (dl >= 8) {
            *file_read++ = (char) dh;
            dl = dh = 0;
            if (file_read >= image_buffer) {
                write_output();
                file_read = (char *) OUTPUT_BUFFER;
            }
        }
        dh = (uint8_t) (dh >> 1 | (code & 1) << 7);
        code >>= 1;
        dl++;
    }
    encode_bits = dl;
    encode_byte = dh;
}

/* Look up the string (prefix code, then character c); the hash is the
 * sum of the string's words. Returns the entry, or FFFFh with *slot set
 * to the free entry where it would go. */
static uint16_t encode_find(uint16_t c, uint16_t prefix, uint16_t *slot)
{
    uint16_t bx = 0, di = 0, si = string_end, dx;
    PICTURE_TABLE[si] = 0;
    while (di < si) {
        bx = (uint16_t) (bx + peek16(PICTURE_TABLE + di));
        di += 2;
    }
    bx &= 0x7FFF;
    if (!bx) {
        for (uint16_t n = si; n; n--)
            bx = (uint16_t) (bx + 0x25);
        bx &= 0x7FFF;
    }
    dx = bx;
    while (dx >= 0xFF9)
        dx = (uint16_t) (dx - 0xFF9);
    dx = (uint16_t) (0xFF9 - dx);
    for (;;) {
        while (bx >= TABLE_SIZE)
            bx = (uint16_t) (bx - TABLE_SIZE);
        if (peek16(entry(bx)) == 0xFFFF) {
            *slot = bx;
            return 0xFFFF;
        }
        if (peek16(entry(bx)) == c && peek16(entry(bx) + 2) == prefix)
            return bx;
        bx = (uint16_t) (bx + dx);
    }
}

/* image_begin(name, width, height): create the file and start an X0
 * picture. Returns 0, or the DOS error. */
int16_t image_begin(const char *name, int16_t width, int16_t height)
{
    uint16_t max;
    image_handle = reciv_create(name);
    if (image_handle < 0)
        return image_handle = 5;        /* access denied */
    if (reciv_dos_alloc(0x5FF, &encode_segment, &max))
        return (int16_t) (encode_segment = 8);
    poke16(OUTPUT_BUFFER, 0x3058);
    poke16(OUTPUT_BUFFER + 2, 0);
    poke16(OUTPUT_BUFFER + 4, (uint16_t) width);
    poke16(OUTPUT_BUFFER + 6, (uint16_t) height);
    file_read = (char *) OUTPUT_BUFFER + 8;
    encode_bits = encode_byte = 0;
    encode_status = 0xFFFF;
    return 0;
}

void image_byte(int16_t value)
{
    uint16_t c = (uint16_t) value, bp = encode_previous, found, slot = 0;
    if (encode_status == 0xFFFF) {
        encode_status = 0;
        PICTURE_TABLE[0] = (uint8_t) c;
        string_end = 1;
        encode_previous = c;
        *file_read++ = 11;              /* the code size limit */
        encode_init();
        return;
    }
    PICTURE_TABLE[string_end] = (uint8_t) c;
    string_end++;
    found = encode_find(c, bp, &slot);
    if (found != 0xFFFF) {
        encode_previous = found;
        return;
    }
    /* Not in the table: add it, emit the previous string's code, and
     * start a new string with this character. */
    PICTURE_TABLE[0] = (uint8_t) c;
    string_end = 1;
    encode_previous = c;
    poke16(entry(slot), c);
    poke16(entry(slot) + 2, bp);
    poke16(entry(slot) + 4, encode_next_code);
    encode_next_code++;
    encode_bits_out(peek16(entry(bp) + 4));
    if (encode_next_code > (uint16_t) (1u << encode_width)) {
        encode_width++;
        if (encode_width > 11)
            encode_init();
    }
}

int16_t image_finish(void)
{
    uint16_t status = 0;
    uint8_t size[2];
    int32_t end;
    encode_bits_out(peek16(entry(encode_previous) + 4));
    encode_width = 8;
    encode_bits_out(0);
    write_output();
    if (reciv_dos_free(encode_segment))
        status = 9;
    end = reciv_lseek(image_handle, 0, 1);
    if (end < 0)
        status = 6;
    poke16(size, (uint16_t) (end - 4));
    if (reciv_lseek(image_handle, 2, 0) < 0)
        status = 6;
    if (reciv_write(image_handle, size, 2) != 2)
        status = 6;
    if (reciv_close(image_handle))
        status = 6;
    return (int16_t) status;
}
