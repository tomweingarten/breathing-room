// Breathing Room for Nintendo DS: the breathing ring on the top screen,
// settings and Begin on the touchscreen. Same rhythms, lengths, tones and glow
// as the web version (../index.html).

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <nds.h>

#define W 256
#define H 192
#define CX 128
#define CY 96
#define FPS 59.8261f

// Ring geometry (the web version's 300-unit viewBox, scaled to fit 192 px).
#define R_OUTER 80
#define R_MIN 36    // breath at rest
#define R_MAX 76    // breath at the top of the in-breath
#define R_PROG 84   // session progress arc
#define R_RAYS 87

// Palette slots for the 8-bit top-screen bitmap. Colours are recomputed each
// frame so the glow tints the background and everything drawn on it.
enum { P_BG, P_LINE, P_WELL, P_SPOT, P_BRASS, P_INK, P_INKSOFT };

typedef struct { int r, g, b; } rgb8;   // 0-255
static const rgb8 BONE = {243, 236, 220}, WELL = {232, 222, 200}, INK = {31, 42, 68},
                 INKSOFT = {74, 84, 112}, SPOT = {95, 143, 122}, BRASS = {184, 145, 63},
                 GLOW = {127, 174, 149};
#define GLOW_MAX 0.42f

static u8 layer[W * H];   // rings and rays, drawn once
static u8 frame[W * H];   // composed each frame, then copied to VRAM
static u8 *vram;

// ---------------------------------------------------------------- settings

static const int RHYTHMS[4][2] = {{4, 6}, {5, 5}, {4, 8}, {5, 7}};
static const int LENGTHS[3] = {3, 10, 15};
static int rhythm = 0, length = 1, tones = 1, glow = 1, focus = 0;

static int running = 0, done = 0;
static u32 frames = 0;   // since Begin

// ---------------------------------------------------------------- colour

static u16 mix(rgb8 a, rgb8 b, float t) {
    return RGB15((int)(a.r + (b.r - a.r) * t) >> 3, (int)(a.g + (b.g - a.g) * t) >> 3,
                 (int)(a.b + (b.b - a.b) * t) >> 3);
}

static void set_palette(float g) {
    rgb8 bg = {BONE.r + (int)((GLOW.r - BONE.r) * g), BONE.g + (int)((GLOW.g - BONE.g) * g),
              BONE.b + (int)((GLOW.b - BONE.b) * g)};
    BG_PALETTE[P_BG] = mix(bg, bg, 0);
    BG_PALETTE[P_LINE] = mix(bg, INK, 0.18f);
    BG_PALETTE[P_WELL] = mix(WELL, WELL, 0);
    BG_PALETTE[P_SPOT] = mix(SPOT, SPOT, 0);
    BG_PALETTE[P_BRASS] = mix(BRASS, BRASS, 0);
    BG_PALETTE[P_INK] = mix(INK, INK, 0);
    BG_PALETTE[P_INKSOFT] = mix(INKSOFT, INKSOFT, 0);
}

// ---------------------------------------------------------------- drawing

static inline void plot(u8 *buf, int x, int y, u8 c) {
    if ((unsigned)x < W && (unsigned)y < H) buf[y * W + x] = c;
}

static int isqrt(int n) {
    int x = n, y = (x + 1) / 2;
    if (n < 2) return n;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}

static void disk(u8 *buf, int r, u8 c) {
    for (int dy = -r; dy <= r; dy++) {
        int y = CY + dy, dx = isqrt(r * r - dy * dy);
        if ((unsigned)y >= H) continue;
        int x0 = CX - dx < 0 ? 0 : CX - dx, x1 = CX + dx >= W ? W - 1 : CX + dx;
        memset(buf + y * W + x0, c, x1 - x0 + 1);
    }
}

static void circle(u8 *buf, int r, u8 c, int dash) {
    int steps = (int)(2 * M_PI * r * 2);
    for (int i = 0; i < steps; i++) {
        if (dash && (i / 3) % 3) continue;
        float a = 2 * M_PI * i / steps;
        plot(buf, CX + (int)lroundf(r * sinf(a)), CY - (int)lroundf(r * cosf(a)), c);
    }
}

static void draw_layer(void) {
    memset(layer, P_BG, sizeof layer);
    circle(layer, R_OUTER, P_LINE, 0);
    circle(layer, R_MIN, P_LINE, 1);
    for (int i = 0; i < 48; i++) {
        float a = 2 * M_PI * i / 48;
        int len = i % 4 == 0 ? 8 : 4;
        for (int r = R_RAYS; r <= R_RAYS + len; r++)
            plot(layer, CX + (int)lroundf(r * sinf(a)), CY - (int)lroundf(r * cosf(a)), P_LINE);
    }
}

// 5x7 glyphs for the readout, drawn scaled.
static const char *GLYPHS = "0123456789./ADEIMNORTUY";
static const u8 FONT[][7] = {
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},   {14, 17, 1, 2, 4, 8, 31},
    {31, 2, 4, 2, 1, 17, 14},     {2, 6, 10, 18, 31, 2, 2},  {31, 16, 30, 1, 1, 17, 14},
    {6, 8, 16, 30, 17, 17, 14},   {31, 1, 2, 4, 8, 8, 8},    {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 2, 12},   {0, 0, 0, 0, 0, 12, 12},   {1, 2, 2, 4, 8, 8, 16},
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31},
    {14, 4, 4, 4, 4, 4, 14},      {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
    {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 20, 18, 17}, {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14}, {17, 17, 10, 4, 4, 4, 4},
};

static void text(const char *s, int cy, int scale, u8 c) {
    int n = strlen(s), adv = 6 * scale, x = CX - (n * adv - scale) / 2, y = cy - 7 * scale / 2;
    for (; *s; s++, x += adv) {
        const char *g = strchr(GLYPHS, *s);
        if (!g || *s == ' ') continue;
        const u8 *rows = FONT[g - GLYPHS];
        for (int ry = 0; ry < 7; ry++)
            for (int rx = 0; rx < 5; rx++)
                if (rows[ry] & (16 >> rx))
                    for (int sy = 0; sy < scale; sy++)
                        memset(frame + (y + ry * scale + sy) * W + x + rx * scale, c, scale);
    }
}

static void progress(float f) {
    int steps = (int)(2 * M_PI * R_PROG * 2 * f);
    for (int i = 0; i < steps; i++) {
        float a = 2 * M_PI * i / (2 * M_PI * R_PROG * 2);
        for (int r = R_PROG - 1; r <= R_PROG; r++)
            plot(frame, CX + (int)lroundf(r * sinf(a)), CY - (int)lroundf(r * cosf(a)), P_BRASS);
    }
}

// ---------------------------------------------------------------- sound

static s8 wave[32] ALIGN(4);   // one cycle of a triangle wave, like the web version's oscillator
static int voice = -1, tone_frames = 0, tone_len = 0;

static void tone(int hz, float seconds) {
    if (!tones) return;
    if (voice >= 0) soundKill(voice);
    voice = soundPlaySample(wave, SoundFormat_8Bit, sizeof wave, hz * 32, 0, 64, true, 0);
    tone_frames = 0;
    tone_len = (int)(seconds * FPS);
}

static void tone_tick(void) {   // 80 ms attack, then exponential decay
    if (voice < 0) return;
    tone_frames++;
    int attack = 5;
    float v = tone_frames < attack ? 90.0f * tone_frames / attack
                                   : 90.0f * powf(0.001f, (float)(tone_frames - attack) / (tone_len - attack));
    if (tone_frames >= tone_len) { soundKill(voice); voice = -1; return; }
    soundSetVolume(voice, (u8)v);
}

static int finish_chime = 0;   // frames until the second closing note

// ---------------------------------------------------------------- touchscreen

#define ROW_RHYTHM 4
#define ROW_LENGTH 7
#define ROW_TONES 9
#define ROW_GLOW 10
#define ROW_GO 13
#define ROW_TIMER 16

static void fmt_time(char *out, int secs) { sprintf(out, "%d:%02d", secs / 60, secs % 60); }

static void draw_ui(void) {
    static const int label_rows[4] = {ROW_RHYTHM - 1, ROW_LENGTH - 1, ROW_TONES, ROW_GLOW};
    consoleClear();
    printf("\x1b[1;1HBREATHING ROOM");
    printf("\x1b[%d;1HRHYTHM", ROW_RHYTHM - 1);
    printf("\x1b[%d;1H", ROW_RHYTHM);
    for (int i = 0; i < 4; i++)
        printf(i == rhythm ? "[%d-%d] " : " %d-%d  ", RHYTHMS[i][0], RHYTHMS[i][1]);
    printf("\x1b[%d;1HLENGTH", ROW_LENGTH - 1);
    printf("\x1b[%d;1H", ROW_LENGTH);
    for (int i = 0; i < 3; i++) printf(i == length ? "[%2dm] " : " %2dm  ", LENGTHS[i]);
    printf("\x1b[%d;1HTONES  %s", ROW_TONES, tones ? "[on]  off" : " on  [off]");
    printf("\x1b[%d;1HGLOW   %s", ROW_GLOW, glow ? "[on]  off" : " on  [off]");
    printf("\x1b[%d;0H>", label_rows[focus]);
    printf("\x1b[%d;8H+--------------+", ROW_GO - 1);
    printf("\x1b[%d;8H|    %s    |", ROW_GO, running ? " STOP " : "BEGIN ");
    printf("\x1b[%d;8H+--------------+", ROW_GO + 1);
    printf("\x1b[19;1HNose in, slow out. Teeth apart,");
    printf("\x1b[20;1Hshoulders down. High tone: in.");
    printf("\x1b[21;1HLow tone: out.");
    printf("\x1b[23;1HA begin/stop  +pad settings");
}

static void draw_timer(void) {
    int total = LENGTHS[length] * 60, el = running ? (int)(frames / FPS) : 0;
    char a[8], b[8];
    fmt_time(a, el);
    fmt_time(b, total - el);
    printf("\x1b[%d;1H%-6s           %6s left", ROW_TIMER, a, b);
}

static void start(void) {
    running = 1; done = 0; frames = 0;
}

static void stop(int finished) {
    running = 0; done = finished;
    if (finished) { tone(659, 2.4f); finish_chime = 30; }
}

static void adjust(int row, int dir) {
    if (row == 0) rhythm = (rhythm + dir + 4) % 4;
    if (row == 1) length = (length + dir + 3) % 3;
    if (row == 2) tones = !tones;
    if (row == 3) glow = !glow;
}

static void handle_touch(touchPosition t) {
    int row = t.py / 8, col = t.px / 8;
    if (row == ROW_RHYTHM && col >= 1 && col < 25) { rhythm = (col - 1) / 6; focus = 0; }
    else if (row == ROW_LENGTH && col >= 1 && col < 19) { length = (col - 1) / 6; focus = 1; }
    else if (row == ROW_TONES) { tones = !tones; focus = 2; }
    else if (row == ROW_GLOW) { glow = !glow; focus = 3; }
    else if (row >= ROW_GO - 1 && row <= ROW_GO + 1) { running ? stop(0) : start(); }
    else return;
    draw_ui();
}

// ---------------------------------------------------------------- main

int main(void) {
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    int bg = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    vram = (u8 *)bgGetGfxPtr(bg);
    consoleDemoInit();   // touchscreen text UI

    soundEnable();
    for (int i = 0; i < 32; i++) wave[i] = (s8)(i < 16 ? -96 + i * 12 : 96 - (i - 16) * 12);
    DC_FlushRange(wave, sizeof wave);

    draw_layer();
    draw_ui();

    while (1) {
        scanKeys();
        int down = keysDown();
        if (down & (KEY_A | KEY_START)) { running ? stop(0) : start(); draw_ui(); }
        if (down & KEY_UP) { focus = (focus + 3) % 4; draw_ui(); }
        if (down & KEY_DOWN) { focus = (focus + 1) % 4; draw_ui(); }
        if (down & KEY_LEFT) { adjust(focus, -1); draw_ui(); }
        if (down & KEY_RIGHT) { adjust(focus, 1); draw_ui(); }
        if (down & KEY_TOUCH) { touchPosition t; touchRead(&t); handle_touch(t); }

        int tin = RHYTHMS[rhythm][0], tout = RHYTHMS[rhythm][1];
        float e = 0, f = 0;
        char big[8] = "READY", small[12];
        int tenths = (600 + (tin + tout) / 2) / (tin + tout);   // breaths per minute, one decimal
        sprintf(small, "%d.%d/MIN", tenths / 10, tenths % 10);

        if (running) {
            float el = frames / FPS, total = LENGTHS[length] * 60.0f;
            if (el >= total) {
                stop(1);
                draw_ui();
            } else {
                float cyc = tin + tout, p = fmodf(el, cyc);
                int inhale = p < tin;
                float k = inhale ? p / tin : 1 - (p - tin) / tout;
                e = 0.5f - 0.5f * cosf(M_PI * k);
                f = el / total;
                static int last_phase = -1;
                if (frames == 0) last_phase = -1;
                if (inhale != last_phase) { last_phase = inhale; tone(inhale ? 523 : 392, inhale ? 1.2f : 1.6f); }
                strcpy(big, inhale ? "IN" : "OUT");
                sprintf(small, "%d", (int)ceilf(inhale ? tin - p : cyc - p));
                frames++;
            }
        }
        if (!running && done) strcpy(big, "DONE");
        if (finish_chime && --finish_chime == 0) tone(523, 2.8f);
        tone_tick();

        set_palette(running && glow ? GLOW_MAX * e : 0);
        memcpy(frame, layer, sizeof frame);
        int r = R_MIN + (int)((R_MAX - R_MIN) * e);
        disk(frame, r, P_SPOT);
        disk(frame, r - 2, P_WELL);
        if (running) progress(f);
        text(big, CY - 6, 4, P_INK);
        text(small, CY + 22, 2, P_INKSOFT);

        DC_FlushRange(frame, sizeof frame);
        swiWaitForVBlank();   // copy during vblank so the ring never tears
        dmaCopy(frame, vram, sizeof frame);
        static int last_second = -1;
        int second = running ? (int)(frames / FPS) : -2 - length;
        if (second != last_second) { last_second = second; draw_timer(); }
    }
    return 0;
}
