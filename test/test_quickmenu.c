/* SPDX-FileCopyrightText: 2026 jerbanpls
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * port: the F1 quick menu (frontend/quickmenu.h), headless and without a
 * ROM: SDL's dummy video driver with a software SDL_Renderer (the plain
 * path's ImGui backend). Opens it, moves the focus ring with the d-pad,
 * the stick and keys, activates tiles with A and Enter, adjusts a slider
 * and a stepper, taps and drags with the mouse and with finger events,
 * opens the F10 menu from "All settings...", and draws the sheet at
 * 1280x800 and 800x1280 (PNG shots when a folder is given).
 *
 * usage: test_quickmenu [SHOT_DIR] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "actions.h"
#include "bindings.h"
#include "menu.h"
#include "quickmenu.h"
#include "settings.h"

static long checks, fails;
#define CHECK(c, ...)                                                    \
    do {                                                                 \
        checks++;                                                        \
        if (!(c)) {                                                      \
            fails++;                                                     \
            fprintf(stderr, "%s:%d: FAIL: ", __FILE__, __LINE__);        \
            fprintf(stderr, __VA_ARGS__);                                \
            fputc('\n', stderr);                                         \
        }                                                                \
    } while (0)

/* the rows the tiles bind to, as main.c and qol.c register them */
static int volume = 50, aspect, fullscreen, integer = 1, crt;
static int get_volume(void *u) { (void)u; return volume; }
static void set_volume(void *u, int v) { (void)u; volume = v; }
static int get_aspect(void *u) { (void)u; return aspect; }
static void set_aspect(void *u, int v) { (void)u; aspect = v; }
static int get_fs(void *u) { (void)u; return fullscreen; }
static void set_fs(void *u, int v) { (void)u; fullscreen = v; }
static int get_int(void *u) { (void)u; return integer; }
static void set_int(void *u, int v) { (void)u; integer = v; }
static int get_crt(void *u) { (void)u; return crt; }
static void set_crt(void *u, int v) { (void)u; crt = v; }

static int presses[ACTION_COUNT], looks, look_on = 1, saved_slot, loaded_slot;
static void handler(ct_action a, int down, void *user)
{
    (void)user;
    if (down)
        presses[a]++;
}
static void look_cycle(void) { looks++; }
static int look_enabled(void) { return look_on; }
static void save_slot(int s) { saved_slot = s; }
static void load_slot(int s) { loaded_slot = s; }

static SDL_Window *win;
static SDL_Renderer *ren;
static int W = 1280, H = 800;

static void frame(void)
{
    SDL_SetRenderDrawColor(ren, 200, 40, 40, 255);   /* the picture */
    SDL_RenderClear(ren);
    quickmenu_draw(W, H);
}

static void send(const SDL_Event *e) { quickmenu_event(e); }

static void button(int b, int down)
{
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
    e.cbutton.button = (Uint8)b;
    send(&e);
}
static void tap_button(int b)
{
    button(b, 1);
    button(b, 0);
}
static void key(SDL_Scancode sc)
{
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.scancode = sc;
    send(&e);
    e.type = SDL_KEYUP;
    send(&e);
}
static void axis(int a, int v)
{
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_CONTROLLERAXISMOTION;
    e.caxis.axis = (Uint8)a;
    e.caxis.value = (Sint16)v;
    send(&e);
}
static void mouse(int type, int x, int y)
{
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = (Uint32)type;
    if (type == SDL_MOUSEMOTION) {
        e.motion.x = x, e.motion.y = y, e.motion.xrel = 1;
    } else {
        e.button.button = SDL_BUTTON_LEFT;
        e.button.x = x, e.button.y = y;
    }
    send(&e);
}
static void finger(int type, float fx, float fy)
{
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = (Uint32)type;
    e.tfinger.touchId = 1;
    e.tfinger.fingerId = 5;
    e.tfinger.x = fx / W;
    e.tfinger.y = fy / H;
    send(&e);
}

static int find(const char *title)
{
    for (int i = 0; i < quickmenu_tile_count(); i++)
        if (!strcmp(quickmenu_tile_title(i), title))
            return i;
    return -1;
}

static void centre(int i, float *x, float *y)
{
    int rx, ry, rw, rh;
    if (quickmenu_tile_rect(i, &rx, &ry, &rw, &rh))
        *x = *y = -1;
    else
        *x = rx + rw / 2.0f, *y = ry + rh / 2.0f;
}

/* Walk the ring to tile i: rows first (up / down), then along the row. */
static void focus_on(int i)
{
    for (int n = 0; n < 40 && quickmenu_focus() != i; n++) {
        int fx, fy, fw, fh, tx, ty, tw, th;
        frame();   /* the sheet scrolls to the ring when drawn */
        if (quickmenu_tile_rect(quickmenu_focus(), &fx, &fy, &fw, &fh))
            return;
        if (quickmenu_tile_rect(i, &tx, &ty, &tw, &th)) {   /* out of view: scrolls as we go */
            tap_button(i > quickmenu_focus() ? SDL_CONTROLLER_BUTTON_DPAD_DOWN
                                             : SDL_CONTROLLER_BUTTON_DPAD_UP);
            continue;
        }
        if (abs(ty - fy) > fh / 2)
            tap_button(ty > fy ? SDL_CONTROLLER_BUTTON_DPAD_DOWN : SDL_CONTROLLER_BUTTON_DPAD_UP);
        else
            tap_button(tx > fx ? SDL_CONTROLLER_BUTTON_DPAD_RIGHT : SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    }
}

static void open_fresh(void)
{
    if (quickmenu_is_open())
        quickmenu_close();
    quickmenu_open();
    quickmenu_tick(400);   /* the slide is over */
    frame();
}

static Uint32 pixel(int x, int y)
{
    Uint32 p = 0;
    SDL_Rect r = {x, y, 1, 1};
    SDL_RenderReadPixels(ren, &r, SDL_PIXELFORMAT_ARGB8888, &p, 4);
    return p;
}
/* the sheet's panel is dark and not the picture's red; the picture above
   it is dimmed but still red */
static int sheet_at_bottom(void) { return ((pixel(W / 2, H - 4) >> 16) & 255) < 60; }
static int picture_dimmed(void)
{
    int r = (pixel(W / 2, 2) >> 16) & 255;
    return r > 40 && r < 200;
}

static void shot(const char *dir, const char *name)
{
    if (!dir)
        return;
    char path[600];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    CHECK(!menu_shot(path, W, H), "writing %s", path);
}

int main(int argc, char **argv)
{
    const char *shots = argc > 1 ? argv[1] : NULL;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    win = SDL_CreateWindow("qm", 0, 0, 1280, 800, SDL_WINDOW_HIDDEN);
    ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    if (!ren) {
        fprintf(stderr, "test_quickmenu: no software renderer here: %s\n", SDL_GetError());
        return 77;
    }
    SDL_GetRendererOutputSize(ren, &W, &H);

    static const char *const fs_values[] = {"0", "1"};
    static const char *const aspects[] = {"4:3", "8:7", "Stretch"};
    settings_register(&(setting){.key = "volume", .label = "Volume", .category = "Audio",
                                 .type = SETTING_INT, .min = 0, .max = 100, .step = 5,
                                 .format = "%d%%", .get = get_volume, .set = set_volume});
    settings_register(&(setting){.key = "aspect", .label = "Aspect", .category = "Look",
                                 .type = SETTING_ENUM, .options = aspects, .count = 3,
                                 .get = get_aspect, .set = set_aspect});
    settings_register(&(setting){.key = "fullscreen", .label = "Fullscreen", .category = "Display",
                                 .type = SETTING_BOOL, .values = fs_values, .get = get_fs,
                                 .set = set_fs});
    settings_register(&(setting){.label = "Integer scale", .category = "Display",
                                 .type = SETTING_BOOL, .get = get_int, .set = set_int});
    settings_register(&(setting){.label = "CRT look", .category = "Display", .type = SETTING_BOOL,
                                 .get = get_crt, .set = set_crt});
    for (int a = 1; a < ACTION_COUNT; a++)
        actions_set_handler((ct_action)a, handler, NULL);
    quickmenu_hooks hk = {look_cycle, look_enabled, save_slot, load_slot};
    quickmenu_set_hooks(&hk);

    menu_host mh = {win, ren, 0, "CT Recomp", "test"};
    menu_init(&mh);

    /* open and close */
    CHECK(!quickmenu_is_open(), "closed at the start");
    quickmenu_open();
    CHECK(quickmenu_is_open() && quickmenu_opened_count() == 1, "opens");
    quickmenu_tick(400);
    frame();
    int n = quickmenu_tile_count();
    CHECK(n >= 12, "tiles: %d", n);
    CHECK(!strcmp(quickmenu_tile_title(0), "Resume"), "first tile %s", quickmenu_tile_title(0));
    CHECK(find("All settings...") == n - 1, "All settings is last");
    CHECK(find("Volume") == 7 && find("Mute") == 3, "the Game section: %d tiles before Look", find("Look"));
    CHECK(quickmenu_focus() == 0, "focus starts on Resume");
    CHECK(quickmenu_columns() == 4, "4 columns landscape: %d", quickmenu_columns());
    for (int i = 0; i < n; i++) {
        int x, y, w, h;
        if (!quickmenu_tile_rect(i, &x, &y, &w, &h))
            CHECK(w >= 72 && h >= 72, "tile %d is %dx%d at 1280x800", i, w, h);
    }
    CHECK(sheet_at_bottom(), "the sheet is drawn at the bottom");
    CHECK(picture_dimmed(), "the picture above is dimmed, not covered");
    shot(shots, "quickmenu-1280x800.png");
    tap_button(SDL_CONTROLLER_BUTTON_B);
    CHECK(!quickmenu_is_open(), "B closes");
    open_fresh();
    tap_button(SDL_CONTROLLER_BUTTON_START);
    CHECK(!quickmenu_is_open(), "Start closes");
    open_fresh();
    key(SDL_SCANCODE_ESCAPE);
    CHECK(!quickmenu_is_open(), "Escape closes");
    open_fresh();
    tap_button(SDL_CONTROLLER_BUTTON_A);   /* Resume */
    CHECK(!quickmenu_is_open(), "A on Resume closes");

    /* focus movement */
    open_fresh();
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    CHECK(quickmenu_focus() == 1, "right: %d", quickmenu_focus());
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    CHECK(quickmenu_focus() == 5, "down: %d", quickmenu_focus());
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_UP);
    CHECK(quickmenu_focus() == 1, "up: %d", quickmenu_focus());
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    CHECK(quickmenu_focus() == 0, "left stops at the edge: %d", quickmenu_focus());
    axis(SDL_CONTROLLER_AXIS_LEFTX, 30000);
    axis(SDL_CONTROLLER_AXIS_LEFTX, 0);
    CHECK(quickmenu_focus() == 1, "stick right: %d", quickmenu_focus());
    key(SDL_SCANCODE_DOWN);
    CHECK(quickmenu_focus() == 5, "arrow down: %d", quickmenu_focus());
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_CONTROLLERBUTTONDOWN;   /* held: repeats after a while */
    e.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_UP;
    send(&e);
    CHECK(quickmenu_focus() == 1, "held up moved once");
    quickmenu_tick(1000);
    e.type = SDL_CONTROLLERBUTTONUP;
    send(&e);
    CHECK(quickmenu_focus() >= 0, "held repeat is safe");
    open_fresh();
    tap_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);   /* a page: next section */
    CHECK(!strcmp(quickmenu_tile_title(quickmenu_focus()), "Frame step"), "R1 to the next section (Time; its rows are not registered here): %s",
          quickmenu_tile_title(quickmenu_focus()));
    tap_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    CHECK(quickmenu_focus() == 0, "L1 back to Game: %d", quickmenu_focus());

    /* activation by pad: A runs the tile under the ring */
    open_fresh();
    memset(presses, 0, sizeof presses);
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(presses[ACTION_QUICKSAVE] == 1 && quickmenu_is_open(), "A: quick save, menu stays");
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(presses[ACTION_QUICKLOAD] == 1 && !quickmenu_is_open(), "A: quick load closes");

    /* the slot button, the slider (volume) and toggles */
    open_fresh();
    char buf[64];
    int slot = find("Slot");
    focus_on(slot);
    CHECK(!strcmp(quickmenu_tile_value(slot, buf, sizeof buf), "Slot 1"), "slot 1: %s", buf);
    tap_button(SDL_CONTROLLER_BUTTON_A);
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(!strcmp(quickmenu_tile_value(slot, buf, sizeof buf), "Slot 3"), "A steps the slot on: %s", buf);
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    CHECK(quickmenu_focus() == find("Save to slot"), "left / right move on a button");
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(saved_slot == 3 && quickmenu_is_open(), "save to the chosen slot: %d", saved_slot);
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(loaded_slot == 3 && !quickmenu_is_open(), "load the chosen slot: %d", loaded_slot);
    open_fresh();
    int vol = find("Volume");
    focus_on(vol);
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    CHECK(volume == 55, "slider right: %d", volume);
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    CHECK(volume == 45 && quickmenu_focus() == vol, "slider left: %d", volume);
    key(SDL_SCANCODE_RIGHT);
    CHECK(volume == 50, "arrow right on the slider: %d", volume);
    tap_button(SDL_CONTROLLER_BUTTON_DPAD_UP);
    CHECK(quickmenu_focus() != vol, "up leaves the slider");
    int mute = find("Mute");
    focus_on(mute);
    key(SDL_SCANCODE_RETURN);
    CHECK(volume == 0, "Mute: %d", volume);
    key(SDL_SCANCODE_RETURN);
    CHECK(volume == 50, "Unmute restores: %d", volume);
    focus_on(find("Fullscreen"));
    key(SDL_SCANCODE_SPACE);
    CHECK(fullscreen == 1, "Fullscreen toggles");
    key(SDL_SCANCODE_SPACE);
    focus_on(find("Aspect"));
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(aspect == 1, "Aspect steps: %d", aspect);
    focus_on(find("CRT look"));
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(crt == 1, "CRT toggles");
    focus_on(find("Integer scale"));
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(integer == 0, "Integer scale toggles");
    focus_on(find("Look"));
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(looks == 1, "the Look tile cycles the look: %d", looks);
    look_on = 0;
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(looks == 1, "the Look tile is off without the HD-2D renderer: %d", looks);
    look_on = 1;
    frame();

    /* mouse */
    open_fresh();
    float x, y;
    centre(find("Mute"), &x, &y);
    mouse(SDL_MOUSEMOTION, (int)x, (int)y);
    CHECK(quickmenu_focus() == find("Mute"), "hover focuses");
    mouse(SDL_MOUSEBUTTONDOWN, (int)x, (int)y);
    mouse(SDL_MOUSEBUTTONUP, (int)x, (int)y);
    CHECK(volume == 0, "a click on Mute: %d", volume);
    mouse(SDL_MOUSEBUTTONDOWN, (int)x, (int)y);
    mouse(SDL_MOUSEBUTTONUP, (int)x, (int)y);
    centre(find("Volume"), &x, &y);
    int vx, vy, vw, vh;
    quickmenu_tile_rect(find("Volume"), &vx, &vy, &vw, &vh);
    mouse(SDL_MOUSEBUTTONDOWN, vx + vw * 3 / 4, vy + vh / 2);
    mouse(SDL_MOUSEBUTTONUP, vx + vw * 3 / 4, vy + vh / 2);
    CHECK(volume >= 70 && volume <= 80, "a click on the slider sets it: %d", volume);
    mouse(SDL_MOUSEBUTTONDOWN, 10, 10);   /* above the sheet */
    mouse(SDL_MOUSEBUTTONUP, 10, 10);
    CHECK(!quickmenu_is_open(), "a click above the sheet closes");

    /* touch: finger events, the mouse copies of them ignored */
    open_fresh();
    memset(presses, 0, sizeof presses);
    centre(find("Quick save"), &x, &y);
    finger(SDL_FINGERDOWN, x, y);
    finger(SDL_FINGERUP, x, y);
    CHECK(presses[ACTION_QUICKSAVE] == 1, "a tap on Quick save: %d", presses[ACTION_QUICKSAVE]);
    memset(&e, 0, sizeof e);
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.which = SDL_TOUCH_MOUSEID;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = (int)x, e.button.y = (int)y;
    send(&e);
    e.type = SDL_MOUSEBUTTONUP;
    send(&e);
    CHECK(presses[ACTION_QUICKSAVE] == 1, "a finger's mouse copy is not a second tap");
    quickmenu_tile_rect(find("Volume"), &vx, &vy, &vw, &vh);
    finger(SDL_FINGERDOWN, vx + vw * 0.1f, vy + vh * 0.7f);
    finger(SDL_FINGERMOTION, vx + vw * 0.5f, vy + vh * 0.7f);
    finger(SDL_FINGERMOTION, vx + vw * 0.9f, vy + vh * 0.7f);
    finger(SDL_FINGERUP, vx + vw * 0.9f, vy + vh * 0.7f);
    CHECK(volume >= 85, "a drag on the slider: %d", volume);
    int slot_i = find("Slot");
    quickmenu_tile_rect(slot_i, &vx, &vy, &vw, &vh);
    finger(SDL_FINGERDOWN, vx + vw * 0.5f, vy + vh * 0.5f);
    finger(SDL_FINGERUP, vx + vw * 0.5f, vy + vh * 0.5f);
    CHECK(!strcmp(quickmenu_tile_value(slot_i, buf, sizeof buf), "Slot 4"), "tap on Slot: %s", buf);
    finger(SDL_FINGERDOWN, W / 2.0f, 20);   /* above the sheet */
    finger(SDL_FINGERUP, W / 2.0f, 20);
    CHECK(!quickmenu_is_open(), "a tap above the sheet closes");

    /* All settings... opens the F10 menu */
    open_fresh();
    focus_on(find("All settings..."));
    tap_button(SDL_CONTROLLER_BUTTON_A);
    CHECK(!quickmenu_is_open() && menu_is_open(), "All settings opens the menu");
    menu_close();

    /* portrait: a new window; 3 columns, still big tiles; the shot */
    menu_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    win = SDL_CreateWindow("qm", 0, 0, 800, 1280, SDL_WINDOW_HIDDEN);
    ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    CHECK(ren != NULL, "a portrait renderer");
    if (ren) {
        SDL_GetRendererOutputSize(ren, &W, &H);
        mh.win = win, mh.ren = ren;
        menu_init(&mh);
        open_fresh();
        CHECK(W == 800 && H == 1280, "output %dx%d", W, H);
        CHECK(quickmenu_columns() == 3, "3 columns portrait: %d", quickmenu_columns());
        for (int i = 0; i < quickmenu_tile_count(); i++) {
            int rx, ry, rw, rh;
            if (!quickmenu_tile_rect(i, &rx, &ry, &rw, &rh))
                CHECK(rw >= 72 && rh >= 72, "tile %d is %dx%d portrait", i, rw, rh);
        }
        CHECK(sheet_at_bottom(), "the sheet is drawn at the bottom (portrait)");
        shot(shots, "quickmenu-800x1280.png");
        quickmenu_close();
    }

    /* the bindings: the action, F1 and misc1 */
    CHECK(action_parse("quickmenu") == ACTION_QUICKMENU, "action name quickmenu");
    bindings b;
    bindings_defaults(&b);
    CHECK(bindings_for_key(&b, SDL_SCANCODE_F1) == ACTION_QUICKMENU, "F1 is the quick menu");
    CHECK(bindings_for_button(&b, SDL_CONTROLLER_BUTTON_MISC1) == ACTION_QUICKMENU, "misc1 too");

    menu_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    printf("test_quickmenu: %ld checks, %ld failures\n", checks, fails);
    return fails ? 1 : 0;
}
