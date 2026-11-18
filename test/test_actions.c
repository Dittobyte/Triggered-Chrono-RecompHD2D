/* SPDX-FileCopyrightText: 2026 jerbanpls
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Frontend actions (frontend/actions.h, bindings.h) through SDL virtual
 * game controllers shaped like a Steam Deck's built-in controller (Valve
 * 28de:1205, with paddles 1-4), a Steam Controller, an Xbox pad, a
 * DualSense and a Switch Pro pad: the back buttons fire the bound actions,
 * fast-forward holds, settings.ini lines rebind them, the keyboard keys do
 * the same, the SNES mapping ignores the paddles. Then the glyph sets
 * (glyphs.h), the message masks (osd_text.h) and gyro aiming
 * (input.h gyro_cam_step). No ROM, no window.
 *
 * usage: test_actions [MASK.png]   (MASK.png: a message's mask, to look at) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "actions.h"
#include "bindings.h"
#include "glyphs.h"
#include "input.h"
#include "osd_text.h"
#include "png.h"
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

/* What the handlers saw. */
static int presses[ACTION_COUNT], releases[ACTION_COUNT];
static char last_msg[128];

static void handler(ct_action a, int down, void *user)
{
    (void)user;
    if (down)
        presses[a]++;
    else
        releases[a]++;
}

static void message(const char *text)
{
    snprintf(last_msg, sizeof last_msg, "%s", text);
}

static void reset_counts(void)
{
    memset(presses, 0, sizeof presses);
    memset(releases, 0, sizeof releases);
}

static int pad_button(void *ctx, SDL_GameControllerButton b)
{
    return SDL_GameControllerGetButton(ctx, b);
}

static int pad_axis(void *ctx, SDL_GameControllerAxis a)
{
    return SDL_GameControllerGetAxis(ctx, a);
}

typedef struct {
    int idx;
    SDL_GameController *pad;
    SDL_Joystick *joy;
} vpad;

static vpad attach(uint16_t vid, uint16_t pid, const char *name)
{
    vpad v = {-1, NULL, NULL};
    SDL_VirtualJoystickDesc d;
    memset(&d, 0, sizeof d);
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    d.naxes = SDL_CONTROLLER_AXIS_MAX;
    d.nbuttons = SDL_CONTROLLER_BUTTON_MAX;   /* paddle1-4, misc1, touchpad included */
    d.vendor_id = vid;
    d.product_id = pid;
    d.name = name;
    v.idx = SDL_JoystickAttachVirtualEx(&d);
    CHECK(v.idx >= 0, "attach %s: %s", name, SDL_GetError());
    if (v.idx < 0)
        return v;
    v.pad = SDL_GameControllerOpen(v.idx);
    v.joy = v.pad ? SDL_GameControllerGetJoystick(v.pad) : NULL;
    CHECK(v.pad && v.joy, "open %s: %s", name, SDL_GetError());
    return v;
}

static void detach(vpad *v)
{
    if (v->pad)
        SDL_GameControllerClose(v->pad);
    if (v->idx >= 0)
        SDL_JoystickDetachVirtual(v->idx);
    v->pad = NULL;
    v->idx = -1;
}

static void press(vpad *v, SDL_GameControllerButton b, int down)
{
    SDL_JoystickSetVirtualButton(v->joy, b, down ? SDL_PRESSED : SDL_RELEASED);
}

/* One frontend frame: SDL's state, then the binder. */
static int frame(vpad *v, binder *s, const bindings *b)
{
    SDL_GameControllerUpdate();
    return binder_update(s, b, pad_button, v->pad);
}

static void test_deck(void)
{
    vpad v = attach(0x28de, 0x1205, "Steam Deck");
    if (!v.pad)
        return;
    CHECK(SDL_GameControllerHasButton(v.pad, SDL_CONTROLLER_BUTTON_PADDLE1) &&
              SDL_GameControllerHasButton(v.pad, SDL_CONTROLLER_BUTTON_PADDLE4),
          "the virtual Deck has paddles");
    bindings b;
    bindings_defaults(&b);
    binder s;
    binder_init(&s);
    reset_counts();
    CHECK(frame(&v, &s, &b) == 0, "idle: no actions");

    /* L4 (paddle2): quick save, once per press. */
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE2, 1);
    frame(&v, &s, &b);
    frame(&v, &s, &b);
    CHECK(presses[ACTION_QUICKSAVE] == 1, "L4 held 2 frames: one quick save (%d)",
          presses[ACTION_QUICKSAVE]);
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE2, 0);
    frame(&v, &s, &b);
    CHECK(releases[ACTION_QUICKSAVE] == 0, "a tap action gets no release");
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE2, 1);
    frame(&v, &s, &b);
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE2, 0);
    frame(&v, &s, &b);
    CHECK(presses[ACTION_QUICKSAVE] == 2, "a second press: a second quick save");

    /* R4 (paddle1) quick load, R5 (paddle3) menu. */
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE1, 1);
    frame(&v, &s, &b);
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE1, 0);
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE3, 1);
    frame(&v, &s, &b);
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE3, 0);
    frame(&v, &s, &b);
    CHECK(presses[ACTION_QUICKLOAD] == 1 && presses[ACTION_MENU] == 1, "R4 quick load, R5 menu");

    /* L5 (paddle4) fast-forward: held while held, with the Tab key too. */
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE4, 1);
    frame(&v, &s, &b);
    CHECK(presses[ACTION_FFWD] == 1 && actions_held(ACTION_FFWD) == 1, "L5 starts fast-forward");
    CHECK(bindings_key_event(&b, SDL_SCANCODE_TAB, 1, 0), "Tab is bound");
    CHECK(presses[ACTION_FFWD] == 1 && actions_held(ACTION_FFWD) == 2, "Tab too: one press");
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE4, 0);
    frame(&v, &s, &b);
    CHECK(releases[ACTION_FFWD] == 0, "L5 let go, Tab still held: still fast");
    CHECK(bindings_key_event(&b, SDL_SCANCODE_TAB, 1, 1), "a key repeat: claimed");
    CHECK(actions_held(ACTION_FFWD) == 1, "a repeat doesn't add a hold");
    bindings_key_event(&b, SDL_SCANCODE_TAB, 0, 0);
    CHECK(releases[ACTION_FFWD] == 1 && actions_held(ACTION_FFWD) == 0, "both up: it ends");

    /* The paddles are not SNES buttons. */
    for (int k = SDL_CONTROLLER_BUTTON_PADDLE1; k <= SDL_CONTROLLER_BUTTON_TOUCHPAD; k++)
        press(&v, (SDL_GameControllerButton)k, 1);
    SDL_GameControllerUpdate();
    CHECK(input_controller(pad_button, pad_axis, v.pad) == 0, "paddles, misc1, touchpad: $%04X",
          input_controller(pad_button, pad_axis, v.pad));
    frame(&v, &s, &b);
    CHECK(actions_held(ACTION_FFWD) == 1, "held again");
    binder_release(&s, &b);   /* the pad is unplugged while held */
    CHECK(actions_held(ACTION_FFWD) == 0 && releases[ACTION_FFWD] == 2,
          "unplugged: fast-forward ends");
    for (int k = SDL_CONTROLLER_BUTTON_PADDLE1; k <= SDL_CONTROLLER_BUTTON_TOUCHPAD; k++)
        press(&v, (SDL_GameControllerButton)k, 0);
    binder_init(&s);
    frame(&v, &s, &b);

    /* Rebinding from settings.ini lines. */
    reset_counts();
    CHECK(bindings_parse_line(&b, "bind_paddle1=screenshot\n"), "bind_paddle1 is ours");
    CHECK(bindings_parse_line(&b, "bind_touchpad=ffwd\r\n"), "bind_touchpad");
    CHECK(bindings_parse_line(&b, "bind_paddle3=none"), "bind_paddle3=none");
    CHECK(bindings_parse_line(&b, "bind_a=menu") && b.button[SDL_CONTROLLER_BUTTON_A] == 0,
          "A can't be bound (an SNES button)");
    CHECK(bindings_parse_line(&b, "bind_paddle2=jump") &&
              b.button[SDL_CONTROLLER_BUTTON_PADDLE2] == ACTION_QUICKSAVE,
          "an unknown action leaves the binding as it was");
    CHECK(!bindings_parse_line(&b, "preset=hd2d"), "other lines are not ours");
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE1, 1);
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE3, 1);
    press(&v, SDL_CONTROLLER_BUTTON_TOUCHPAD, 1);
    frame(&v, &s, &b);
    /* SDL before 2.32 doesn't report the touchpad button of a virtual pad (CI has 2.30):
     * the binding is still parsed above, but the press can't arrive, so expect it only
     * when SDL passes it on. */
    int tp_seen = SDL_GameControllerGetButton(v.pad, SDL_CONTROLLER_BUTTON_TOUCHPAD);
    CHECK(presses[ACTION_SCREENSHOT] == 1 && presses[ACTION_MENU] == 0 &&
              presses[ACTION_QUICKLOAD] == 0 && presses[ACTION_FFWD] == (tp_seen ? 1 : 0),
          "R4 screenshot, R5 nothing, touchpad fast-forward (shot %d menu %d qload %d ffwd %d, "
          "touchpad seen %d)",
          presses[ACTION_SCREENSHOT], presses[ACTION_MENU], presses[ACTION_QUICKLOAD],
          presses[ACTION_FFWD], tp_seen);
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE1, 0);
    press(&v, SDL_CONTROLLER_BUTTON_PADDLE3, 0);
    press(&v, SDL_CONTROLLER_BUTTON_TOUCHPAD, 0);
    frame(&v, &s, &b);
    CHECK(actions_held(ACTION_FFWD) == 0, "touchpad up: fast-forward ends");

    /* Keys: rebinding, taken keys, round trip through bindings_write. */
    CHECK(bindings_parse_line(&b, "key_quicksave=F7") && b.key[ACTION_QUICKSAVE] == SDL_SCANCODE_F7,
          "key_quicksave=F7");
    CHECK(bindings_parse_line(&b, "key_menu=Z") && b.key[ACTION_MENU] == SDL_SCANCODE_F10,
          "Z is the game's: refused");
    CHECK(bindings_parse_line(&b, "key_quickload=F7") && b.key[ACTION_QUICKLOAD] == SDL_SCANCODE_F7 &&
              b.key[ACTION_QUICKSAVE] == SDL_SCANCODE_UNKNOWN,
          "one action per key");
    CHECK(bindings_parse_line(&b, "key_screenshot=none") &&
              b.key[ACTION_SCREENSHOT] == SDL_SCANCODE_UNKNOWN, "key_screenshot=none");
    /* the quick menu action: F1 and misc1 by default, F1 only for it */
    {
        bindings q;
        bindings_defaults(&q);
        CHECK(action_parse("quickmenu") == ACTION_QUICKMENU &&
                  !strcmp(action_name(ACTION_QUICKMENU), "quickmenu"), "action quickmenu");
        CHECK(q.key[ACTION_QUICKMENU] == SDL_SCANCODE_F1, "key_quickmenu default F1");
        CHECK(bindings_for_key(&q, SDL_SCANCODE_F1) == ACTION_QUICKMENU, "F1 maps to the quick menu");
        CHECK(bindings_for_button(&q, SDL_CONTROLLER_BUTTON_MISC1) == ACTION_QUICKMENU,
              "misc1 maps to the quick menu");
        CHECK(bindings_button_of(&q, ACTION_QUICKMENU) == SDL_CONTROLLER_BUTTON_MISC1,
              "the quick menu's button is misc1");
        CHECK(bindings_parse_line(&q, "key_menu=F1") && q.key[ACTION_MENU] == SDL_SCANCODE_F10,
              "F1 is refused for other actions");
        CHECK(bindings_parse_line(&q, "key_quickmenu=F2") && q.key[ACTION_QUICKMENU] == SDL_SCANCODE_F1,
              "F2 stays the renderer's");
        CHECK(bindings_parse_line(&q, "key_quickmenu=F12") &&
                  q.key[ACTION_QUICKMENU] == SDL_SCANCODE_F12, "key_quickmenu=F12");
        CHECK(bindings_parse_line(&q, "key_quickmenu=F1") && q.key[ACTION_QUICKMENU] == SDL_SCANCODE_F1,
              "key_quickmenu=F1 allowed");
        CHECK(bindings_parse_line(&q, "bind_misc1=quickmenu") &&
                  q.button[SDL_CONTROLLER_BUTTON_MISC1] == ACTION_QUICKMENU, "bind_misc1=quickmenu");
        CHECK(bindings_parse_line(&q, "bind_paddle1=quickmenu") &&
                  bindings_for_button(&q, SDL_CONTROLLER_BUTTON_PADDLE1) == ACTION_QUICKMENU,
              "bind_paddle1=quickmenu");
        actions_emit(ACTION_QUICKMENU, 1);
        CHECK(presses[ACTION_QUICKMENU] == 1, "the quick menu action reaches its handler");
        reset_counts();
    }
    CHECK(!bindings_key_event(&b, SDL_SCANCODE_Z, 1, 0), "Z is game input");
    bindings_key_event(&b, SDL_SCANCODE_Z, 0, 0);
    FILE *f = tmpfile();
    bindings saved = b, loaded;
    bindings_write(&b, f);
    rewind(f);
    bindings_defaults(&loaded);
    char line[128];
    while (fgets(line, sizeof line, f))
        CHECK(bindings_parse_line(&loaded, line), "written line read back: %s", line);
    fclose(f);
    CHECK(!memcmp(&saved, &loaded, sizeof saved), "bindings survive settings.ini");
    detach(&v);
}

/* Glyph families from what SDL reports. Virtual pads all report
   SDL_CONTROLLER_TYPE_VIRTUAL, so the vendor / product decide. */
static void test_families(void)
{
    static const struct { uint16_t vid, pid; const char *name; int fam, deck; } want[] = {
        {0x28de, 0x1205, "Steam Deck", GLYPHS_STEAM_DECK, 0},
        {0x28de, 0x1102, "Steam Controller", GLYPHS_STEAM_CONTROLLER, 0},
        {0x28de, 0x1142, "Steam Controller", GLYPHS_STEAM_CONTROLLER, 0},
        {0x28de, 0x1302, "Steam Controller", GLYPHS_STEAM_DECK, 0},   /* 2026 model */
        {0x28de, 0x11ff, "Steam Virtual Gamepad", GLYPHS_XBOX, 0},
        {0x28de, 0x11ff, "Steam Virtual Gamepad", GLYPHS_STEAM_DECK, 1},   /* on a Deck */
        {0x045e, 0x0b12, "Xbox Series X Controller", GLYPHS_XBOX, 0},
        {0x054c, 0x0ce6, "DualSense Wireless Controller", GLYPHS_PLAYSTATION, 0},
        {0x057e, 0x2009, "Nintendo Switch Pro Controller", GLYPHS_SWITCH, 0},
        {0x057e, 0x0337, "Nintendo GameCube Controller Adapter", GLYPHS_GAMECUBE, 0},
        {0x0f0d, 0x00aa, "Pro Controller", GLYPHS_SWITCH, 0},   /* a third-party Pro pad */
        {0x1234, 0x5678, "Some Pad", GLYPHS_XBOX, 0},
    };
    for (size_t k = 0; k < sizeof want / sizeof *want; k++) {
        vpad v = attach(want[k].vid, want[k].pid, want[k].name);
        if (!v.pad)
            continue;
        int got = glyphs_family(SDL_GameControllerGetType(v.pad), SDL_GameControllerGetVendor(v.pad),
                                SDL_GameControllerGetProduct(v.pad), SDL_GameControllerName(v.pad),
                                want[k].deck);
        CHECK(got == want[k].fam, "%04x:%04x \"%s\"%s: %s, expected %s", want[k].vid, want[k].pid,
              want[k].name, want[k].deck ? " on a Deck" : "", glyphs_family_name(got),
              glyphs_family_name(want[k].fam));
        detach(&v);
    }
    /* Types decide when SDL knows them (real pads, Steam's virtual pad
       with SteamVirtualGamepadInfo). */
    CHECK(glyphs_family(SDL_CONTROLLER_TYPE_PS4, 0x28de, 0x11ff, "x", 1) == GLYPHS_PLAYSTATION,
          "Steam's virtual pad for a DualShock 4");
    CHECK(glyphs_family(SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR, 0, 0, "x", 0) ==
              GLYPHS_SWITCH, "Joy-Con pair");
    CHECK(glyphs_family(SDL_CONTROLLER_TYPE_XBOXONE, 0, 0, NULL, 0) == GLYPHS_XBOX, "Xbox One");
    CHECK(glyphs_family(SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO, 0x20d6, 0xa711, "Wired Controller",
                        0) == GLYPHS_SWITCH, "a Pro pad with a foreign ID: by SDL's type");
    CHECK(glyphs_family(SDL_CONTROLLER_TYPE_PS4, 0x054c, 0x09cc, "Pro Controller", 0) ==
              GLYPHS_PLAYSTATION, "PlayStation type beats the name");
    CHECK(glyphs_family(SDL_CONTROLLER_TYPE_XBOXONE, 0x045e, 0x02ea, "Xbox One Controller", 0) ==
              GLYPHS_XBOX, "an Xbox pad is not caught by the Pro rules");
    CHECK(glyphs_family(SDL_CONTROLLER_TYPE_PS5, 0x054c, 0x0ce6, "DualSense Wireless Controller",
                        0) == GLYPHS_PLAYSTATION, "DualSense by type");
    CHECK(glyphs_family(SDL_CONTROLLER_TYPE_UNKNOWN, 0, 0, "Steam Deck Controller", 0) ==
              GLYPHS_STEAM_DECK, "by name");
}

static void test_glyphs(const char *mask_png)
{
    const char *label;
    int have = glyphs_built_in() > 0;
    fprintf(stderr, "test_actions: %d glyph pictures built in\n", glyphs_built_in());
    bindings_defaults(bindings_current());

    const uint8_t *g = glyphs_lookup_in(GLYPHS_STEAM_DECK, "paddle2", &label);
    CHECK(!strcmp(label, "L4") && (!have || g), "Deck paddle2 is L4 (%s)", label);
    g = glyphs_lookup_in(GLYPHS_STEAM_DECK, "@menu", &label);
    CHECK(!strcmp(label, "R5") && (!have || g), "Deck menu action: R5 (%s)", label);
    g = glyphs_lookup_in(GLYPHS_STEAM_DECK, "start", &label);
    CHECK(!strcmp(label, "Menu"), "Deck start: Menu (%s)", label);
    g = glyphs_lookup_in(GLYPHS_GAMECUBE, "a", &label);
    CHECK(!g && !strcmp(label, "A"), "GameCube south: text A (%s)", label);
    g = glyphs_lookup_in(GLYPHS_GAMECUBE, "b", &label);
    CHECK(!g && !strcmp(label, "X"), "GameCube east: its X (%s)", label);
    g = glyphs_lookup_in(GLYPHS_GAMECUBE, "r1", &label);
    CHECK(!g && !strcmp(label, "Z"), "GameCube right shoulder: Z (%s)", label);
    CHECK(!strcmp(glyphs_family_name(GLYPHS_GAMECUBE), "GameCube"), "GameCube family name");
    g = glyphs_lookup_in(GLYPHS_XBOX, "paddle1", &label);
    CHECK(!g && !strcmp(label, "P1"), "Xbox paddle1: text P1 (%s)", label);
    g = glyphs_lookup_in(GLYPHS_SWITCH, "a", &label);
    CHECK(!strcmp(label, "B"), "Switch south button: B (%s)", label);
    g = glyphs_lookup_in(GLYPHS_PLAYSTATION, "b", &label);
    CHECK(!strcmp(label, "Circle"), "PlayStation east: Circle (%s)", label);
    g = glyphs_lookup_in(GLYPHS_STEAM_CONTROLLER, "paddle1", &label);
    CHECK(!strcmp(label, "R grip") && (!have || g), "Steam Controller paddle1: R grip (%s)", label);
    g = glyphs_lookup_in(GLYPHS_KEYBOARD, "@quicksave", &label);
    CHECK(!strcmp(label, "F6") && (!have || g), "keyboard quick save: F6 (%s)", label);
    g = glyphs_lookup_in(GLYPHS_KEYBOARD, "paddle4", &label);
    CHECK(!strcmp(label, "Tab") && (!have || g), "keyboard: L5's action's key, Tab (%s)", label);
    g = glyphs_lookup_in(GLYPHS_KEYBOARD, "a", &label);
    CHECK(!strcmp(label, "Z") && (!have || g), "keyboard: south = Z (%s)", label);
    g = glyphs_lookup_in(GLYPHS_KEYBOARD, "r3", &label);
    CHECK(!strcmp(label, "F1"), "keyboard: R3 = F1 (%s)", label);
    g = glyphs_lookup_in(GLYPHS_KEYBOARD, "key:Right Shift", &label);
    CHECK(!strcmp(label, "Right Shift") && (!have || g), "keyboard: Right Shift (%s)", label);
    g = glyphs_lookup_in(GLYPHS_KEYBOARD, "key:Keypad 5", &label);
    CHECK(!g && !strcmp(label, "Keypad 5"), "a key with no picture: its name (%s)", label);

    char plain[128];
    glyphs_use(GLYPHS_STEAM_DECK);
    osd_text_plain("Press [@quicksave] to save, [start] for the menu", plain, sizeof plain);
    CHECK(!strcmp(plain, "Press L4 to save, Menu for the menu"), "plain: \"%s\"", plain);
    glyphs_use(GLYPHS_KEYBOARD);
    osd_text_plain("Press [@quicksave] [x [y]", plain, sizeof plain);
    CHECK(!strcmp(plain, "Press F6 [x S"), "plain, keyboard, a bracket alone: \"%s\"", plain);

    /* Masks: text 6 units a character, a glyph OSD_GLYPH_UNITS. */
    uint8_t *m;
    int w, h;
    CHECK(!osd_text_mask("AB", &m, &w, &h), "mask");
    CHECK(w == (2 * 6 + 3) * OSD_OS && h == OSD_UNITS_H * OSD_OS, "AB: %dx%d", w, h);
    CHECK(m[(2 * OSD_OS) * w + 2 * OSD_OS] == 0 && m[(2 * OSD_OS) * w + 3 * OSD_OS] == 255,
          "A's top row: .###.");
    free(m);
    glyphs_use(GLYPHS_STEAM_DECK);
    CHECK(!osd_text_mask("[a]", &m, &w, &h), "mask with a glyph");
    CHECK(w == ((have ? OSD_GLYPH_UNITS : 6) + 3) * OSD_OS, "one glyph: %d texels", w);
    long sum = 0;
    for (int k = 0; k < w * h; k++)
        sum += m[k];
    CHECK(sum > 0, "the glyph drew something");
    free(m);
    const char *sample = "Quit? [a] quit  [b] back  [@quicksave] save  [@ffwd] fast  [r3] look";
    CHECK(!osd_text_mask(sample, &m, &w, &h), "long mask");
    CHECK(w <= OSD_MAX_TEXELS, "width capped: %d", w);
    if (mask_png) {   /* to look at: white on black */
        uint8_t *px = malloc((size_t)w * h * 4);
        for (int k = 0; px && k < w * h; k++)
            px[4 * k] = px[4 * k + 1] = px[4 * k + 2] = m[k], px[4 * k + 3] = 255;
        CHECK(px && !png_write_bgrx(mask_png, px, w, h), "write %s", mask_png);
        free(px);
    }
    free(m);
    glyphs_use(GLYPHS_KEYBOARD);
}

static void test_stubs(void)
{
    /* No handler: the stub says so; a handler replaces it; NULL restores it. */
    last_msg[0] = 0;
    actions_set_message(message);
    actions_set_handler(ACTION_QUICKSAVE, NULL, NULL);
    actions_emit(ACTION_QUICKSAVE, 1);
    CHECK(!strcmp(last_msg, "Quick save: not available yet"), "stub message: \"%s\"", last_msg);
    reset_counts();
    actions_set_handler(ACTION_QUICKSAVE, handler, NULL);
    last_msg[0] = 0;
    actions_emit(ACTION_QUICKSAVE, 1);
    CHECK(presses[ACTION_QUICKSAVE] == 1 && !last_msg[0], "the handler, not the stub");
    actions_emit(ACTION_NONE, 1);
    actions_emit((ct_action)99, 1);
    CHECK(action_parse("FFWD") == ACTION_FFWD && action_parse("jump") == -1, "action_parse");
    CHECK(!strcmp(action_name(ACTION_SCREENSHOT), "screenshot"), "action_name");
    actions_emit(ACTION_FFWD, 1);
    actions_release_all();
    CHECK(actions_held(ACTION_FFWD) == 0, "release all");
}

static void test_gyro(void)
{
    gyro_cam gc = {0, 0};
    const float still[3] = {0.01f, -0.02f, 0.5f};   /* noise, and roll (unused) */
    for (int k = 0; k < 60; k++)
        gyro_cam_step(&gc, still, 1 / 60.0f, 1.0f);
    CHECK(gc.tilt == 0.0f && gc.yaw == 0.0f, "a still pad: no movement (%g, %g)", gc.tilt, gc.yaw);
    const float turn[3] = {0.5f, -0.5f, 0.0f};   /* rad/s */
    for (int k = 0; k < 12; k++)
        gyro_cam_step(&gc, turn, 1 / 60.0f, 1.0f);
    CHECK(gc.tilt > 1.0f && gc.yaw < -1.0f, "turning tilts and pans (%g, %g)", gc.tilt, gc.yaw);
    for (int k = 0; k < 600; k++)
        gyro_cam_step(&gc, turn, 1 / 60.0f, 4.0f);
    CHECK(gc.tilt <= GYRO_MAX_DEG && gc.yaw >= -GYRO_MAX_DEG, "capped (%g, %g)", gc.tilt, gc.yaw);
    for (int k = 0; k < 300; k++)
        gyro_cam_step(&gc, still, 1 / 60.0f, 1.0f);
    CHECK(gc.tilt < 0.01f && gc.yaw > -0.01f, "eases back (%g, %g)", gc.tilt, gc.yaw);
}

/* port: the layout swaps and the stick's dead zone (input.h), through a
   virtual pad: only the face buttons, shoulders, d-pad and the left stick
   (SDL 2.30's virtual pads have no touchpad / misc buttons to count on). */
static uint16_t snes(vpad *v)
{
    SDL_GameControllerUpdate();
    return input_controller(pad_button, pad_axis, v->pad);
}

static void tilt(vpad *v, int x, int y)
{
    SDL_JoystickSetVirtualAxis(v->joy, SDL_CONTROLLER_AXIS_LEFTX, (Sint16)x);
    SDL_JoystickSetVirtualAxis(v->joy, SDL_CONTROLLER_AXIS_LEFTY, (Sint16)y);
}

static void test_swap_deadzone(void)
{
    vpad v = attach(0x045e, 0x028e, "Xbox 360");
    if (!v.pad)
        return;
    input_swap_set(0, 0);
    input_deadzone_set(INPUT_DEADZONE_DEFAULT);
    press(&v, SDL_CONTROLLER_BUTTON_A, 1);   /* south: SNES B */
    CHECK(snes(&v) == PAD_B, "south is B: %04x", snes(&v));
    press(&v, SDL_CONTROLLER_BUTTON_A, 0);
    press(&v, SDL_CONTROLLER_BUTTON_B, 1);   /* east: SNES A */
    CHECK(snes(&v) == PAD_A, "east is A: %04x", snes(&v));
    press(&v, SDL_CONTROLLER_BUTTON_B, 0);

    input_swap_set(1, 0);
    press(&v, SDL_CONTROLLER_BUTTON_A, 1);
    CHECK(snes(&v) == PAD_A, "swapped: south is A: %04x", snes(&v));
    press(&v, SDL_CONTROLLER_BUTTON_B, 1);
    CHECK(snes(&v) == (PAD_A | PAD_B), "both held stay both: %04x", snes(&v));
    press(&v, SDL_CONTROLLER_BUTTON_A, 0);
    CHECK(snes(&v) == PAD_B, "swapped: east is B: %04x", snes(&v));
    press(&v, SDL_CONTROLLER_BUTTON_B, 0);
    press(&v, SDL_CONTROLLER_BUTTON_X, 1);   /* west: SNES Y, X untouched by the A/B swap */
    CHECK(snes(&v) == PAD_Y, "west is still Y: %04x", snes(&v));
    press(&v, SDL_CONTROLLER_BUTTON_X, 0);

    input_swap_set(0, 1);
    press(&v, SDL_CONTROLLER_BUTTON_X, 1);
    CHECK(snes(&v) == PAD_X, "X/Y swapped: west is X: %04x", snes(&v));
    press(&v, SDL_CONTROLLER_BUTTON_Y, 1);
    CHECK(snes(&v) == (PAD_X | PAD_Y), "both: %04x", snes(&v));
    press(&v, SDL_CONTROLLER_BUTTON_X, 0);
    CHECK(snes(&v) == PAD_Y, "X/Y swapped: north is Y: %04x", snes(&v));
    press(&v, SDL_CONTROLLER_BUTTON_Y, 0);
    press(&v, SDL_CONTROLLER_BUTTON_A, 1);
    press(&v, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 1);
    press(&v, SDL_CONTROLLER_BUTTON_DPAD_UP, 1);
    CHECK(snes(&v) == (PAD_B | PAD_L | PAD_UP), "other buttons unaffected: %04x", snes(&v));
    press(&v, SDL_CONTROLLER_BUTTON_A, 0);
    press(&v, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0);
    press(&v, SDL_CONTROLLER_BUTTON_DPAD_UP, 0);

    /* The keyboard doesn't swap. */
    uint8_t keys[SDL_NUM_SCANCODES] = {0};
    keys[SDL_SCANCODE_X] = 1;   /* A */
    input_swap_set(1, 1);
    CHECK(input_keyboard(keys) == PAD_A, "keyboard X is A with swaps on");
    keys[SDL_SCANCODE_X] = 0;
    keys[SDL_SCANCODE_S] = 1;   /* X */
    CHECK(input_keyboard(keys) == PAD_X, "keyboard S is X with swaps on");
    input_swap_set(0, 0);

    /* Dead zone: the default is the old 16000 of 32767. */
    CHECK(input_deadzone_get() == INPUT_DEADZONE_DEFAULT, "default deadzone");
    tilt(&v, 15900, 0);
    CHECK(snes(&v) == 0, "15900 is inside the default zone: %04x", snes(&v));
    tilt(&v, 16100, 0);
    CHECK(snes(&v) == PAD_RIGHT, "16100 is outside it: %04x", snes(&v));
    tilt(&v, -16100, -16100);
    CHECK(snes(&v) == (PAD_LEFT | PAD_UP), "diagonal: %04x", snes(&v));
    input_deadzone_set(20);   /* 6553 */
    tilt(&v, 0, 8000);
    CHECK(snes(&v) == PAD_DOWN, "8000 passes a 20%% zone: %04x", snes(&v));
    tilt(&v, 0, 5000);
    CHECK(snes(&v) == 0, "5000 doesn't: %04x", snes(&v));
    input_deadzone_set(80);   /* 26213 */
    tilt(&v, 20000, 0);
    CHECK(snes(&v) == 0, "20000 doesn't pass an 80%% zone: %04x", snes(&v));
    tilt(&v, 30000, 0);
    CHECK(snes(&v) == PAD_RIGHT, "30000 does: %04x", snes(&v));
    input_deadzone_set(0);
    CHECK(input_deadzone_get() == INPUT_DEADZONE_MIN, "zone clamped up");
    input_deadzone_set(INPUT_DEADZONE_DEFAULT);
    tilt(&v, 0, 0);

    /* The cheat-sheet follows the swaps. */
    bindings b;
    bindings_defaults(&b);
    cheat_row rows[24];
    int n = bindings_cheatsheet(&b, rows, 24);
    CHECK(n == 9 + ACTION_COUNT - 1, "rows %d", n);
    CHECK(!strcmp(rows[0].what, "A") && !strcmp(rows[0].pad, "[b]") && !strcmp(rows[0].key, "X"),
          "A: %s %s", rows[0].pad, rows[0].key);
    CHECK(!strcmp(rows[1].what, "B") && !strcmp(rows[1].pad, "[a]"), "B: %s", rows[1].pad);
    input_swap_set(1, 1);
    bindings_cheatsheet(&b, rows, 24);
    CHECK(!strcmp(rows[0].pad, "[a]") && !strcmp(rows[1].pad, "[b]"), "swapped A/B: %s %s",
          rows[0].pad, rows[1].pad);
    CHECK(!strcmp(rows[2].pad, "[x]") && !strcmp(rows[3].pad, "[y]"), "swapped X/Y: %s %s",
          rows[2].pad, rows[3].pad);
    input_swap_set(0, 0);
    int found = 0;
    for (int k = 9; k < n; k++)
        if (!strcmp(rows[k].what, action_title(ACTION_QUICKSAVE)))
            found = !strcmp(rows[k].pad, "[paddle2]") && !strcmp(rows[k].key, "F6");
    CHECK(found, "quick save row: paddle2 and F6");
    CHECK(bindings_cheatsheet(&b, rows, 3) == 3, "max respected");
    detach(&v);
}

/* Player index and light colour (input_pad_extras_apply) on a virtual pad
   that records what SDL passes to its driver. */
static struct { int index; int led_calls; Uint8 rgb[3]; } rec_pad;
static void SDLCALL rec_index(void *u, int i) { (void)u; rec_pad.index = i; }
static int SDLCALL rec_led(void *u, Uint8 r, Uint8 g, Uint8 b)
{
    (void)u;
    rec_pad.led_calls++;
    rec_pad.rgb[0] = r, rec_pad.rgb[1] = g, rec_pad.rgb[2] = b;
    return 0;
}

static void test_pad_extras(void)
{
    SDL_VirtualJoystickDesc d;
    memset(&d, 0, sizeof d);
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    d.naxes = SDL_CONTROLLER_AXIS_MAX;
    d.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    d.vendor_id = 0x054c;
    d.product_id = 0x0ce6;
    d.name = "DualSense";
    d.SetPlayerIndex = rec_index;
    d.SetLED = rec_led;
    int idx = SDL_JoystickAttachVirtualEx(&d);
    CHECK(idx >= 0, "attach lit pad: %s", SDL_GetError());
    SDL_GameController *pad = idx >= 0 ? SDL_GameControllerOpen(idx) : NULL;
    CHECK(pad != NULL, "open lit pad");
    if (!pad)
        return;
    CHECK(SDL_GameControllerHasLED(pad), "the virtual pad has a light");
    input_register_settings();
    rec_pad.index = -9;
    input_pad_extras_apply(pad, 1);
    CHECK(rec_pad.index == 1 && rec_pad.led_calls == 1 && rec_pad.rgb[0] == 255 &&
              rec_pad.rgb[2] < 64,
          "player 2: index %d, light %d,%d,%d (red)", rec_pad.index, rec_pad.rgb[0], rec_pad.rgb[1],
          rec_pad.rgb[2]);
    input_pad_extras_apply(pad, 0);
    CHECK(rec_pad.index == 0 && rec_pad.rgb[2] == 255 && rec_pad.rgb[0] < 64, "player 1: blue");
    CHECK(settings_parse_line("pad_leds=off\n") == 1 && !input_pad_leds(), "pad_leds=off read");
    rec_pad.index = -9;
    rec_pad.led_calls = 0;
    input_pad_extras_apply(pad, 2);
    CHECK(rec_pad.index == -9 && rec_pad.led_calls == 0, "pad_leds=off: nothing sent");
    settings_parse_line("pad_leds=auto\n");
    CHECK(input_pad_leds() == 1, "pad_leds=auto back on");
    input_pad_extras_apply(NULL, 0);   /* no pad: no crash */
    SDL_GameControllerClose(pad);
    SDL_JoystickDetachVirtual(idx);
}

int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_GAMECONTROLLER)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_version v;
    SDL_GetVersion(&v);
    fprintf(stderr, "test_actions: SDL %d.%d.%d\n", v.major, v.minor, v.patch);
    for (int a = 1; a < ACTION_COUNT; a++)
        actions_set_handler((ct_action)a, handler, NULL);
    test_deck();
    test_families();
    test_glyphs(argc > 1 ? argv[1] : NULL);
    test_stubs();
    test_gyro();
    test_swap_deadzone();
    test_pad_extras();
    SDL_Quit();
    printf("test_actions: %ld checks, %ld failures\n", checks, fails);
    return fails ? 1 : 0;
}
