/* SPDX-FileCopyrightText: 2026 jerbanpls
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * port: the quick menu's tiles for the time, overlay, controls and save
 * features (frontend/quickmenu.cpp), against the REAL settings rows (the
 * registrars of timectl.c, overlays.c, input.c, touch.c and saves.c, not
 * copies), so a tile bound to a wrong key or label, which the menu leaves
 * out silently, fails here. Headless, no ROM: SDL's dummy video driver and
 * a software renderer.
 *
 * Checks: every tile is there with its title and value, in section order;
 * one tile of each kind runs by pad, by mouse and by a finger; Reset and
 * Restore need two presses; every tile can be reached by the pad in
 * landscape and in portrait; the pictures of the open sheet.
 *
 * usage: test_quickmenu_tiles [SHOT_DIR] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL.h>

#include "actions.h"
#include "input.h"
#include "menu.h"
#include "overlays.h"
#include "quickmenu.h"
#include "saves.h"
#include "settings.h"
#include "timectl.h"
#include "touch.h"

/* the runtime's save RAM, which a restore replaces */
extern unsigned char *bus_sram(void);

static long checks, fails;
#define CHECK(c, ...)                                             \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            fails++;                                              \
            fprintf(stderr, "%s:%d: FAIL: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                         \
            fputc('\n', stderr);                                  \
        }                                                         \
    } while (0)

static SDL_Window *win;
static SDL_Renderer *ren;
static int W, H;
static int frame_steps;
static const char *shots;

static int get_i(void *u) { return *(int *)u; }
static void set_i(void *u, int v) { *(int *)u = v; }
static int get_vol(void *u) { return *(int *)u; }
static void set_vol(void *u, int v) { *(int *)u = v; }

static void on_step(ct_action a, int down, void *u)
{
    (void)a, (void)u;
    if (down)
        frame_steps++;
}

static void frame(void)
{
    SDL_SetRenderDrawColor(ren, 40, 90, 160, 255);
    SDL_RenderClear(ren);
    quickmenu_draw(W, H);
}
static void send(const SDL_Event *e) { quickmenu_event(e); }
static void button(int b)
{
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_CONTROLLERBUTTONDOWN;
    e.cbutton.button = (Uint8)b;
    send(&e);
    e.type = SDL_CONTROLLERBUTTONUP;
    send(&e);
}
static void mouse_click(int x, int y)
{
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_MOUSEMOTION;
    e.motion.x = x, e.motion.y = y, e.motion.xrel = 1;
    send(&e);
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = x, e.button.y = y;
    send(&e);
    e.type = SDL_MOUSEBUTTONUP;
    send(&e);
}
static void finger(int type, float x, float y)
{
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = (Uint32)type;
    e.tfinger.touchId = 1;
    e.tfinger.fingerId = 7;
    e.tfinger.x = x / W;
    e.tfinger.y = y / H;
    send(&e);
}

static int find(const char *title)
{
    for (int i = 0; i < quickmenu_tile_count(); i++)
        if (!strcmp(quickmenu_tile_title(i), title))
            return i;
    return -1;
}

static void focus_on(int i);

static void open_fresh(void)
{
    if (quickmenu_is_open())
        quickmenu_close();
    quickmenu_open();
    quickmenu_tick(400);
    frame();
}

/* the tile's centre on screen after scrolling it into view */
static int centre(int i, float *x, float *y)
{
    int rx, ry, rw, rh;
    focus_on(i);
    frame();
    if (quickmenu_tile_rect(i, &rx, &ry, &rw, &rh))
        return -1;
    *x = rx + rw / 2.0f, *y = ry + rh / 2.0f;
    return 0;
}

static const char *value(const char *title, char *buf, unsigned n)
{
    int i = find(title);
    return i < 0 ? "(missing)" : quickmenu_tile_value(i, buf, n);
}

/* ---- the save file the Restore tile works on ---- */
static char tmp[4096];
static time_t now;
static time_t fake_clock(void) { return now; }
static void version(int v)
{
    memset(bus_sram(), v, 8192);
    saves_flush();
    now += 60;
}

static void make_window(int w, int h)
{
    win = SDL_CreateWindow("qm", 0, 0, w, h, SDL_WINDOW_HIDDEN);
    ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    if (ren) {
        SDL_GetRendererOutputSize(ren, &W, &H);
        menu_host mh = {win, ren, 0, "CT Recomp", "test"};
        menu_init(&mh);
    }
}
static void drop_window(void)
{
    menu_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
}

/* every title the new tiles bring, in the order of their sections */
static const char *const NEW[] = {
    "Reset",
    "Game speed", "Rewind length", "Frame step", "Rewind",
    "FPS", "Clock", "Battery", "Play time", "Corner",
    "Swap A/B", "Swap X/Y", "Dead zone", "Touch size", "Touch opacity", "Touch hand",
    "Back up now", "Backup", "Restore",
};
#define N_NEW (int)(sizeof NEW / sizeof *NEW)

static void tiles_exist(void)
{
    int last = -1;
    for (int k = 0; k < N_NEW; k++) {
        int i = find(NEW[k]);
        CHECK(i >= 0, "tile %s is missing (a wrong settings key or label leaves it out)", NEW[k]);
        CHECK(i > last, "tile %s is out of order (%d after %d)", NEW[k], i, last);
        if (i >= 0)
            last = i;
    }
    CHECK(find("All settings...") == quickmenu_tile_count() - 1, "All settings stays last");
    CHECK(find("Reset") > find("Volume") && find("Reset") < find("Game speed"),
          "Reset is in the Game section");
    char b[64];
    CHECK(!strcmp(value("Game speed", b, sizeof b), "1x"), "Game speed: %s", b);
    CHECK(!strcmp(value("Rewind length", b, sizeof b), "5 s"), "Rewind length: %s", b);
    CHECK(!strcmp(value("Corner", b, sizeof b), "Top right"), "Corner: %s", b);
    CHECK(!strcmp(value("Touch hand", b, sizeof b), "Right hand"), "Touch hand: %s", b);
    CHECK(strstr(value("Dead zone", b, sizeof b), "%"), "Dead zone: %s", b);
    CHECK(strstr(value("Touch size", b, sizeof b), "%"), "Touch size: %s", b);
    CHECK(strstr(value("Touch opacity", b, sizeof b), "%"), "Touch opacity: %s", b);
    CHECK(strlen(value("Backup", b, sizeof b)) > 8 && b[0] == '2', "Backup shows a date: %s", b);
}

/* The pad's moves: up, down, left, right, R1, L1. Left / right adjust a
   stepper or slider instead of moving, so a tile behind one can only be
   reached from above or below; search every tile the pad can get to. */
static const int MOVES[6] = {SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                             SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
                             SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                             SDL_CONTROLLER_BUTTON_LEFTSHOULDER};

static void defaults(void)   /* the values a walk by pad changes */
{
    timectl_speed_set(100);
    settings_set_value(settings_find("rewind_seconds"), 5);
    input_deadzone_set(INPUT_DEADZONE_DEFAULT);
    touch_options_set(TOUCH_OPACITY_DEFAULT, 100, 0);
    overlays_set_corner(OVERLAY_TR);
    if (settings_find("volume"))
        settings_set_value(settings_find("volume"), 50);
}

/* The pad's way to each tile, found by reachable(): focus_on replays it
   on a fresh sheet (a greedy walk would turn the steppers it crosses). */
static char pathto[64][48];
static int plen[64];

static void focus_on(int i)
{
    open_fresh();
    for (int k = 0; k < plen[i]; k++)
        button(MOVES[(int)pathto[i][k]]);
    frame();
}

/* Up / down keep the column they started in, so the state of the walk is
   the tile and that column (its x on screen). */
static int centre_x(int i)
{
    int rx, ry, rw, rh;
    frame();
    return quickmenu_tile_rect(i, &rx, &ry, &rw, &rh) ? -1 : rx + rw / 2;
}

static void reachable(const char *what)
{
    enum { MAXS = 512, MAXP = 48 };
    static char path[MAXS][MAXP];
    static int len[MAXS], sf[MAXS], sx[MAXS];
    int n = quickmenu_tile_count(), ns = 0, qh = 0;
    char seen[64] = {0};
    open_fresh();
    sf[0] = quickmenu_focus(), sx[0] = centre_x(sf[0]), ns = 1;
    seen[sf[0]] = 1;
    plen[sf[0]] = 0;
    while (qh < ns) {
        int at = qh++;
        for (int m = 0; m < 6; m++) {
            defaults();
            open_fresh();
            for (int k = 0; k < len[at]; k++)
                button(MOVES[(int)path[at][k]]);
            button(MOVES[m]);
            int f = quickmenu_focus();
            if (f < 0 || f >= n || ns >= MAXS || len[at] + 1 >= MAXP)
                continue;
            int gx = (f != sf[at] && m >= 2) ? centre_x(f) : sx[at];
            int dup = 0;
            for (int k = 0; k < ns; k++)
                dup |= sf[k] == f && sx[k] == gx;
            if (dup)
                continue;
            memcpy(path[ns], path[at], (size_t)len[at]);
            path[ns][len[at]] = (char)m;
            len[ns] = len[at] + 1, sf[ns] = f, sx[ns] = gx;
            if (!seen[f]) {
                memcpy(pathto[f], path[ns], (size_t)len[ns]);
                plen[f] = len[ns];
            }
            seen[f] = 1;
            ns++;
        }
    }
    for (int i = 0; i < n; i++)
        CHECK(seen[i], "%s: the pad cannot reach tile %d (%s)", what, i, quickmenu_tile_title(i));
    defaults();
    quickmenu_close();
}

static void activations(void)
{
    char b[64];
    float x, y;
    int A = SDL_CONTROLLER_BUTTON_A;

    /* a toggle by pad: Swap A/B */
    open_fresh();
    int ab, xy;
    focus_on(find("Swap A/B"));
    button(A);
    input_swap_get(&ab, &xy);
    CHECK(ab == 1 && xy == 0, "Swap A/B by pad: %d %d", ab, xy);
    button(A);
    input_swap_get(&ab, &xy);
    CHECK(ab == 0, "and back");
    /* a toggle by mouse: FPS */
    CHECK(!centre(find("FPS"), &x, &y), "FPS in view");
    mouse_click((int)x, (int)y);
    CHECK(overlays_get(OVERLAY_FPS) == 1, "FPS by mouse");
    mouse_click((int)x, (int)y);
    CHECK(overlays_get(OVERLAY_FPS) == 0, "FPS off again");
    /* a toggle by finger: Clock */
    CHECK(!centre(find("Clock"), &x, &y), "Clock in view");
    finger(SDL_FINGERDOWN, x, y);
    finger(SDL_FINGERUP, x, y);
    CHECK(overlays_get(OVERLAY_CLOCK) == 1, "Clock by finger");
    overlays_set(OVERLAY_CLOCK, 0);
    /* the other overlay toggles by key */
    focus_on(find("Battery"));
    button(A);
    CHECK(overlays_get(OVERLAY_BATTERY) == 1, "Battery");
    overlays_set(OVERLAY_BATTERY, 0);
    focus_on(find("Play time"));
    button(A);
    CHECK(overlays_get(OVERLAY_PLAYTIME) == 1, "Play time");
    overlays_set(OVERLAY_PLAYTIME, 0);
    focus_on(find("Swap X/Y"));
    button(A);
    input_swap_get(&ab, &xy);
    CHECK(xy == 1, "Swap X/Y");
    input_swap_set(0, 0);

    /* a stepper by pad, mouse and finger: Game speed, the corner, the hand */
    focus_on(find("Game speed"));
    button(A);
    CHECK(timectl_speed_pct() == 200, "Game speed by pad: %d", timectl_speed_pct());
    CHECK(!strcmp(value("Game speed", b, sizeof b), "2x"), "shows 2x: %s", b);
    CHECK(!centre(find("Game speed"), &x, &y), "speed in view");
    mouse_click((int)x, (int)y);
    CHECK(timectl_speed_pct() == 300, "Game speed by mouse: %d", timectl_speed_pct());
    finger(SDL_FINGERDOWN, x, y);
    finger(SDL_FINGERUP, x, y);
    CHECK(timectl_speed_pct() == 400, "Game speed by finger: %d", timectl_speed_pct());
    timectl_speed_set(100);
    focus_on(find("Corner"));
    button(A);
    CHECK(overlays_get_corner() == OVERLAY_BL,
          "Corner steps: %d", overlays_get_corner());
    overlays_set_corner(OVERLAY_TR);
    int o, s, h;
    focus_on(find("Touch hand"));
    button(A);
    touch_options_get(&o, &s, &h);
    CHECK(h == 1, "Touch hand by pad: %d", h);
    button(A);
    touch_options_get(&o, &s, &h);
    CHECK(h == 0, "and round");

    /* sliders: Dead zone and Touch size by pad, Touch opacity by a drag */
    int dz = input_deadzone_get();
    focus_on(find("Dead zone"));
    button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    CHECK(input_deadzone_get() == dz + 1, "Dead zone slider: %d -> %d", dz, input_deadzone_get());
    focus_on(find("Touch size"));
    touch_options_get(&o, &s, &h);
    int size0 = s;
    button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    touch_options_get(&o, &s, &h);
    CHECK(s == size0 - 5, "Touch size slider: %d -> %d", size0, s);
    int rx, ry, rw, rh;
    CHECK(!centre(find("Touch opacity"), &x, &y), "opacity in view");
    quickmenu_tile_rect(find("Touch opacity"), &rx, &ry, &rw, &rh);
    finger(SDL_FINGERDOWN, rx + rw * 0.5f, ry + rh * 0.7f);
    finger(SDL_FINGERMOTION, rx + rw * 0.9f, ry + rh * 0.7f);
    finger(SDL_FINGERMOTION, rx + rw * 0.99f, ry + rh * 0.7f);
    finger(SDL_FINGERUP, rx + rw * 0.99f, ry + rh * 0.7f);
    touch_options_get(&o, &s, &h);
    CHECK(o >= 90, "Touch opacity by a drag: %d", o);
    button(SDL_CONTROLLER_BUTTON_B);   /* closes */
    CHECK(!quickmenu_is_open(), "B closes");

    /* Frame step: a button that closes the menu and emits the action */
    open_fresh();
    frame_steps = 0;
    focus_on(find("Frame step"));
    button(A);
    CHECK(frame_steps == 1 && !quickmenu_is_open(), "Frame step: %d, open %d", frame_steps,
          quickmenu_is_open());
    open_fresh();
    CHECK(!centre(find("Frame step"), &x, &y), "step in view");
    mouse_click((int)x, (int)y);
    CHECK(frame_steps == 2 && !quickmenu_is_open(), "Frame step by mouse");
    open_fresh();
    CHECK(!centre(find("Frame step"), &x, &y), "step in view");
    finger(SDL_FINGERDOWN, x, y);
    finger(SDL_FINGERUP, x, y);
    CHECK(frame_steps == 3 && !quickmenu_is_open(), "Frame step by finger");

    /* Rewind: closes the menu; with no states yet it refuses on its own */
    open_fresh();
    focus_on(find("Rewind"));
    button(A);
    CHECK(!quickmenu_is_open() && !timectl_rewinding(), "Rewind closes; nothing to rewind yet");
    settings_set_value(settings_find("rewind_seconds"), 0);
    open_fresh();
    focus_on(find("Rewind"));
    button(A);
    CHECK(quickmenu_is_open(), "Rewind is off when the length is 0");
    settings_set_value(settings_find("rewind_seconds"), 5);
    quickmenu_close();

    /* Reset: two presses, then the request and the menu closes */
    open_fresh();
    game_reset_take();   /* clear */
    focus_on(find("Reset"));
    button(A);
    CHECK(quickmenu_is_open() && !game_reset_take(), "Reset: the first press only asks");
    button(A);
    CHECK(!quickmenu_is_open() && game_reset_take(), "Reset: the second press resets");
    open_fresh();
    focus_on(find("Reset"));
    button(A);
    button(SDL_CONTROLLER_BUTTON_DPAD_UP);   /* moving the ring cancels the ask */
    button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    button(A);
    CHECK(quickmenu_is_open() && !game_reset_take(), "Reset: a move in between starts over");
    quickmenu_close();

    /* Back up now, then Restore (two presses) of an older backup */
    saves_backup list[8];
    int nb0 = saves_backup_list(list, 8);
    focus_on(find("Back up now"));
    version(9);   /* the file changes: a new backup is due */
    button(A);
    SDL_Delay(1100);   /* the rows' list refreshes once a second */
    int nb1 = saves_backup_list(list, 8);
    CHECK(nb1 >= nb0 && nb1 >= 2, "Back up now: %d -> %d backups", nb0, nb1);
    focus_on(find("Backup"));
    CHECK(!strcmp(value("Backup", b, sizeof b), list[0].label), "Backup 1 is the newest: %s vs %s (%d backups, %s)", b, list[0].label, nb1, list[1].label);
    button(A);
    CHECK(!strcmp(value("Backup", b, sizeof b), list[1].label), "Backup steps to the next: %s", b);
    int want = -1;
    {   /* what backup 2 holds */
        char p[4400];
        snprintf(p, sizeof p, "%s/saves/ct.srm.backup-%s", tmp, list[1].stamp);
        FILE *f = fopen(p, "rb");
        want = f ? fgetc(f) : -1;
        if (f)
            fclose(f);
    }
    CHECK(want > 0, "backup 2 holds version %d", want);
    memset(bus_sram(), 99, 8192);
    CHECK(!centre(find("Restore"), &x, &y), "Restore in view");   /* (a fresh sheet; the choice starts over) */
    focus_on(find("Backup"));
    button(A);
    CHECK(!strcmp(value("Backup", b, sizeof b), list[1].label), "stepped again: %s", b);
    {   /* the pointer moves the ring to Restore; the pad presses it */
        int rx, ry, rw, rh;
        frame();
        CHECK(!quickmenu_tile_rect(find("Restore"), &rx, &ry, &rw, &rh), "Restore on screen");
        SDL_Event e;
        memset(&e, 0, sizeof e);
        e.type = SDL_MOUSEMOTION;
        e.motion.x = rx + rw / 2, e.motion.y = ry + rh / 2, e.motion.xrel = 1;
        send(&e);
    }
    CHECK(quickmenu_focus() == find("Restore"), "the ring is on Restore");
    button(A);
    CHECK(bus_sram()[0] == 99, "Restore: the first press only asks (%d)", bus_sram()[0]);
    button(A);
    CHECK(bus_sram()[0] == want, "Restore: the second press restores (%d, want %d)",
          bus_sram()[0], want);
    quickmenu_close();

    /* a touch on Restore twice, also needing both */
    open_fresh();
    version(78);
    CHECK(!centre(find("Restore"), &x, &y), "Restore in view");
    finger(SDL_FINGERDOWN, x, y);
    finger(SDL_FINGERUP, x, y);
    CHECK(bus_sram()[0] == 78, "Restore by finger: asks first");
    finger(SDL_FINGERDOWN, x, y);
    finger(SDL_FINGERUP, x, y);
    CHECK(bus_sram()[0] != 78, "Restore by finger: restores the second time");
    quickmenu_close();
}

static void one_orientation(int w, int h, const char *name, int cols)
{
    make_window(w, h);
    CHECK(ren != NULL, "%s renderer", name);
    if (!ren)
        return;
    open_fresh();
    CHECK(quickmenu_columns() == cols, "%s: %d columns", name, quickmenu_columns());
    tiles_exist();
    for (int i = 0; i < quickmenu_tile_count(); i++)
        CHECK(quickmenu_tile_title(i)[0], "tile %d has a title", i);
    reachable(name);
    if (shots) {
        char p[700];
        open_fresh();
        snprintf(p, sizeof p, "%s/quickmenu-tiles-%s.png", shots, name);
        CHECK(!menu_shot(p, W, H), "picture %s", p);
        /* and scrolled to the Saves end */
        focus_on(find("Restore"));
        frame();
        snprintf(p, sizeof p, "%s/quickmenu-tiles-%s-end.png", shots, name);
        CHECK(!menu_shot(p, W, H), "picture %s", p);
    }
    if (!strcmp(name, "landscape"))
        activations();
    drop_window();
}

int main(int argc, char **argv)
{
    shots = argc > 1 ? argv[1] : NULL;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *probe = SDL_CreateWindow("p", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer *pr = probe ? SDL_CreateRenderer(probe, -1, SDL_RENDERER_SOFTWARE) : NULL;
    if (!pr) {
        fprintf(stderr, "test_quickmenu_tiles: no software renderer here: %s\n", SDL_GetError());
        return 77;
    }
    SDL_DestroyRenderer(pr);
    SDL_DestroyWindow(probe);

    const char *t = getenv("TMPDIR");
    snprintf(tmp, sizeof tmp, "%s/ct_test_qmt_XXXXXX", t && *t ? t : "/tmp");
    if (!mkdtemp(tmp))
        return 1;
    char dir[4200], srm[4300];
    snprintf(dir, sizeof dir, "%s/saves", tmp);
    mkdir(dir, 0755);
    snprintf(srm, sizeof srm, "%s/ct.srm", dir);
    saves_set_clock(fake_clock);
    now = 1780000000;
    CHECK(saves_open(srm, "ct") == 0, "saves_open");
    for (int v = 1; v <= 4; v++)
        version(v);

    /* the rows main.c and qol.c register for the Look section and the volume */
    static const char *const aspects[] = {"4:3", "8:7", "Stretch"};
    static int vol = 50, asp, fs, in = 1, crt;
    settings_register(&(setting){.key = "volume", .label = "Volume", .category = "Audio",
                                 .type = SETTING_INT, .min = 0, .max = 100, .step = 5,
                                 .format = "%d%%", .get = get_vol, .set = set_vol, .user = &vol});
    settings_register(&(setting){.key = "aspect", .label = "Aspect", .category = "Look",
                                 .type = SETTING_ENUM, .options = aspects, .count = 3,
                                 .get = get_i, .set = set_i, .user = &asp});
    settings_register(&(setting){.key = "fullscreen", .label = "Fullscreen", .category = "Display",
                                 .type = SETTING_BOOL, .get = get_i, .set = set_i, .user = &fs});
    settings_register(&(setting){.label = "Integer scale", .category = "Display",
                                 .type = SETTING_BOOL, .get = get_i, .set = set_i, .user = &in});
    settings_register(&(setting){.label = "CRT look", .category = "Display", .type = SETTING_BOOL,
                                 .get = get_i, .set = set_i, .user = &crt});
    /* the real rows, as main.c registers them */
    timectl_init();
    input_register_settings();
    touch_register_settings();
    overlays_register_settings();
    saves_register_settings();
    actions_set_handler(ACTION_FRAMESTEP, on_step, NULL);   /* counts the presses */

    one_orientation(1280, 800, "landscape", 4);
    one_orientation(800, 1280, "portrait", 3);

    char cmd[4300];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", tmp);
    if (system(cmd)) {
    }
    SDL_Quit();
    printf("test_quickmenu_tiles: %ld checks, %ld failures\n", checks, fails);
    return fails ? 1 : 0;
}
