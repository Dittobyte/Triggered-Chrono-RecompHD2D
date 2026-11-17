/* SPDX-FileCopyrightText: 2026 jerbanpls
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * port: button glyphs (glyphs.h). The pictures come from
 * tools/embed_glyphs.py (CT_GLYPHS_DATA: glyphs_data.h); without it every
 * token is its label. */
#include "glyphs.h"

#include <stdio.h>
#include <string.h>

#include "bindings.h"
#include "device.h"

#ifdef CT_GLYPHS_DATA
#include "glyphs_data.h"
#if GLYPH_DATA_PX != GLYPH_PX
#error "glyphs_data.h was built for another size"
#endif
#endif

/* The tokens for controller buttons, in this order in the tables below. */
enum {
    T_A, T_B, T_X, T_Y, T_BACK, T_START, T_GUIDE, T_L1, T_R1, T_L2, T_R2, T_L3, T_R3, T_DPAD,
    T_PADDLE1, T_PADDLE2, T_PADDLE3, T_PADDLE4, T_MISC1, T_TOUCHPAD, T_COUNT
};
static const char *const token_name[T_COUNT] = {
    "a", "b", "x", "y", "back", "start", "guide", "l1", "r1", "l2", "r2", "l3", "r3", "dpad",
    "paddle1", "paddle2", "paddle3", "paddle4", "misc1", "touchpad",
};
static const SDL_GameControllerButton token_button[T_COUNT] = {
    SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_GUIDE, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_INVALID,
    SDL_CONTROLLER_BUTTON_INVALID, SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_PADDLE1, SDL_CONTROLLER_BUTTON_PADDLE2, SDL_CONTROLLER_BUTTON_PADDLE3,
    SDL_CONTROLLER_BUTTON_PADDLE4, SDL_CONTROLLER_BUTTON_MISC1, SDL_CONTROLLER_BUTTON_TOUCHPAD,
};

typedef struct {
    const char *file;    /* picture name (third_party/glyphs/<file>.png), or NULL */
    const char *label;
} glyph_def;

/* Controller sets (index GLYPHS_XBOX.. - 1). */
/* (The PlayStation west button is "Sq": the full word is on check_agnostic's list.) */
static const glyph_def pads[GLYPHS_SETS - 1][T_COUNT] = {
    [GLYPHS_XBOX - 1] = {
        {"xbox_button_a", "A"}, {"xbox_button_b", "B"}, {"xbox_button_x", "X"},
        {"xbox_button_y", "Y"}, {"xbox_button_view", "View"}, {"xbox_button_menu", "Menu"},
        {"xbox_guide", "Guide"}, {"xbox_lb", "LB"}, {"xbox_rb", "RB"}, {"xbox_lt", "LT"},
        {"xbox_rt", "RT"}, {"xbox_stick_l_press", "LS"}, {"xbox_stick_r_press", "RS"},
        {"xbox_dpad", "D-pad"}, {NULL, "P1"}, {NULL, "P3"}, {NULL, "P2"}, {NULL, "P4"},
        {NULL, "Share"}, {NULL, "Touchpad"},
    },
    [GLYPHS_PLAYSTATION - 1] = {
        {"playstation_button_cross", "Cross"}, {"playstation_button_circle", "Circle"},
        {"playstation_button_square", "Sq"}, {"playstation_button_triangle", "Triangle"},
        {"playstation5_button_create", "Create"}, {"playstation5_button_options", "Options"},
        {NULL, "PS"}, {"playstation_trigger_l1", "L1"}, {"playstation_trigger_r1", "R1"},
        {"playstation_trigger_l2", "L2"}, {"playstation_trigger_r2", "R2"},
        {"playstation_button_l3", "L3"}, {"playstation_button_r3", "R3"},
        {"playstation_dpad", "D-pad"}, {NULL, "P1"}, {NULL, "P2"}, {NULL, "P3"}, {NULL, "P4"},
        {NULL, "Mute"}, {"playstation5_touchpad", "Touchpad"},
    },
    [GLYPHS_SWITCH - 1] = {   /* by position: south is Nintendo's B */
        {"switch_button_b", "B"}, {"switch_button_a", "A"}, {"switch_button_y", "Y"},
        {"switch_button_x", "X"}, {"switch_button_minus", "-"}, {"switch_button_plus", "+"},
        {"switch_button_home", "Home"}, {"switch_button_l", "L"}, {"switch_button_r", "R"},
        {"switch_button_zl", "ZL"}, {"switch_button_zr", "ZR"},
        {"switch_stick_l_press", "L3"}, {"switch_stick_r_press", "R3"},
        {"switch_dpad", "D-pad"}, {NULL, "P1"}, {NULL, "P2"}, {NULL, "P3"}, {NULL, "P4"},
        {NULL, "Capture"}, {NULL, "Touchpad"},
    },
    [GLYPHS_STEAM_DECK - 1] = {
        {"steamdeck_button_a", "A"}, {"steamdeck_button_b", "B"}, {"steamdeck_button_x", "X"},
        {"steamdeck_button_y", "Y"}, {"steamdeck_button_view", "View"},
        {"steamdeck_button_options", "Menu"}, {"steamdeck_button_guide", "Steam"},
        {"steamdeck_button_l1", "L1"}, {"steamdeck_button_r1", "R1"},
        {"steamdeck_button_l2", "L2"}, {"steamdeck_button_r2", "R2"},
        {"steamdeck_stick_l_press", "L3"}, {"steamdeck_stick_r_press", "R3"},
        {"steamdeck_dpad", "D-pad"},
        {"steamdeck_button_r4", "R4"}, {"steamdeck_button_l4", "L4"},   /* SDL_hidapi_steamdeck.c */
        {"steamdeck_button_r5", "R5"}, {"steamdeck_button_l5", "L5"},
        {"steamdeck_button_quickaccess", "..."}, {"steamdeck_trackpad", "Trackpad"},
    },
    [GLYPHS_STEAM_CONTROLLER - 1] = {
        {"steam_button_a", "A"}, {"steam_button_b", "B"}, {"steam_button_x", "X"},
        {"steam_button_y", "Y"}, {"steam_button_back_icon", "Back"},
        {"steam_button_start_icon", "Start"}, {NULL, "Steam"}, {"steam_lb", "LB"},
        {"steam_rb", "RB"}, {"steam_lt", "LT"}, {"steam_rt", "RT"},
        {"steam_stick_l_press", "L3"}, {"steam_button_rp", "R pad"}, {"steam_dpad", "D-pad"},
        {"steam_rg", "R grip"}, {"steam_lg", "L grip"}, {NULL, "P3"}, {NULL, "P4"},
        {NULL, "Misc"}, {NULL, "Pad"},
    },
    /* The GameCube adapter, by position (SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS=0:
       SDL_hidapi_gamecube.c, release-2.32.10, swaps its B and X then): south is the
       pad's A, east its X, west its B, north its Y; its Z is SDL's right shoulder,
       its L and R are the triggers, and it has no Back, Guide or left shoulder button.
       No pictures: text only. */
    [GLYPHS_GAMECUBE - 1] = {
        {NULL, "A"}, {NULL, "X"}, {NULL, "B"}, {NULL, "Y"}, {NULL, "Back"}, {NULL, "Start"},
        {NULL, "Guide"}, {NULL, "L1"}, {NULL, "Z"}, {NULL, "L"}, {NULL, "R"}, {NULL, "L3"},
        {NULL, "R3"}, {NULL, "D-pad"}, {NULL, "P1"}, {NULL, "P2"}, {NULL, "P3"}, {NULL, "P4"},
        {NULL, "Misc"}, {NULL, "Touchpad"},
    },
};

/* With the keyboard in use: the key doing what a controller button does
   (input.c's map, main.c's R3 / L3); NULL: through its action, or none. */
static const char *const keyboard_for[T_COUNT] = {
    [T_A] = "Z", [T_B] = "X", [T_X] = "A", [T_Y] = "S", [T_BACK] = "Right Shift",
    [T_START] = "Return", [T_L1] = "Q", [T_R1] = "W", [T_L3] = "F3", [T_R3] = "F1",
    [T_DPAD] = "Arrows",
};

static const char *const family_names[GLYPHS_SETS] = {
    "keyboard", "Xbox", "PlayStation", "Nintendo Switch", "Steam Deck", "Steam Controller",
    "GameCube",
};

int glyphs_family(int type, uint16_t vendor, uint16_t product, const char *name, int on_deck)
{
    /* Nintendo's GameCube adapter (USB_PRODUCT_NINTENDO_GAMECUBE_ADAPTER 0x0337,
       SDL src/joystick/usb_ids.h, release-2.30.0 and 2.32.10): before the Switch fallback */
    if (vendor == 0x057e && product == 0x0337)
        return GLYPHS_GAMECUBE;
    if (vendor == 0x28de) {   /* Valve; product IDs from SDL's controller_list.h */
        switch (product) {
        case 0x1205:   /* the Steam Deck's built-in controller */
            return GLYPHS_STEAM_DECK;
        case 0x12f0: case 0x12fa: case 0x12fb: case 0x12fc: case 0x12fd: case 0x12fe:
        case 0x12ff:
            /* InputPlumber's emulated Deck controller on other handhelds
               (Ally 12fd, Legion Go 12fe, ...: hid_deck.h): the Deck's
               layout, A / B / X / Y where an Xbox pad has them, and L4 /
               R4 / L5 / R5 for the back buttons it maps */
            return GLYPHS_STEAM_DECK;
        case 0x1302: case 0x1303: case 0x1304: case 0x1305:
            /* the 2026 Steam Controller (and its dongles): the Deck's
               layout, L4 / L5 / R4 / R5 included */
            return GLYPHS_STEAM_DECK;
        case 0x1101: case 0x1102: case 0x1105: case 0x1106: case 0x1142: case 0x1201:
        case 0x1202:
            return GLYPHS_STEAM_CONTROLLER;
        default:
            break;
        }
    }
    switch (type) {
    case SDL_CONTROLLER_TYPE_PS3:
    case SDL_CONTROLLER_TYPE_PS4:
    case SDL_CONTROLLER_TYPE_PS5:
        return GLYPHS_PLAYSTATION;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return GLYPHS_SWITCH;
    default:
        break;
    }
    if (vendor == 0x28de && product == 0x11ff)   /* Steam Input's virtual pad */
        return on_deck ? GLYPHS_STEAM_DECK : GLYPHS_XBOX;
    if (name && strstr(name, "Steam Deck"))
        return GLYPHS_STEAM_DECK;
    if (name && strstr(name, "Steam Controller"))
        return GLYPHS_STEAM_CONTROLLER;
    if (type == SDL_CONTROLLER_TYPE_XBOX360 || type == SDL_CONTROLLER_TYPE_XBOXONE)
        return GLYPHS_XBOX;
    /* A third-party Switch Pro pad: another vendor ID, the Pro's generic name */
    if (name && strstr(name, "Pro Controller"))
        return GLYPHS_SWITCH;
    if (vendor == 0x054c)   /* Sony */
        return GLYPHS_PLAYSTATION;
    if (vendor == 0x057e)   /* Nintendo */
        return GLYPHS_SWITCH;
    return GLYPHS_XBOX;
}

int glyphs_family_of(SDL_GameController *pad)
{
    if (!pad)
        return GLYPHS_KEYBOARD;
    return glyphs_family(SDL_GameControllerGetType(pad), SDL_GameControllerGetVendor(pad),
                         SDL_GameControllerGetProduct(pad), SDL_GameControllerName(pad),
                         device_kind() == DEVICE_STEAM_DECK);
}

const char *glyphs_family_name(int set)
{
    return set >= 0 && set < GLYPHS_SETS ? family_names[set] : "?";
}

static int in_use = GLYPHS_KEYBOARD;

void glyphs_use(int set)
{
    if (set >= 0 && set < GLYPHS_SETS)
        in_use = set;
}

int glyphs_in_use(void)
{
    return in_use;
}

int glyphs_built_in(void)
{
#ifdef CT_GLYPHS_DATA
    return GLYPH_DATA_COUNT;
#else
    return 0;
#endif
}

static const uint8_t *picture(const char *file)
{
#ifdef CT_GLYPHS_DATA
    if (!file)
        return NULL;
    int lo = 0, hi = GLYPH_DATA_COUNT - 1;   /* sorted by name */
    while (lo <= hi) {
        int mid = (lo + hi) / 2, c = strcmp(file, glyph_data[mid].name);
        if (!c)
            return glyph_data[mid].px;
        if (c < 0)
            hi = mid - 1;
        else
            lo = mid + 1;
    }
#else
    (void)file;
#endif
    return NULL;
}

/* A key by SDL name: its picture and label. */
static const uint8_t *key_glyph(const char *key, const char **label)
{
    static char lab[32];
    static const struct { const char *sdl, *file, *label; } named[] = {
        {"Return", "keyboard_return", "Enter"}, {"Keypad Enter", "keyboard_enter", "Enter"},
        {"Right Shift", "keyboard_shift", "Right Shift"}, {"Left Shift", "keyboard_shift", "Shift"},
        {"Left Ctrl", "keyboard_ctrl", "Ctrl"}, {"Right Ctrl", "keyboard_ctrl", "Ctrl"},
        {"Left Alt", "keyboard_alt", "Alt"}, {"Right Alt", "keyboard_alt", "Alt"},
        {"Tab", "keyboard_tab", "Tab"}, {"Space", "keyboard_space", "Space"},
        {"Escape", "keyboard_escape", "Esc"}, {"Backspace", "keyboard_backspace", "Backspace"},
        {"Insert", "keyboard_insert", "Insert"}, {"Delete", "keyboard_delete", "Delete"},
        {"Home", "keyboard_home", "Home"}, {"End", "keyboard_end", "End"},
        {"PageUp", "keyboard_page_up", "Page Up"}, {"PageDown", "keyboard_page_down", "Page Down"},
        {"Up", "keyboard_arrow_up", "Up"}, {"Down", "keyboard_arrow_down", "Down"},
        {"Left", "keyboard_arrow_left", "Left"}, {"Right", "keyboard_arrow_right", "Right"},
        {"Arrows", "keyboard_arrows", "Arrows"},
    };
    for (size_t k = 0; k < sizeof named / sizeof *named; k++)
        if (!SDL_strcasecmp(key, named[k].sdl)) {
            *label = named[k].label;
            return picture(named[k].file);
        }
    snprintf(lab, sizeof lab, "%s", key);
    *label = lab;
    /* Letters, digits, F1-F12: keyboard_<lowercase name>. */
    size_t n = strlen(key);
    int simple = n == 1 && ((key[0] >= 'A' && key[0] <= 'Z') || (key[0] >= 'a' && key[0] <= 'z') ||
                            (key[0] >= '0' && key[0] <= '9'));
    int fkey = (key[0] == 'F' || key[0] == 'f') && n >= 2 && n <= 3 && key[1] >= '1' &&
               key[1] <= '9' && (n == 2 || (key[2] >= '0' && key[2] <= '2' && key[1] == '1'));
    if (!simple && !fkey)
        return NULL;
    char file[24] = "keyboard_";
    for (size_t k = 0; k < n; k++)
        file[9 + k] = (char)(key[k] >= 'A' && key[k] <= 'Z' ? key[k] - 'A' + 'a' : key[k]);
    file[9 + n] = 0;
    return picture(file);
}

static const uint8_t *button_glyph(int set, int t, const char **label)
{
    if (set == GLYPHS_KEYBOARD) {
        if (keyboard_for[t])
            return key_glyph(keyboard_for[t], label);
        /* A bound button: the key of its action. */
        ct_action a = token_button[t] == SDL_CONTROLLER_BUTTON_INVALID
                          ? ACTION_NONE
                          : bindings_for_button(bindings_current(), token_button[t]);
        SDL_Scancode k = a != ACTION_NONE ? bindings_current()->key[a] : SDL_SCANCODE_UNKNOWN;
        if (k != SDL_SCANCODE_UNKNOWN)
            return key_glyph(SDL_GetScancodeName(k), label);
        *label = pads[GLYPHS_XBOX - 1][t].label;
        return NULL;
    }
    const glyph_def *d = &pads[set - 1][t];
    *label = d->label;
    return picture(d->file);
}

const uint8_t *glyphs_lookup_in(int set, const char *token, const char **label)
{
    static char unknown[40];
    if (set < 0 || set >= GLYPHS_SETS)
        set = GLYPHS_KEYBOARD;
    if (!SDL_strncasecmp(token, "key:", 4))
        return key_glyph(token + 4, label);
    if (token[0] == '@') {
        int a = action_parse(token + 1);
        if (a > ACTION_NONE) {
            const bindings *b = bindings_current();
            if (set == GLYPHS_KEYBOARD) {
                if (b->key[a] != SDL_SCANCODE_UNKNOWN)
                    return key_glyph(SDL_GetScancodeName(b->key[a]), label);
            } else {
                SDL_GameControllerButton btn = bindings_button_of(b, (ct_action)a);
                for (int t = 0; t < T_COUNT; t++)
                    if (btn != SDL_CONTROLLER_BUTTON_INVALID && token_button[t] == btn)
                        return button_glyph(set, t, label);
            }
            *label = action_title((ct_action)a);   /* not bound on this device */
            return NULL;
        }
    }
    for (int t = 0; t < T_COUNT; t++)
        if (!SDL_strcasecmp(token, token_name[t]))
            return button_glyph(set, t, label);
    snprintf(unknown, sizeof unknown, "%s", token);
    *label = unknown;
    return NULL;
}

const uint8_t *glyphs_lookup(const char *token, const char **label)
{
    return glyphs_lookup_in(in_use, token, label);
}
