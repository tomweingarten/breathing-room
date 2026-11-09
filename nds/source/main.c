// Breathing Room for Nintendo DS: the breathing ring on the top screen,
// settings and Begin on the touchscreen. Same rhythms, lengths, tones and glow
// as the web version (../index.html).

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <fat.h>
#include <nds.h>

#define W 256
#define H 192
#define CX 128
#define CY 96
#define FPS 59.8261f

// Ring geometry (the web version's 300-unit viewBox, scaled to fit 192 px).
#define R_OUTER 80
#define R_MIN 36    // breath at rest
#define R_IDLE 52   // before Begin and after Done, so the label fits inside
#define R_MAX 76    // breath at the top of the in-breath
#define R_PROG 84   // session progress arc
#define R_RAYS 87

// Palette slots for the 8-bit top-screen bitmap. Colours are recomputed each
// frame so the glow tints the background and everything drawn on it.
enum { P_BG, P_LINE, P_WELL, P_SPOT, P_BRASS, P_INK, P_INKSOFT };

typedef struct { int r, g, b; } rgb8;   // 0-255
typedef struct { rgb8 bg, well, ink, inksoft, spot, brass, glow; float glow_max, line; } theme;
// The web version's light and dark palettes.
static const theme LIGHT = {{243, 236, 220}, {232, 222, 200}, {31, 42, 68}, {74, 84, 112},
                            {95, 143, 122}, {184, 145, 63}, {127, 174, 149}, 0.42f, 0.18f};
static const theme DARK = {{20, 27, 45}, {28, 37, 64}, {236, 228, 210}, {169, 176, 196},
                           {127, 179, 155}, {207, 169, 85}, {93, 143, 120}, 0.50f, 0.15f};

static u8 layer[W * H];   // rings and rays, drawn once
static u8 frame[W * H];   // composed each frame, then copied to VRAM
static u8 *vram;

// ---------------------------------------------------------------- settings

static const int RHYTHMS[4][2] = {{4, 6}, {5, 5}, {4, 8}, {5, 7}};
#define N_LENGTHS 5
static const int LENGTHS[N_LENGTHS] = {3, 10, 15, 20, 30};
// BUZZ: off, a haptic actuator on the headphone jack (driven by a hum), or a Rumble Pak.
enum { BUZZ_OFF, BUZZ_JACK, BUZZ_PAK };
static int rhythm = 0, length = 1, tones = 1, buzz_mode = BUZZ_OFF, hum_hz = 235, strength = 2;
static int glow = 1, dark = 1, lights_out = 0, focus = 0;
static int has_rumble = 0;   // DS Rumble Pak in the GBA slot
static int has_fat = 0;      // settings can be saved to the SD card

static int running = 0, done = 0;
// Time comes from the hardware vblank interrupt, not from loop passes, so a slow
// frame can never stretch the breathing rhythm, the timer or the tones.
static volatile u32 vblanks = 0;
static void on_vblank(void) { vblanks++; }
static u32 started_at = 0;   // vblanks at Begin
static u32 frames = 0;       // vblanks since Begin
static int last_phase = -1;  // so the first phase of a session always cues

// ---------------------------------------------------------------- colour

static u16 mix(rgb8 a, rgb8 b, float t) {
    return RGB15((int)(a.r + (b.r - a.r) * t) >> 3, (int)(a.g + (b.g - a.g) * t) >> 3,
                 (int)(a.b + (b.b - a.b) * t) >> 3);
}

static const theme *T(void) { return dark ? &DARK : &LIGHT; }

// e: 0-1 breath fullness; the glow tints the background toward the theme's glow colour.
static void set_palette(float e) {
    const theme *t = T();
    float g = t->glow_max * e;
    rgb8 bg = {t->bg.r + (int)((t->glow.r - t->bg.r) * g), t->bg.g + (int)((t->glow.g - t->bg.g) * g),
               t->bg.b + (int)((t->glow.b - t->bg.b) * g)};
    BG_PALETTE[P_BG] = mix(bg, bg, 0);
    BG_PALETTE[P_LINE] = mix(bg, t->ink, t->line);
    BG_PALETTE[P_WELL] = mix(t->well, t->well, 0);
    BG_PALETTE[P_SPOT] = mix(t->spot, t->spot, 0);
    BG_PALETTE[P_BRASS] = mix(t->brass, t->brass, 0);
    BG_PALETTE[P_INK] = mix(t->ink, t->ink, 0);
    BG_PALETTE[P_INKSOFT] = mix(t->inksoft, t->inksoft, 0);
}

static void set_sub_palette(void) {   // touchscreen text in the same theme
    BG_PALETTE_SUB[0] = mix(T()->bg, T()->bg, 0);
    for (int i = 1; i < 256; i++) BG_PALETTE_SUB[i] = mix(T()->ink, T()->ink, 0);
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

// The progress arc's pixels, clockwise from 12 o'clock, computed once at startup:
// drawing them with trig every frame got slower as the arc grew.
#define PROG_POINTS 1056   // ~2 per pixel of circumference
static u16 prog_xy[PROG_POINTS][2][2];

static void init_progress(void) {
    for (int i = 0; i < PROG_POINTS; i++) {
        float a = 2 * M_PI * i / PROG_POINTS;
        for (int k = 0; k < 2; k++) {
            int r = R_PROG - 1 + k;
            prog_xy[i][k][0] = CX + (int)lroundf(r * sinf(a));
            prog_xy[i][k][1] = CY - (int)lroundf(r * cosf(a));
        }
    }
}

static void progress(float f) {
    int n = (int)(PROG_POINTS * f);
    for (int i = 0; i < n; i++)
        for (int k = 0; k < 2; k++) plot(frame, prog_xy[i][k][0], prog_xy[i][k][1], P_BRASS);
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

// ---------------------------------------------------------------- vibration

// Patterns are on/off frame counts ending in 0. In: two short buzzes. Out: one long one.
static const int BUZZ_IN[] = {7, 7, 7, 0};
static const int BUZZ_OUT[] = {30, 0};
static const int BUZZ_DONE[] = {7, 7, 7, 7, 7, 0};
static const int *buzz = NULL;
static int buzz_step = 0, buzz_left = 0, motor = 0;

static void vibrate_pattern(const int *pattern) {
    if (buzz_mode != BUZZ_PAK || !has_rumble) return;
    buzz = pattern; buzz_step = 0; buzz_left = pattern[0];
}

static void vibrate_tick(void) {
    if (!buzz) return;
    if (buzz_step % 2 == 0) { motor = !motor; setRumble(motor); }   // the motor runs while the line toggles
    if (--buzz_left > 0) return;
    buzz_step++;
    if (buzz[buzz_step] == 0) { buzz = NULL; setRumble(false); return; }
    buzz_left = buzz[buzz_step];
}

// Headphone-jack haptics: a sine hum whose volume follows the breath, so an
// actuator (via a small amp) swells on the in-breath and fades on the out-breath.
// Plugging into the jack mutes the speakers. STRENGTH caps the volume in software.
static s8 sine[32] ALIGN(4);
static int hum_voice = -1, preview_left = 0;
#define PREVIEW_FRAMES 120   // a 2 s test swell when the hum is adjusted outside a session
#define HUM_FLOOR 150
#define HUM_CEIL 260

static void hum_start(void) {
    if (hum_voice >= 0) return;
    hum_voice = soundPlaySample(sine, SoundFormat_8Bit, sizeof sine, hum_hz * 32, 0, 64, true, 0);
}

static void hum_stop(void) {
    if (hum_voice >= 0) soundKill(hum_voice);
    hum_voice = -1;
}

static void hum_preview(void) {
    if (buzz_mode != BUZZ_JACK) return;
    hum_start();
    soundSetFreq(hum_voice, hum_hz * 32);
    preview_left = PREVIEW_FRAMES;
}

// level: 0-1 (breath fullness x end-of-session fade), or <0 for "not in a session".
static void hum_tick(float level) {
    if (hum_voice < 0) return;
    float max = 25.0f * strength;   // strength 1-5 -> volume up to 125 of 127
    if (level >= 0) { soundSetVolume(hum_voice, (u8)(max * level)); return; }
    if (preview_left > 0) {
        preview_left--;
        soundSetVolume(hum_voice, (u8)(max * sinf(M_PI * (PREVIEW_FRAMES - preview_left) / PREVIEW_FRAMES)));
        if (preview_left == 0) hum_stop();
    }
}

// ---------------------------------------------------------------- settings file

#define SETTINGS_PATH "/data/breathing-room.cfg"

static void save_settings(void) {
    if (!has_fat) return;
    FILE *f = fopen(SETTINGS_PATH, "w");
    if (!f) return;
    fprintf(f, "%d %d %d %d %d %d %d %d %d\n", rhythm, length, tones, buzz_mode, hum_hz, strength,
            glow, dark, lights_out);
    fclose(f);
}

static void load_settings(void) {
    if (!has_fat) return;
    FILE *f = fopen(SETTINGS_PATH, "r");
    if (!f) return;
    int v[9];
    if (fscanf(f, "%d %d %d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6],
               &v[7], &v[8]) == 9) {
        rhythm = v[0] % 4; length = v[1] % N_LENGTHS; tones = !!v[2]; buzz_mode = v[3] % 3;
        hum_hz = v[4] < HUM_FLOOR || v[4] > HUM_CEIL ? 235 : v[4];
        strength = v[5] < 1 || v[5] > 5 ? 2 : v[5];
        glow = !!v[6]; dark = !!v[7]; lights_out = !!v[8];
    }
    fclose(f);
}

// ---------------------------------------------------------------- lights out

// With LIGHTS OUT on, both backlights switch off a few seconds into a session and
// stay off through the end, so finishing doesn't light up a dark room. Any button
// or touch turns them back on for a few seconds.
#define LIGHTS_DELAY (5 * 60)   // vblanks
static int lit = 1;
static u32 last_input = 0;

static void backlights(int on) {
    if (on == lit) return;
    lit = on;
    if (on) powerOn(PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
    else powerOff(PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
}

// ---------------------------------------------------------------- touchscreen

#define ROW_RHYTHM 4
#define ROW_LENGTH 7
#define ROW_TONES 9
#define ROW_BUZZ 10
#define ROW_HZ 11
#define ROW_STRENGTH 12
#define ROW_GLOW 13
#define ROW_THEME 14
#define ROW_LIGHTS 15
#define ROW_GO 18
#define ROW_TIMER 21
#define FOCUS_ITEMS 9

static void fmt_time(char *out, int secs) { snprintf(out, 16, "%d:%02d", secs / 60, secs % 60); }

static void draw_ui(void) {
    static const int label_rows[FOCUS_ITEMS] = {ROW_RHYTHM - 1, ROW_LENGTH - 1, ROW_TONES, ROW_BUZZ,
                                                ROW_HZ, ROW_STRENGTH, ROW_GLOW, ROW_THEME, ROW_LIGHTS};
    static const char *BUZZ_NAMES[3] = {"off", "headphone jack", "rumble pak"};
    consoleClear();
    printf("\x1b[1;1HBREATHING ROOM");
    printf("\x1b[%d;1HRHYTHM", ROW_RHYTHM - 1);
    printf("\x1b[%d;1H", ROW_RHYTHM);
    for (int i = 0; i < 4; i++)
        printf(i == rhythm ? "[%d-%d] " : " %d-%d  ", RHYTHMS[i][0], RHYTHMS[i][1]);
    printf("\x1b[%d;1HLENGTH", ROW_LENGTH - 1);
    printf("\x1b[%d;1H", ROW_LENGTH);
    for (int i = 0; i < N_LENGTHS; i++) printf(i == length ? "[%2dm]" : " %2dm ", LENGTHS[i]), printf(" ");
    printf("\x1b[%d;1HTONES  %s", ROW_TONES, tones ? "[on]  off" : " on  [off]");
    printf("\x1b[%d;1HBUZZ   < %s >", ROW_BUZZ, BUZZ_NAMES[buzz_mode]);
    printf("\x1b[%d;1HHUM    < %d Hz >", ROW_HZ, hum_hz);
    printf("\x1b[%d;1HSTRENGTH < %d of 5 >", ROW_STRENGTH, strength);
    printf("\x1b[%d;1HGLOW   %s", ROW_GLOW, glow ? "[on]  off" : " on  [off]");
    printf("\x1b[%d;1HTHEME  %s", ROW_THEME, dark ? "[dark]  light" : " dark  [light]");
    printf("\x1b[%d;1HLIGHTS OUT %s", ROW_LIGHTS, lights_out ? "[on]  off" : " on  [off]");
    printf("\x1b[%d;0H>", label_rows[focus]);
    printf("\x1b[%d;8H+--------------+", ROW_GO - 1);
    printf("\x1b[%d;8H|    %s    |", ROW_GO, running ? " STOP " : "BEGIN ");
    printf("\x1b[%d;8H+--------------+", ROW_GO + 1);
    printf("\x1b[23;1HA begin/stop  +pad settings");
}

static void draw_timer(void) {
    int total = LENGTHS[length] * 60, el = running ? (int)(frames / FPS) : 0;
    char a[16], b[16];
    fmt_time(a, el);
    fmt_time(b, total - el);
    printf("\x1b[%d;1H%-6s           %6s left", ROW_TIMER, a, b);
}

static void start(void) {
    running = 1; done = 0; frames = 0; started_at = vblanks; last_phase = -1; last_input = vblanks;
    preview_left = 0;
    if (buzz_mode == BUZZ_JACK) hum_start();
}

static void stop(int finished) {
    running = 0; done = finished;
    hum_stop();
    if (finished) { tone(659, 2.4f); finish_chime = 30; vibrate_pattern(BUZZ_DONE); }
    else backlights(1);   // stopped by hand: you want to see the screen
}

static void adjust(int row, int dir) {
    if (row == 0) rhythm = (rhythm + dir + 4) % 4;
    if (row == 1) length = (length + dir + N_LENGTHS) % N_LENGTHS;
    if (row == 2) tones = !tones;
    if (row == 3) {
        do buzz_mode = (buzz_mode + dir + 3) % 3; while (buzz_mode == BUZZ_PAK && !has_rumble);
        hum_stop();
        if (buzz_mode == BUZZ_JACK) { if (running) hum_start(); else hum_preview(); }
    }
    if (row == 4) {
        hum_hz += 5 * dir;
        hum_hz = hum_hz < HUM_FLOOR ? HUM_FLOOR : hum_hz > HUM_CEIL ? HUM_CEIL : hum_hz;
        if (running && hum_voice >= 0) soundSetFreq(hum_voice, hum_hz * 32); else hum_preview();
    }
    if (row == 5) {
        strength += dir;
        strength = strength < 1 ? 1 : strength > 5 ? 5 : strength;
        if (!running) hum_preview();
    }
    if (row == 6) glow = !glow;
    if (row == 7) { dark = !dark; set_sub_palette(); }
    if (row == 8) lights_out = !lights_out;
    save_settings();
}

static void handle_touch(touchPosition t) {
    int row = t.py / 8, col = t.px / 8;
    int dir = col < 16 ? -1 : 1;   // < > rows: left half steps down, right half up
    if (row == ROW_RHYTHM && col >= 1 && col < 25) { rhythm = (col - 1) / 6; focus = 0; save_settings(); }
    else if (row == ROW_LENGTH && col >= 1 && col < 31) { length = (col - 1) / 6; focus = 1; save_settings(); }
    else if (row == ROW_TONES) { focus = 2; adjust(2, 1); }
    else if (row == ROW_BUZZ) { focus = 3; adjust(3, dir); }
    else if (row == ROW_HZ) { focus = 4; adjust(4, dir); }
    else if (row == ROW_STRENGTH) { focus = 5; adjust(5, dir); }
    else if (row == ROW_GLOW) { focus = 6; adjust(6, 1); }
    else if (row == ROW_THEME) { focus = 7; adjust(7, 1); }
    else if (row == ROW_LIGHTS) { focus = 8; adjust(8, 1); }
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
    set_sub_palette();
    sysSetCartOwner(BUS_OWNER_ARM9);   // the GBA slot, for the Rumble Pak
    has_rumble = isRumbleInserted();

    soundEnable();
    for (int i = 0; i < 32; i++) {
        wave[i] = (s8)(i < 16 ? -96 + i * 12 : 96 - (i - 16) * 12);
        sine[i] = (s8)lroundf(120 * sinf(2 * M_PI * i / 32));
    }
    DC_FlushRange(wave, sizeof wave);
    DC_FlushRange(sine, sizeof sine);

    has_fat = fatInitDefault();
    load_settings();
    if (buzz_mode == BUZZ_PAK && !has_rumble) buzz_mode = BUZZ_OFF;
    set_sub_palette();

    draw_layer();
    init_progress();
    draw_ui();
    irqSet(IRQ_VBLANK, on_vblank);
    irqEnable(IRQ_VBLANK);
    u32 last_tick = vblanks;

    while (1) {
        scanKeys();
        int down = keysDown();
        if (down) {   // any input wakes the screens; while dark, that press only wakes
            int was_dark = !lit;
            last_input = vblanks;
            backlights(1);
            if (was_dark) down = 0;
        }
        if (down & (KEY_A | KEY_START)) { running ? stop(0) : start(); draw_ui(); }
        if (down & KEY_UP) { focus = (focus + FOCUS_ITEMS - 1) % FOCUS_ITEMS; draw_ui(); }
        if (down & KEY_DOWN) { focus = (focus + 1) % FOCUS_ITEMS; draw_ui(); }
        if (down & KEY_LEFT) { adjust(focus, -1); draw_ui(); }
        if (down & KEY_RIGHT) { adjust(focus, 1); draw_ui(); }
        if (down & KEY_TOUCH) { touchPosition t; touchRead(&t); handle_touch(t); }

        int tin = RHYTHMS[rhythm][0], tout = RHYTHMS[rhythm][1];
        float e = 0, f = 0, fade = 1;
        char big[8] = "READY", small[12];
        int tenths = (600 + (tin + tout) / 2) / (tin + tout);   // breaths per minute, one decimal
        sprintf(small, "%d.%d/MIN", tenths / 10, tenths % 10);

        if (running) frames = vblanks - started_at;
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
                fade = total - el < 60 ? (total - el) / 60 : 1;   // ease out over the last minute
                if (inhale != last_phase) {
                    last_phase = inhale;
                    tone(inhale ? 523 : 392, inhale ? 1.2f : 1.6f);
                    vibrate_pattern(inhale ? BUZZ_IN : BUZZ_OUT);
                }
                strcpy(big, inhale ? "IN" : "OUT");
                sprintf(small, "%d", (int)ceilf(inhale ? tin - p : cyc - p));
            }
        }
        if (!running && done) strcpy(big, "DONE");
        // Envelopes advance once per vblank that passed, even if this loop pass ran long.
        for (u32 now = vblanks; last_tick != now; last_tick++) {
            if (finish_chime && --finish_chime == 0) tone(523, 2.8f);
            tone_tick();
            vibrate_tick();
            if (!running) hum_tick(-1);
        }
        if (running) hum_tick(e * fade);
        if (running && lights_out && vblanks - last_input > LIGHTS_DELAY) backlights(0);

        set_palette(running && glow ? e : 0);
        memcpy(frame, layer, sizeof frame);
        int r = running ? R_MIN + (int)((R_MAX - R_MIN) * e) : R_IDLE;
        disk(frame, r, P_SPOT);
        disk(frame, r - 2, P_WELL);
        if (running) progress(f);
        int words = strlen(big) > 3;   // READY / DONE are wider than IN / OUT
        text(big, CY - 6, words ? 3 : 4, P_INK);
        text(small, CY + (words ? 18 : 22), 2, P_INKSOFT);

        DC_FlushRange(frame, sizeof frame);
        swiWaitForVBlank();   // copy during vblank so the ring never tears
        dmaCopy(frame, vram, sizeof frame);
        static int last_second = -1;
        int second = running ? (int)(frames / FPS) : -2 - length;
        if (second != last_second) { last_second = second; draw_timer(); }
    }
    return 0;
}
