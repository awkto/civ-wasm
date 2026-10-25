/* video.c: the VGA as the player sees it, in an SDL window.
 *
 * The game and the graphics driver draw into the VGA window at A000:0000
 * (mode 13h: 320x200, one byte per pixel) and load colours into the DAC
 * (256 entries of 6-bit RGB). This file shows that memory through the DAC
 * in a window, at the 4:3 shape of the monitor it was drawn for, and turns
 * window events into keyboard and mouse input. */
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "reciv.h"
#include "filters.h"

uint8_t reciv_dac[256][3];

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_Texture *scaled_texture;
static int scaled_factor;
static enum reciv_filter filter = RECIV_FILTER_HQ4X;
enum reciv_text_filter {
    RECIV_TEXT_NEAREST, RECIV_TEXT_SHARP, RECIV_TEXT_BILINEAR
};
static enum reciv_text_filter text_filter = RECIV_TEXT_SHARP;
static int split_text = 1;
static SDL_Texture *filtered_texture;
static uint32_t *filtered_pixels;
static uint32_t *frame_pixels;
static SDL_Texture *text_texture;
static SDL_Texture *text_scaled_texture;
static int text_scaled_factor, text_filter_failed;
static uint32_t *text_pixels;
static SDL_BlendMode text_blend_mode;
static int filter_failed;
/* civ-wasm: the WinCiv skin, composited at twice the screen's size. */
static SDL_Texture *skin_texture, *skin_scaled_texture;
static uint32_t *skin_pixels;
static int skin_scaled_factor;
static uint16_t start_address;          /* CRTC start address, in 4-pixel units */
static void text_clear(void);
static int video_mode = 3;
static int dirty = 1;
static uint64_t last_present;
static SDL_FRect view;                   /* where the picture is drawn */
/* civ-wasm: the screen is reciv_screen_w x reciv_screen_h; the part shown
 * is all of it in wide view (the map), else the classic 320x200 corner.
 * Either keeps the VGA's pixel shape: 320x200 filled a 4:3 monitor. */
int reciv_wide_view;
#define SW reciv_screen_w
#define SH reciv_screen_h
static int shown_w(void) { return reciv_wide_view ? SW : 320; }
static int shown_h(void) { return reciv_wide_view ? SH : 200; }

/* civ-wasm: the page's settings while playing. A new filter takes effect
 * at the next present, which rebuilds the textures for it; the skin, when
 * loaded, can be hidden (the DOS pixels are always kept under it). */
static int pending_filter = -1;
int reciv_skin_hidden;

void reciv_video_set_filter(int index)
{
    if (index >= RECIV_FILTER_NEAREST && index <= RECIV_FILTER_HQ4X)
        pending_filter = index;
}

void reciv_video_show_skin(int shown)
{
    reciv_skin_hidden = !shown;
    dirty = 1;
}

/* Presentation only: the VGA pixels stay original. */
int reciv_video_filter(const char *name)
{
    static const char *names[] = {"nearest", "sharp", "bilinear", "hq4x"};
    for (int i = 0; i < (int) (sizeof names / sizeof *names); ++i)
        if (!strcmp(name, names[i])) {
            filter = (enum reciv_filter) i;
            dirty = 1;
            return 1;
        }
    return 0;
}

int reciv_video_text_filter(const char *name)
{
    static const char *names[] = {"nearest", "sharp", "bilinear"};
    if (!strcmp(name, "same")) {
        split_text = 0;
        dirty = 1;
        return 1;
    }
    for (int i = 0; i < (int) (sizeof names / sizeof *names); ++i)
        if (!strcmp(name, names[i])) {
            text_filter = (enum reciv_text_filter) i;
            split_text = 1;
            dirty = 1;
            return 1;
        }
    return 0;
}

/* The screen's pixel buffers and textures, at reciv_screen_w x
 * reciv_screen_h (civ-wasm: rebuilt when the screen is resized). */
static void screen_textures(void)
{
    frame_pixels = SDL_malloc((size_t) SW * SH * sizeof(uint32_t));
    text_pixels = SDL_malloc((size_t) SW * SH * sizeof(uint32_t));
    if (!frame_pixels || !text_pixels)
        reciv_fatal("no memory for a %dx%d screen", SW, SH);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, SW, SH);
    if (!texture)
        reciv_fatal("SDL_CreateTexture: %s", SDL_GetError());
    SDL_SetTextureScaleMode(texture, filter == RECIV_FILTER_BILINEAR ?
                            SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    if (split_text) {
        text_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, SW, SH);
        text_blend_mode = SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
            SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
        if (!text_texture || !SDL_SetTextureBlendMode(text_texture, text_blend_mode)) {
            reciv_log("text layer unavailable; filtering the complete image: %s", SDL_GetError());
            SDL_DestroyTexture(text_texture);
            text_texture = NULL;
            split_text = 0;
        } else {
            SDL_SetTextureScaleMode(text_texture, text_filter == RECIV_TEXT_BILINEAR ?
                                                  SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        }
    }
    int factor = reciv_filter_factor(filter);
    if (factor) {
        filtered_pixels = SDL_malloc((size_t) SW * SH * factor * factor * sizeof(uint32_t));
        filtered_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
            SDL_TEXTUREACCESS_STREAMING, SW * factor, SH * factor);
        if (!filtered_pixels || !filtered_texture) {
            reciv_log("graphics filter allocation failed; using nearest pixels");
            filter_failed = 1;
        } else {
            SDL_SetTextureScaleMode(filtered_texture, SDL_SCALEMODE_LINEAR);
        }
    }
}

/* civwasm.c's resize: drop the old size's buffers and textures. */
void reciv_video_resize(void)
{
    SDL_free(frame_pixels);
    SDL_free(text_pixels);
    SDL_free(filtered_pixels);
    frame_pixels = text_pixels = filtered_pixels = NULL;
    SDL_DestroyTexture(texture);
    SDL_DestroyTexture(text_texture);
    SDL_DestroyTexture(filtered_texture);
    SDL_DestroyTexture(scaled_texture);
    SDL_DestroyTexture(text_scaled_texture);
    SDL_DestroyTexture(skin_texture);
    SDL_DestroyTexture(skin_scaled_texture);
    SDL_free(skin_pixels);
    skin_pixels = NULL;
    texture = text_texture = filtered_texture = scaled_texture = text_scaled_texture = NULL;
    skin_texture = skin_scaled_texture = NULL;
    scaled_factor = text_scaled_factor = skin_scaled_factor = 0;
    screen_textures();
    dirty = 1;
}

void reciv_video_open(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        reciv_fatal("SDL_Init: %s", SDL_GetError());
#ifdef __EMSCRIPTEN__
    /* civ-wasm: draw at the display's pixel density, so the browser does not
     * smooth the canvas again on a high-DPI screen. OPENGL up front: otherwise
     * the WebGL renderer recreates the window, and the recreated one no longer
     * follows the canvas's CSS size. */
    window = SDL_CreateWindow("Civilization", 960, 720,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_OPENGL);
#else
    window = SDL_CreateWindow("Civilization", 960, 720, SDL_WINDOW_RESIZABLE);
#endif
    if (!window)
        reciv_fatal("SDL_CreateWindow: %s", SDL_GetError());
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer)
        reciv_fatal("SDL_CreateRenderer: %s", SDL_GetError());
    screen_textures();
    SDL_HideCursor();
}

void reciv_video_dirty(void)
{
    dirty = 1;
}

void reciv_set_start_address(uint16_t start)
{
    if (start != start_address) {
        start_address = start;
        dirty = 1;
    }
}

/* BIOS INT 10h services (dos.c). */
void reciv_video_set_mode(int mode)
{
    video_mode = mode & 0x7F;
    if (video_mode == 0x13 && !(mode & 0x80)) {
        memset(reciv_screen_pixels(), 0, (size_t) SW * SH);
        reciv_page_layers_reset(0);
    }
    if (video_mode == 3)
        text_clear();
    start_address = 0;
    dirty = 1;
}

int reciv_video_mode(void)
{
    return video_mode;
}

/* ---------------------------------------------------------- text mode */

/* Mode 3: 80x25 characters and attributes at B800:0000. The game uses it
 * before the graphics start: the setup questions and the disk prompts. */
#define TEXT (reciv_mem + 0xB8000)
static int text_row, text_col;

static void text_clear(void)
{
    for (int i = 0; i < 80 * 25; i++) {
        TEXT[i * 2] = ' ';
        TEXT[i * 2 + 1] = 0x07;
    }
    text_row = text_col = 0;
}

void reciv_text_cursor(int row, int column)
{
    text_row = row;
    text_col = column;
}

/* INT 10h function 09h: character and attribute, `count` times, at the
 * cursor, which does not move. */
void reciv_text_write(uint8_t c, uint8_t attribute, int count)
{
    int at = text_row * 80 + text_col;
    for (int i = 0; i < count && at + i < 80 * 25; i++) {
        TEXT[(at + i) * 2] = c;
        TEXT[(at + i) * 2 + 1] = attribute;
    }
    dirty = 1;
}

/* INT 10h function 0Eh: teletype output (also DOS console output). */
void reciv_text_teletype(uint8_t c)
{
    switch (c) {
    case '\r':
        text_col = 0;
        break;
    case '\n':
        text_row++;
        break;
    case 8:
        if (text_col)
            text_col--;
        break;
    case 7:
        break;
    default:
        if (text_row < 25 && text_col < 80)
            TEXT[(text_row * 80 + text_col) * 2] = c;
        if (++text_col >= 80) {
            text_col = 0;
            text_row++;
        }
    }
    if (text_row >= 25) {
        memmove(TEXT, TEXT + 160, 160 * 24);
        for (int i = 0; i < 80; i++) {
            TEXT[(24 * 80 + i) * 2] = ' ';
            TEXT[(24 * 80 + i) * 2 + 1] = 0x07;
        }
        text_row = 24;
    }
    dirty = 1;
}

static const uint8_t text_colours[16][3] = {
    { 0, 0, 0 }, { 0, 0, 170 }, { 0, 170, 0 }, { 0, 170, 170 }, { 170, 0, 0 }, { 170, 0, 170 },
    { 170, 85, 0 }, { 170, 170, 170 }, { 85, 85, 85 }, { 85, 85, 255 }, { 85, 255, 85 },
    { 85, 255, 255 }, { 255, 85, 85 }, { 255, 85, 255 }, { 255, 255, 85 }, { 255, 255, 255 },
};

static void present_text(void)
{
    float cw = view.w / 80, ch = view.h / 25;
    char glyph[2] = { 0, 0 };
    for (int row = 0; row < 25; row++)
        for (int col = 0; col < 80; col++) {
            uint8_t c = TEXT[(row * 80 + col) * 2], a = TEXT[(row * 80 + col) * 2 + 1];
            const uint8_t *bg = text_colours[(a >> 4) & 7], *fg = text_colours[a & 15];
            SDL_FRect cell = { view.x + col * cw, view.y + row * ch, cw, ch };
            SDL_SetRenderDrawColor(renderer, bg[0], bg[1], bg[2], 255);
            SDL_RenderFillRect(renderer, &cell);
            if (c > ' ' && c < 127) {
                glyph[0] = (char) c;
                SDL_SetRenderScale(renderer, cw / 8, ch / 8);
                SDL_SetRenderDrawColor(renderer, fg[0], fg[1], fg[2], 255);
                SDL_RenderDebugText(renderer, cell.x / (cw / 8), cell.y / (ch / 8), glyph);
                SDL_SetRenderScale(renderer, 1, 1);
            }
        }
    /* The cursor: an underline. */
    if (text_row < 25 && text_col < 80) {
        SDL_FRect bar = { view.x + text_col * cw, view.y + (text_row + 0.8f) * ch, cw, ch / 8 };
        SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);
        SDL_RenderFillRect(renderer, &bar);
    }
}

void reciv_dac_set(int index, int red, int green, int blue)
{
    reciv_dac[index & 0xFF][0] = (uint8_t) (red & 63);
    reciv_dac[index & 0xFF][1] = (uint8_t) (green & 63);
    reciv_dac[index & 0xFF][2] = (uint8_t) (blue & 63);
    dirty = 1;
}

void reciv_dac_write(int index, int count, const uint8_t *rgb)
{
    for (int i = 0; i < count * 3; i++)
        reciv_dac[(index + i / 3) & 0xFF][i % 3] = rgb[i] & 63;
    dirty = 1;
}

void reciv_dac_read(int index, int count, uint8_t *rgb)
{
    for (int i = 0; i < count * 3; i++)
        rgb[i] = reciv_dac[(index + i / 3) & 0xFF][i % 3];
}

static void layout(void)
{
    int w, h;
    float scale;
    float aw = (float) shown_w(), ah = shown_h() * 1.2f;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    scale = (float) w / aw < (float) h / ah ? (float) w / aw : (float) h / ah;
    view.w = aw * scale;
    view.h = ah * scale;
    view.x = (w - view.w) / 2;
    view.y = (h - view.h) / 2;
}

/* Sharp bilinear: nearest-neighbour integer enlargement first, then linear
 * sampling for the remaining stretch (including the VGA's 4:3 correction).
 * This limits blending to pixel boundaries instead of blurring whole pixels.
 * Keep the intermediate bounded at 1920x1200 even on very large displays. */
static void present_graphics(void)
{
    SDL_Texture *display = texture;
    int display_factor = 1;
    if (reciv_filter_factor(filter) && !filter_failed) {
        int factor = reciv_filter_factor(filter);
        if (reciv_filter_scale(filter, frame_pixels, filtered_pixels, SW, SH) &&
            SDL_UpdateTexture(filtered_texture, NULL, filtered_pixels, SW * factor * (int) sizeof(uint32_t))) {
            display = filtered_texture;
            display_factor = factor;
        } else {
            reciv_log("graphics filter failed; using nearest pixels");
            filter_failed = 1;
        }
    }
    if (filter == RECIV_FILTER_SHARP && !filter_failed) {
        float sx = view.w / shown_w(), sy = view.h / shown_h();
        int factor = (int) (sx < sy ? sx : sy);
        factor = factor < 1 ? 1 : factor > 6 ? 6 : factor;
        while (factor > 1 && (SW * factor > 4096 || SH * factor > 4096))
            factor--;
        if (!scaled_texture || factor != scaled_factor) {
            SDL_DestroyTexture(scaled_texture);
            scaled_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
                                               SDL_TEXTUREACCESS_TARGET, SW * factor, SH * factor);
            scaled_factor = factor;
            if (!scaled_texture || !SDL_SetTextureScaleMode(scaled_texture, SDL_SCALEMODE_LINEAR))
                filter_failed = 1;
        }
        if (!filter_failed) {
            if (SDL_SetRenderTarget(renderer, scaled_texture)) {
                /* The opaque source covers the entire target, so no clear is needed. */
                bool copied = SDL_RenderTexture(renderer, texture, NULL, NULL);
                if (!SDL_SetRenderTarget(renderer, NULL))
                    reciv_fatal("restore window render target: %s", SDL_GetError());
                if (copied) {
                    display = scaled_texture;
                    display_factor = factor;
                } else
                    filter_failed = 1;
            } else {
                filter_failed = 1;
            }
        }
        if (filter_failed) {
            reciv_log("graphics filter unavailable; using nearest pixels: %s", SDL_GetError());
            SDL_DestroyTexture(scaled_texture);
            scaled_texture = NULL;
        }
    }
    SDL_FRect part = { 0, 0, (float) shown_w() * display_factor, (float) shown_h() * display_factor };
    SDL_RenderTexture(renderer, display, &part, &view);
}

/* The skin's picture: each screen pixel as 2 x 2, a skin colour where the
 * skin layer has one, else the pixel. Text drawn over the skin shows (in
 * the frame itself unless the text layer is separate). */
static int skin_frame(const uint32_t *palette, uint32_t offset)
{
    const uint8_t *skin = reciv_page_skin(0);
    const uint8_t *src = reciv_screen_pixels(), *text_mask = reciv_page_text_mask(0);
    const uint8_t *background = split_text ? reciv_page_background(0) : src;
    uint32_t colours[256];
    int w2 = 2 * SW;
    if (!skin || !reciv_skin_colours || reciv_skin_hidden)
        return 0;
    if (!skin_pixels) {
        skin_pixels = SDL_malloc((size_t) w2 * 2 * SH * sizeof(uint32_t));
        skin_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, w2, 2 * SH);
        if (!skin_pixels || !skin_texture) {
            reciv_log("no memory for the skin's picture");
            reciv_skin_colours = 0;
            return 0;
        }
        SDL_SetTextureScaleMode(skin_texture, filter == RECIV_FILTER_BILINEAR ?
                                SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    }
    colours[0] = 0;
    {
        /* WinCiv's water, colours 64 to 71, turns with the map's first
         * colour cycle (CORE.C setup_cycles: eight colours from 60h); a
         * fade takes the skin's colours along with the DAC's. */
        int step = reciv_cycle_step(1);
        uint8_t to[3];
        uint32_t a = reciv_fade(to), keep = 0xFFFF - a;
        for (int i = 0; i < 127; i++) {
            uint32_t c = reciv_skin_palette[i >= 64 && i < 72 ? 64 + (i - 64 + step) % 8 : i];
            if (a) {
                uint32_t r = ((c >> 16 & 255) * keep + (to[0] << 2 | to[0] >> 4) * a) / 0xFFFF;
                uint32_t g = ((c >> 8 & 255) * keep + (to[1] << 2 | to[1] >> 4) * a) / 0xFFFF;
                uint32_t b = ((c & 255) * keep + (to[2] << 2 | to[2] >> 4) * a) / 0xFFFF;
                c = r << 16 | g << 8 | b;
            }
            colours[i + 1] = c;
        }
    }
    for (int i = 0; i < 128; i++)
        colours[128 + i] = palette[i];
    for (int y = 0; y < SH; y++) {
        uint32_t *out0 = skin_pixels + (size_t) (2 * y) * w2, *out1 = out0 + w2;
        for (int x = 0; x < SW; x++) {
            uint32_t at = (uint32_t) y * SW + x + offset;
            uint32_t base;
            if (at >= (uint32_t) (SW * SH)) {
                out0[2 * x] = out0[2 * x + 1] = out1[2 * x] = out1[2 * x + 1] = 0;
                continue;
            }
            base = palette[background[at]];
            if (text_mask[at] && !split_text) {
                out0[2 * x] = out0[2 * x + 1] = out1[2 * x] = out1[2 * x + 1] = base;
                continue;
            }
            {
                uint32_t sa = at + (uint32_t) (at / SW) * SW;   /* (x, y) -> (2x, 2y) */
                const uint8_t *k0 = skin + 2 * (size_t) sa, *k1 = k0 + w2;
                out0[2 * x] = k0[0] ? colours[k0[0]] : base;
                out0[2 * x + 1] = k0[1] ? colours[k0[1]] : base;
                out1[2 * x] = k1[0] ? colours[k1[0]] : base;
                out1[2 * x + 1] = k1[1] ? colours[k1[1]] : base;
            }
        }
    }
    SDL_UpdateTexture(skin_texture, NULL, skin_pixels, w2 * (int) sizeof(uint32_t));
    return 1;
}

/* present_graphics for the skin: the 2x picture, enlarged sharply by an
 * even factor, then stretched to the view. */
static void present_skin(void)
{
    SDL_Texture *display = skin_texture;
    int display_factor = 2;
    if (filter != RECIV_FILTER_NEAREST && filter != RECIV_FILTER_BILINEAR) {
        float sx = view.w / shown_w(), sy = view.h / shown_h();
        int factor = ((int) (sx < sy ? sx : sy)) & ~1;
        while (factor > 2 && (SW * factor > 4096 || SH * factor > 4096))
            factor -= 2;
        if (factor > 2) {
            if (!skin_scaled_texture || factor != skin_scaled_factor) {
                SDL_DestroyTexture(skin_scaled_texture);
                skin_scaled_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
                                                        SDL_TEXTUREACCESS_TARGET, SW * factor, SH * factor);
                skin_scaled_factor = factor;
                if (skin_scaled_texture)
                    SDL_SetTextureScaleMode(skin_scaled_texture, SDL_SCALEMODE_LINEAR);
            }
            if (skin_scaled_texture && SDL_SetRenderTarget(renderer, skin_scaled_texture)) {
                bool copied = SDL_RenderTexture(renderer, skin_texture, NULL, NULL);
                if (!SDL_SetRenderTarget(renderer, NULL))
                    reciv_fatal("restore window render target: %s", SDL_GetError());
                if (copied) {
                    display = skin_scaled_texture;
                    display_factor = factor;
                }
            }
        } else
            SDL_SetTextureScaleMode(skin_texture, SDL_SCALEMODE_LINEAR);
    }
    SDL_FRect part = { 0, 0, (float) shown_w() * display_factor, (float) shown_h() * display_factor };
    SDL_RenderTexture(renderer, display, &part, &view);
}

static void present_graphics_text(void)
{
    SDL_Texture *display = text_texture;
    int display_factor = 1;
    if (text_filter == RECIV_TEXT_SHARP && !text_filter_failed) {
        float sx = view.w / shown_w(), sy = view.h / shown_h();
        int factor = (int) (sx < sy ? sx : sy);
        factor = factor < 1 ? 1 : factor > 6 ? 6 : factor;
        while (factor > 1 && (SW * factor > 4096 || SH * factor > 4096))
            factor--;
        if (!text_scaled_texture || factor != text_scaled_factor) {
            SDL_DestroyTexture(text_scaled_texture);
            text_scaled_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_TARGET, SW * factor, SH * factor);
            text_scaled_factor = factor;
            if (!text_scaled_texture ||
                !SDL_SetTextureScaleMode(text_scaled_texture, SDL_SCALEMODE_LINEAR) ||
                !SDL_SetTextureBlendMode(text_scaled_texture, text_blend_mode))
                text_filter_failed = 1;
        }
        if (!text_filter_failed) {
            if (SDL_SetRenderTarget(renderer, text_scaled_texture)) {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
                SDL_RenderClear(renderer);
                SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_NONE);
                bool copied = SDL_RenderTexture(renderer, text_texture, NULL, NULL);
                SDL_SetTextureBlendMode(text_texture, text_blend_mode);
                if (!SDL_SetRenderTarget(renderer, NULL))
                    reciv_fatal("restore window render target: %s", SDL_GetError());
                if (copied) {
                    display = text_scaled_texture;
                    display_factor = factor;
                } else
                    text_filter_failed = 1;
            } else {
                text_filter_failed = 1;
            }
        }
        if (text_filter_failed) {
            reciv_log("sharp text filter unavailable; using nearest pixels: %s", SDL_GetError());
            SDL_DestroyTexture(text_scaled_texture);
            text_scaled_texture = NULL;
        }
    }
    SDL_FRect part = { 0, 0, (float) shown_w() * display_factor, (float) shown_h() * display_factor };
    SDL_RenderTexture(renderer, display, &part, &view);
}

/* Show the screen; at most once per video frame unless forced. */
void reciv_video_present(int force)
{
    uint64_t now = SDL_GetTicksNS();
    uint32_t palette[256];

    if (pending_filter >= 0) {
        filter = (enum reciv_filter) pending_filter;
        pending_filter = -1;
        filter_failed = 0;
        reciv_video_resize();
        dirty = 1;
    }
    if (!force && (!dirty || now - last_present < 14000000))
        return;
    last_present = now;
    dirty = 0;
    for (int i = 0; i < 256; i++) {
        const uint8_t *c = reciv_dac[i];
        palette[i] = (uint32_t) ((c[0] << 2 | c[0] >> 4) << 16 | (c[1] << 2 | c[1] >> 4) << 8 | (c[2] << 2 | c[2] >> 4));
    }
    layout();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (video_mode != 0x13) {
        present_text();
        SDL_RenderPresent(renderer);
        return;
    }
    uint32_t offset = (uint32_t) start_address * 4;
    const uint8_t *src = reciv_screen_pixels() + offset;
    const int pixels = SW * SH - (int) offset;
    const uint8_t *text_mask = NULL;
    if (split_text) {
        const uint8_t *background = reciv_page_background(0) + offset;
        text_mask = reciv_page_text_mask(0) + offset;
        for (int i = 0; i < pixels; ++i) {
            frame_pixels[i] = palette[background[i]];
            text_pixels[i] = text_mask[i] ? 0xFF000000u | palette[src[i]] : 0;
        }
        SDL_UpdateTexture(text_texture, NULL, text_pixels, SW * (int) sizeof(uint32_t));
    } else {
        for (int i = 0; i < pixels; ++i)
            frame_pixels[i] = palette[src[i]];
    }
    if (skin_frame(palette, offset))
        present_skin();
    else {
        SDL_UpdateTexture(texture, NULL, frame_pixels, SW * (int) sizeof(uint32_t));
        present_graphics();
    }
    if (split_text)
        present_graphics_text();
    SDL_RenderPresent(renderer);
}

/* ------------------------------------------------------------- events */

extern uint16_t reciv_key_code(SDL_Scancode scancode, SDL_Keymod mod);

static int mouse_buttons;

static void mouse_moved(float wx, float wy)
{
    float px, py;
    int x, y;
    SDL_RenderCoordinatesFromWindow(renderer, wx, wy, &px, &py);
    layout();
    x = (int) ((px - view.x) * shown_w() / view.w);
    y = (int) ((py - view.y) * shown_h() / view.h);
    x = x < 0 ? 0 : x > shown_w() - 1 ? shown_w() - 1 : x;
    y = y < 0 ? 0 : y > shown_h() - 1 ? shown_h() - 1 : y;
    reciv_mouse_event(x, y, mouse_buttons);
}

/* Move the host pointer to a game position (INT 33h function 4). */
void reciv_mouse_warp(int x, int y)
{
    float wx, wy;
    layout();
    SDL_RenderCoordinatesToWindow(renderer, view.x + (x + 0.5f) * view.w / shown_w(),
                                  view.y + (y + 0.5f) * view.h / shown_h(), &wx, &wy);
    if (SDL_GetMouseFocus() == window)
        SDL_WarpMouseInWindow(window, wx, wy);
}

void reciv_pump_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
            reciv_exit(0);
        case SDL_EVENT_KEY_DOWN: {
            uint16_t code = reciv_key_code(e.key.scancode, e.key.mod);
            if (code)
                reciv_key_event(code);
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            mouse_moved(e.motion.x, e.motion.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int bit = e.button.button == SDL_BUTTON_LEFT ? 1 : e.button.button == SDL_BUTTON_RIGHT ? 2
                : e.button.button == SDL_BUTTON_MIDDLE ? 4 : 0;
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                mouse_buttons |= bit;
            else
                mouse_buttons &= ~bit;
            mouse_moved(e.button.x, e.button.y);
            break;
        }
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            dirty = 1;
            break;
        default:
            break;
        }
    }
}

void reciv_video_close(void)
{
    SDL_DestroyTexture(filtered_texture);
    filtered_texture = NULL;
    SDL_free(filtered_pixels);
    filtered_pixels = NULL;
    SDL_DestroyTexture(scaled_texture);
    scaled_texture = NULL;
    SDL_DestroyTexture(text_scaled_texture);
    text_scaled_texture = NULL;
    SDL_DestroyTexture(text_texture);
    text_texture = NULL;
    SDL_DestroyTexture(texture);
    texture = NULL;
    SDL_DestroyRenderer(renderer);
    renderer = NULL;
    SDL_DestroyWindow(window);
    window = NULL;
    SDL_Quit();
}
