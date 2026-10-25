/* timer.c: RESIDENT's timer interrupt (TIMER.INC), palette fades
 * (FADE.INC) and colour cycling (PALETTE.INC), run cooperatively.
 *
 * On DOS, resident_init reprogrammed the PIT so that INT 08h fired once
 * per video frame (it timed the vertical retrace; 70 Hz on VGA). Each tick
 * counted `ticks` (timer_read), stepped the palette fade, the colour
 * cycles and the screen shake, and called the sound driver; the BIOS's own
 * 18.2 Hz handler was chained from it.
 *
 * reciv has no interrupts. reciv_poll, called by every routine that waits
 * for time or input, runs the ticks that are due by the real clock, then
 * pumps window events and shows the screen. The game's busy-waits on
 * timer_read therefore advance as on DOS. */
#include <string.h>

#include <SDL3/SDL.h>

#include "reciv.h"

void reciv_pump_events(void);
void reciv_shake_step(void);

#define FRAME_NS 14268635ull            /* VGA mode 13h: 70.086 Hz */
#define BIOS_TICK_NS 54925493ull        /* PIT 1193182 / 65536 */
#define MAX_CATCH_UP_NS 2000000000ull   /* after a stall, run at most 2 s of ticks */

static int installed;
static uint16_t ticks;                  /* tm_ticks */
static uint64_t next_frame, next_bios;
static uint64_t next_fast = UINT64_MAX;
static unsigned fast_phase;
static uint32_t frames;
static int8_t tick_phase = 7;           /* tm_tick_phase */
static int fast_divisor = 4;            /* tm_fast_divisor: 4 at 70 Hz */
static int in_tick;

static void fade_tick(void);
static void palette_tick(void);

/* civ-wasm: --anim-speed. The game paces its full-screen animations by
 * `ticks` (timer_read: flip and the like) and palette fades by fade_tick;
 * both run reciv_anim_speed (in tenths) times as fast. Colour cycling,
 * the music and real time stay as they were. */
int reciv_anim_speed = 10;
static int anim_tenths;

/* TIMER.INC timer_tick: once per frame. */
static void timer_tick(uint64_t frame_time)
{
    int16_t r;
    for (anim_tenths += reciv_anim_speed; anim_tenths >= 10; anim_tenths -= 10) {
        ticks++;
        fade_tick();
    }
    palette_tick();
    reciv_shake_step();
    if (fast_divisor == 4) {
        tick_phase--;
        if (tick_phase <= 0) {
            tick_phase = 7;
            return;
        }
    }
    r = reciv_sound_tick();
    if (r < 0)
        next_fast = UINT64_MAX;
    else if (r > 0 && next_fast == UINT64_MAX) {
        /* TIMER.INC calls slot 104 in this same interrupt after the
         * music step asks for fast mode, then four times per VGA frame. */
        fast_phase = 0;
        next_fast = frame_time;
    }
}

/* The BIOS's INT 08h: time of day and the diskette motor count, which
 * page_dissolve uses to wait for a tick. */
static void bios_tick(void)
{
    uint8_t *motor = REAL(0x40, 0x40);
    uint32_t t = peek16(REAL(0x40, 0x6C)) | (uint32_t) peek16(REAL(0x40, 0x6E)) << 16;
    t++;
    poke16(REAL(0x40, 0x6C), (uint16_t) t);
    poke16(REAL(0x40, 0x6E), (uint16_t) (t >> 16));
    if (*motor)
        (*motor)--;
}

static uint64_t clock_now(void)
{
    return SDL_GetTicksNS();
}

/* Give up the processor briefly. */
static void idle(void)
{
    SDL_DelayNS(1000000);
}

uint64_t reciv_clock_ns(void)
{
    return clock_now();
}


void reciv_timer_init(void)
{
    uint64_t now = clock_now();
    next_frame = now + FRAME_NS;
    next_bios = now + BIOS_TICK_NS;
    next_fast = UINT64_MAX;
}

uint32_t reciv_vsync_count(void)
{
    return frames;
}

void reciv_poll(void)
{
    uint64_t now = clock_now();
    if (in_tick)
        return;
    in_tick = 1;
    if (now > next_frame + MAX_CATCH_UP_NS) {
        next_frame = now - MAX_CATCH_UP_NS;
        if (next_fast != UINT64_MAX) {
            next_fast = next_frame;
            fast_phase = 0;
        }
    }
    if (now > next_bios + MAX_CATCH_UP_NS)
        next_bios = now - MAX_CATCH_UP_NS;
    while (next_frame <= now || next_bios <= now || next_fast <= now) {
        if (next_frame <= next_bios && next_frame <= next_fast) {
            uint64_t frame_time = next_frame;
            next_frame += FRAME_NS;
            frames++;
            reciv_sound_render(frame_time);
            if (installed)
                timer_tick(frame_time);
        } else if (next_fast <= next_bios) {
            reciv_sound_render(next_fast);
            reciv_sound_fast_tick();
            fast_phase = (fast_phase + 1) % 4;
            /* Anchor subdivisions to frames so rounding cannot introduce
             * an extra sweep immediately before the next music tick. */
            next_fast = fast_phase ? next_frame - FRAME_NS + FRAME_NS * fast_phase / 4 : next_frame;
        } else {
            next_bios += BIOS_TICK_NS;
            bios_tick();
        }
    }
    reciv_sound_render(now);
    reciv_pump_events();
    in_tick = 0;
    reciv_video_present(0);
}

void reciv_wait(void)
{
    reciv_poll();
    idle();
}

/* --------------------------------------------------------- C entries */

void resident_init(void)
{
    if (installed)
        return;
    next_fast = UINT64_MAX;
    installed = 1;
}

void restore_timer(void)
{
    installed = 0;
    next_fast = UINT64_MAX;
}

/* The game polls (timer_read, sound_quiet_ticks, kbhit, mouse_poll) in
 * tight loops waiting for a tick. Once the frame count has stayed the
 * same for a while, the host processor is given back between polls. */
void reciv_spin(void)
{
    static uint32_t last;
    static int same;
    reciv_poll();
    if (frames != last)
        last = frames, same = 0;
    else if (++same > 64)
        idle();
}

int16_t timer_read(void)
{
    reciv_spin();
    return (int16_t) ticks;
}

int16_t timer_reset(void)
{
    uint16_t t;
    reciv_poll();
    t = ticks;
    ticks = 0;
    return (int16_t) t;
}

/* --------------------------------------------------------- palette fade */

/* FADE.INC. The fade's three palettes live in the game's msgtext block
 * (DGROUP BA06h), over the text and the palette buffers after it: the
 * target at its start, the starting palette at 300h and the next step's
 * mix at 600h. */
extern char msgtext[];
#define FD(off) ((uint8_t *) msgtext + (off))
#define FD_TARGET 0
#define FD_INITIAL 0x300
#define FD_MIXED 0x600

static uint16_t fd_start, fd_count, fd_pending, fd_fraction, fd_fraction_step;

/* The DAC bytes the fade could write per tick, measured once by timing
 * writes within one vertical retrace (fade_calibrate). A VGA writes the
 * whole palette in one retrace, so count = 256 colours per tick. */
#define FD_COUNT 0x100

static void fade_mix(void)
{
    uint16_t si, cx;
    fd_start = (uint16_t) ((fd_start + fd_count) & 0xFF);
    si = (uint16_t) (fd_start * 3);
    cx = (uint16_t) (fd_count * 3);
    for (uint8_t *di = FD(FD_MIXED); cx; cx--) {
        uint32_t sum = 0;
        uint8_t a = *FD(FD_INITIAL + si), b = *FD(FD_TARGET + si);
        if (a)
            sum = (uint32_t) a * (uint16_t) ~fd_fraction;
        if (b)
            sum += (uint32_t) b * fd_fraction;
        /* SHL BP,1 / ADC BX,0: round on bit 15 of the low word. */
        *di++ = (uint8_t) ((sum >> 16) + ((sum >> 15) & 1));
        si++;
        if (si >= 0x300) {
            si = 0;
            if ((uint32_t) fd_fraction + fd_fraction_step > 0xFFFF)
                fd_fraction = 0xFFFF;
            else
                fd_fraction = (uint16_t) (fd_fraction + fd_fraction_step);
        }
    }
}

static void fade_prepare(uint16_t time)
{
    uint16_t bp = (uint16_t) ((time & 0xFF) * 6);
    uint32_t product;
    uint16_t cx;
    fd_count = FD_COUNT;
    fd_start = (uint16_t) -fd_count;
    product = (uint32_t) fd_count * bp;
    cx = (uint16_t) ((product >> 8) & 0xFFFF);
    cx--;
    if (cx == 0)
        reciv_fatal("fade: divide overflow");    /* INT 0 on DOS */
    fd_fraction_step = (uint16_t) (0x10000u / cx);
    fd_fraction = fd_fraction_step;
    fade_mix();
    fd_pending = bp;
}

static void fade_capture(void)
{
    reciv_dac_read(0, 256, FD(FD_INITIAL));
}

static void fade_wait(void)
{
    while (fd_pending)
        reciv_wait();
}

static void fade_tick(void)
{
    if (!fd_pending)
        return;
    reciv_dac_write(fd_start & 0xFF, fd_count, FD(FD_MIXED));
    fd_pending--;
    fade_mix();
}

/* install_palette(time, record): fade from the current colours to an "M0"
 * palette record's 256 colours over time * 6 ticks. */
void install_palette(int16_t time, const uint8_t *record)
{
    memmove(FD(FD_TARGET), record + 6, 0x300);
    fade_capture();
    fade_prepare((uint16_t) time);
    fade_wait();
}

/* set_palette(time, r, g, b): fade every colour to one colour. */
void set_palette(int16_t time, int16_t red, int16_t green, int16_t blue)
{
    for (int i = 0; i < 0x100; i++) {
        FD(FD_TARGET)[i * 3] = (uint8_t) red;
        FD(FD_TARGET)[i * 3 + 1] = (uint8_t) green;
        FD(FD_TARGET)[i * 3 + 2] = (uint8_t) blue;
    }
    fade_capture();
    fade_prepare((uint16_t) time);
    fade_wait();
}

/* ------------------------------------------------------ colour cycling */

/* PALETTE.INC: eight cycles. Each holds its colours twice over, so a
 * rotation is one contiguous write to the DAC. */
static struct cycle {
    int16_t direction;          /* +3 or -3: bytes per step */
    int16_t position;
    int16_t bytes;              /* colours * 3 */
    uint8_t first;              /* first DAC index */
    uint8_t rate;               /* ticks per step */
    uint8_t countdown;          /* 0: stopped */
    uint8_t colours[93];
} cycles[8];

void set_cycle(int16_t n, int16_t rate, int16_t first, int16_t last)
{
    struct cycle *c;
    int16_t count;
    if ((uint16_t) (n - 1) >= 8)
        return;
    c = &cycles[n - 1];
    count = (int16_t) (last - first);
    if (count <= 0)
        return;
    count++;
    if ((uint16_t) count > 16)
        count = 16;
    c->bytes = (int16_t) (count * 3);
    c->first = (uint8_t) first;
    reciv_dac_read(c->first, count, c->colours);
    memmove(c->colours + c->bytes, c->colours, (size_t) (c->bytes - 3));
    c->rate = (uint8_t) rate;
    c->countdown = 0;
    c->position = 0;
}

void start_cycle(int16_t n)
{
    int16_t sign = n < 0 ? -1 : 0;
    uint16_t index = (uint16_t) ((n ^ sign) - sign - 1);
    struct cycle *c;
    if (index >= 8)
        return;
    c = &cycles[index];
    c->direction = (int16_t) ((3 ^ sign) - sign);
    c->countdown = c->rate;
}

void stop_cycle(int16_t n)
{
    if ((uint16_t) (n - 1) >= 8)
        return;
    cycles[n - 1].countdown = 0;
}

static void palette_tick(void)
{
    for (int i = 0; i < 8; i++) {
        struct cycle *c = &cycles[i];
        int16_t si;
        if (!c->countdown || --c->countdown)
            continue;
        c->countdown = c->rate;
        si = (int16_t) (c->position - c->direction);
        if (si < 0)
            si = (int16_t) (si + c->bytes);
        if (si >= c->bytes)
            si = 0;
        c->position = si;
        reciv_dac_write(c->first, c->bytes / 3, c->colours + si);
    }
}
