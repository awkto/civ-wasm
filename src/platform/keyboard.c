/* keyboard.c: the BIOS keyboard, DOS console input and MISC.EXE.
 *
 * Key presses become BIOS keystrokes (scan code << 8 | character), as the
 * PC/AT BIOS's INT 9 handler made them for a US keyboard, in a 15-key
 * buffer like the BIOS's. Keys are taken by their position, so the game's
 * keys are where they were on a US keyboard whatever the host's layout.
 *
 * The game reads them through MISC.EXE's slots (90-97: INT 16h and DOS
 * function 1) and MSC's kbhit and getch (DOS functions 0Bh and 07h). The
 * keypad sends cursor keys, as with Num Lock off: the game turns Num Lock
 * off at start-up. */
#include <stdlib.h>

#include <SDL3/SDL.h>

#include "reciv.h"

/* ------------------------------------------------------ BIOS keystrokes */

#define BUFFER_KEYS 15
static uint16_t buffer[BUFFER_KEYS + 1];
static int head, tail;

void reciv_key_event(uint16_t code)
{
    int next = (tail + 1) % (BUFFER_KEYS + 1);
    if (next == head)
        return;                         /* buffer full: the BIOS beeps */
    buffer[tail] = code;
    tail = next;
}

static int key_waiting(void)
{
    reciv_spin();
    return head != tail;
}

static uint16_t key_take(void)
{
    uint16_t code;
    reciv_poll();
    while (head == tail)
        reciv_wait();
    code = buffer[head];
    head = (head + 1) % (BUFFER_KEYS + 1);
    return code;
}

/* Keystroke for a key on a US keyboard, by position. */
struct key {
    SDL_Scancode sdl;
    uint8_t scan;
    char plain, shifted;
};

static const struct key keys[] = {
    { SDL_SCANCODE_ESCAPE, 0x01, 27, 27 },
    { SDL_SCANCODE_1, 0x02, '1', '!' }, { SDL_SCANCODE_2, 0x03, '2', '@' },
    { SDL_SCANCODE_3, 0x04, '3', '#' }, { SDL_SCANCODE_4, 0x05, '4', '$' },
    { SDL_SCANCODE_5, 0x06, '5', '%' }, { SDL_SCANCODE_6, 0x07, '6', '^' },
    { SDL_SCANCODE_7, 0x08, '7', '&' }, { SDL_SCANCODE_8, 0x09, '8', '*' },
    { SDL_SCANCODE_9, 0x0A, '9', '(' }, { SDL_SCANCODE_0, 0x0B, '0', ')' },
    { SDL_SCANCODE_MINUS, 0x0C, '-', '_' }, { SDL_SCANCODE_EQUALS, 0x0D, '=', '+' },
    { SDL_SCANCODE_BACKSPACE, 0x0E, 8, 8 }, { SDL_SCANCODE_TAB, 0x0F, 9, 0 },
    { SDL_SCANCODE_Q, 0x10, 'q', 'Q' }, { SDL_SCANCODE_W, 0x11, 'w', 'W' },
    { SDL_SCANCODE_E, 0x12, 'e', 'E' }, { SDL_SCANCODE_R, 0x13, 'r', 'R' },
    { SDL_SCANCODE_T, 0x14, 't', 'T' }, { SDL_SCANCODE_Y, 0x15, 'y', 'Y' },
    { SDL_SCANCODE_U, 0x16, 'u', 'U' }, { SDL_SCANCODE_I, 0x17, 'i', 'I' },
    { SDL_SCANCODE_O, 0x18, 'o', 'O' }, { SDL_SCANCODE_P, 0x19, 'p', 'P' },
    { SDL_SCANCODE_LEFTBRACKET, 0x1A, '[', '{' }, { SDL_SCANCODE_RIGHTBRACKET, 0x1B, ']', '}' },
    { SDL_SCANCODE_RETURN, 0x1C, 13, 13 },
    { SDL_SCANCODE_A, 0x1E, 'a', 'A' }, { SDL_SCANCODE_S, 0x1F, 's', 'S' },
    { SDL_SCANCODE_D, 0x20, 'd', 'D' }, { SDL_SCANCODE_F, 0x21, 'f', 'F' },
    { SDL_SCANCODE_G, 0x22, 'g', 'G' }, { SDL_SCANCODE_H, 0x23, 'h', 'H' },
    { SDL_SCANCODE_J, 0x24, 'j', 'J' }, { SDL_SCANCODE_K, 0x25, 'k', 'K' },
    { SDL_SCANCODE_L, 0x26, 'l', 'L' }, { SDL_SCANCODE_SEMICOLON, 0x27, ';', ':' },
    { SDL_SCANCODE_APOSTROPHE, 0x28, '\'', '"' }, { SDL_SCANCODE_GRAVE, 0x29, '`', '~' },
    { SDL_SCANCODE_BACKSLASH, 0x2B, '\\', '|' },
    { SDL_SCANCODE_Z, 0x2C, 'z', 'Z' }, { SDL_SCANCODE_X, 0x2D, 'x', 'X' },
    { SDL_SCANCODE_C, 0x2E, 'c', 'C' }, { SDL_SCANCODE_V, 0x2F, 'v', 'V' },
    { SDL_SCANCODE_B, 0x30, 'b', 'B' }, { SDL_SCANCODE_N, 0x31, 'n', 'N' },
    { SDL_SCANCODE_M, 0x32, 'm', 'M' }, { SDL_SCANCODE_COMMA, 0x33, ',', '<' },
    { SDL_SCANCODE_PERIOD, 0x34, '.', '>' }, { SDL_SCANCODE_SLASH, 0x35, '/', '?' },
    { SDL_SCANCODE_KP_MULTIPLY, 0x37, '*', '*' }, { SDL_SCANCODE_SPACE, 0x39, ' ', ' ' },
    { SDL_SCANCODE_KP_MINUS, 0x4A, '-', '-' }, { SDL_SCANCODE_KP_PLUS, 0x4E, '+', '+' },
    { SDL_SCANCODE_KP_ENTER, 0x1C, 13, 13 }, { SDL_SCANCODE_KP_DIVIDE, 0x35, '/', '/' },
};

/* Keys without a character: cursor keys, the keypad (Num Lock off) and
 * function keys. Scan codes for plain, Ctrl and Alt. */
struct control_key {
    SDL_Scancode sdl;
    uint8_t plain, shift, ctrl, alt;
};

static const struct control_key control_keys[] = {
    { SDL_SCANCODE_HOME, 0x47, 0x47, 0x77, 0 }, { SDL_SCANCODE_KP_7, 0x47, 0x47, 0x77, 0 },
    { SDL_SCANCODE_UP, 0x48, 0x48, 0, 0 }, { SDL_SCANCODE_KP_8, 0x48, 0x48, 0, 0 },
    { SDL_SCANCODE_PAGEUP, 0x49, 0x49, 0x84, 0 }, { SDL_SCANCODE_KP_9, 0x49, 0x49, 0x84, 0 },
    { SDL_SCANCODE_LEFT, 0x4B, 0x4B, 0x73, 0 }, { SDL_SCANCODE_KP_4, 0x4B, 0x4B, 0x73, 0 },
    { SDL_SCANCODE_RIGHT, 0x4D, 0x4D, 0x74, 0 }, { SDL_SCANCODE_KP_6, 0x4D, 0x4D, 0x74, 0 },
    { SDL_SCANCODE_END, 0x4F, 0x4F, 0x75, 0 }, { SDL_SCANCODE_KP_1, 0x4F, 0x4F, 0x75, 0 },
    { SDL_SCANCODE_DOWN, 0x50, 0x50, 0, 0 }, { SDL_SCANCODE_KP_2, 0x50, 0x50, 0, 0 },
    { SDL_SCANCODE_PAGEDOWN, 0x51, 0x51, 0x76, 0 }, { SDL_SCANCODE_KP_3, 0x51, 0x51, 0x76, 0 },
    { SDL_SCANCODE_INSERT, 0x52, 0x52, 0, 0 }, { SDL_SCANCODE_KP_0, 0x52, 0x52, 0, 0 },
    { SDL_SCANCODE_DELETE, 0x53, 0x53, 0, 0 }, { SDL_SCANCODE_KP_PERIOD, 0x53, 0x53, 0, 0 },
    { SDL_SCANCODE_F1, 0x3B, 0x54, 0x5E, 0x68 }, { SDL_SCANCODE_F2, 0x3C, 0x55, 0x5F, 0x69 },
    { SDL_SCANCODE_F3, 0x3D, 0x56, 0x60, 0x6A }, { SDL_SCANCODE_F4, 0x3E, 0x57, 0x61, 0x6B },
    { SDL_SCANCODE_F5, 0x3F, 0x58, 0x62, 0x6C }, { SDL_SCANCODE_F6, 0x40, 0x59, 0x63, 0x6D },
    { SDL_SCANCODE_F7, 0x41, 0x5A, 0x64, 0x6E }, { SDL_SCANCODE_F8, 0x42, 0x5B, 0x65, 0x6F },
    { SDL_SCANCODE_F9, 0x43, 0x5C, 0x66, 0x70 }, { SDL_SCANCODE_F10, 0x44, 0x5D, 0x67, 0x71 },
};

/* Alt with a digit or the top row: scan codes 78h-83h. */
static const SDL_Scancode alt_digits[] = {
    SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6,
    SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0, SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS,
};

uint16_t reciv_key_code(SDL_Scancode sc, SDL_Keymod mod)
{
    int shift = (mod & SDL_KMOD_SHIFT) != 0, ctrl = (mod & SDL_KMOD_CTRL) != 0, alt = (mod & SDL_KMOD_ALT) != 0;
    int caps = (mod & SDL_KMOD_CAPS) != 0;
    for (size_t i = 0; i < sizeof control_keys / sizeof *control_keys; i++) {
        const struct control_key *k = &control_keys[i];
        if (k->sdl == sc) {
            uint8_t scan = alt ? k->alt : ctrl ? k->ctrl : shift ? k->shift : k->plain;
            return (uint16_t) (scan << 8);
        }
    }
    if (sc == SDL_SCANCODE_KP_5)
        return 0;                       /* nothing with Num Lock off */
    if (alt) {
        for (size_t i = 0; i < sizeof alt_digits / sizeof *alt_digits; i++)
            if (alt_digits[i] == sc)
                return (uint16_t) ((0x78 + i) << 8);
    }
    for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) {
        const struct key *k = &keys[i];
        char c;
        if (k->sdl != sc)
            continue;
        if (alt)
            return (uint16_t) (k->scan << 8);
        if (ctrl) {
            if (k->plain >= 'a' && k->plain <= 'z')
                return (uint16_t) (k->scan << 8 | (k->plain - 'a' + 1));
            if (sc == SDL_SCANCODE_RETURN)
                return 0x1C0A;
            if (sc == SDL_SCANCODE_BACKSPACE)
                return 0x0E7F;
            return 0;
        }
        c = shift ? k->shifted : k->plain;
        if (caps && k->plain >= 'a' && k->plain <= 'z')
            c = shift ? k->plain : k->shifted;
        if (sc == SDL_SCANCODE_TAB && shift)
            return 0x0F00;
        return (uint16_t) (k->scan << 8 | (uint8_t) c);
    }
    return 0;
}

/* ------------------------------------------------------- MISC.EXE slots */

/* Slot 90: 0 when a key is waiting, else -1. */
int16_t key_none(void)
{
    reciv_require_slot(90);
    return key_waiting() ? 0 : -1;
}

/* Slot 91: wait for a keystroke (INT 16h function 0). */
int16_t read_key(void)
{
    reciv_require_slot(91);
    return (int16_t) key_take();
}

/* DOS console input (functions 1, 7, 8): characters, with an extended
 * key read as 0 and then its scan code. */
static int pending_scan = -1;

static uint8_t console_char(void)
{
    uint16_t code;
    if (pending_scan >= 0) {
        uint8_t c = (uint8_t) pending_scan;
        pending_scan = -1;
        return c;
    }
    code = key_take();
    if ((code & 0xFF) == 0)
        pending_scan = code >> 8;
    return (uint8_t) code;
}

/* Slot 92: wait for a key and echo it (DOS function 1); AH stays 1. */
int16_t key_echo(void)
{
    uint8_t c;
    reciv_require_slot(92);
    c = console_char();
    reciv_text_teletype(c);
    return (int16_t) (0x0100 | c);
}

/* Slots 93, 94: MISC masks 0417h with F0h, clearing the modifier bits.
 * Its historical "locks_off" name does not describe that mask. */
int16_t locks_off(void)
{
    uint8_t *flags = REAL(0, 0x417);
    *flags &= 0xF0;
    return *flags;
}

/* Slots 95-97: no joystick. A missing joystick's port reads FFh: no
 * buttons held and both axes timing out at zero. */
int16_t stick_button(int16_t n)
{
    (void) n;
    return 0;
}

int16_t crt_kbhit(void)
{
    return (int16_t) (pending_scan >= 0 || key_waiting() ? -1 : 0);
}

int16_t crt_getch(void)
{
    return console_char();
}
