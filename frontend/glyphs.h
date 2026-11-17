/* SPDX-FileCopyrightText: 2026 jerbanpls
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * port: button glyphs for on-screen messages: which controller family is
 * in use, and the picture (Kenney Input Prompts, CC0, third_party/glyphs/)
 * for a button of it.
 *
 * Messages name buttons with tokens in brackets (osd_text.h):
 *   [a] [b] [x] [y]        the face buttons by position (south, east,
 *                          west, north: SDL's A, B, X, Y)
 *   [back] [start] [guide] View / Menu / the logo button
 *   [l1] [r1] [l2] [r2] [l3] [r3] [dpad]
 *   [paddle1]..[paddle4] [misc1] [touchpad]
 *   [key:NAME]             a keyboard key (SDL's key name: F6, Tab, ...)
 *   [@ACTION]              whatever triggers an action (actions.h) on the
 *                          device in use: its key, or its bound button
 * With the keyboard in use, a controller token shows the key that does
 * the same (south = Z, Start = Enter, R3 = F1, ...). A button with no
 * picture in the set (an Xbox pad's paddles) is written as text. */
#ifndef CT_GLYPHS_H
#define CT_GLYPHS_H

#include <stdint.h>

#include <SDL.h>

enum {
    GLYPHS_KEYBOARD,
    GLYPHS_XBOX,
    GLYPHS_PLAYSTATION,
    GLYPHS_SWITCH,
    GLYPHS_STEAM_DECK,
    GLYPHS_STEAM_CONTROLLER,
    GLYPHS_GAMECUBE,   /* Nintendo's GameCube adapter: text labels only */
    GLYPHS_SETS
};

/* The pictures' size: GLYPH_PX x GLYPH_PX coverage bytes. */
#define GLYPH_PX 44

/* The family of a controller from what SDL says of it: its type
   (SDL_GameControllerType), USB vendor / product and name. A Steam
   Input virtual pad (Valve 28de:11ff, which reports the real pad's type
   when Steam tells SDL) is the Deck's built-in controller when on_deck;
   otherwise an Xbox-style pad unless its type says else. The GameCube
   adapter (057e:0337) has its own family; a Switch Pro pad is known by
   SDL's type or its "Pro Controller" name as well as by Nintendo's
   vendor ID, since third-party ones report other IDs. */
int glyphs_family(int type, uint16_t vendor, uint16_t product, const char *name, int on_deck);
int glyphs_family_of(SDL_GameController *pad);   /* the same, from an open pad */
const char *glyphs_family_name(int set);         /* "Steam Deck", "Xbox", ... */

/* The set the messages use: the device last used. */
void glyphs_use(int set);
int glyphs_in_use(void);

/* A token's picture in the set in use: GLYPH_PX x GLYPH_PX bytes, or
   NULL (no picture: write *label instead). *label is the button's name
   on that device ("R4", "Menu", "F6", ...), never NULL. */
const uint8_t *glyphs_lookup(const char *token, const char **label);
/* As glyphs_lookup, for a given set. */
const uint8_t *glyphs_lookup_in(int set, const char *token, const char **label);

/* How many pictures were built in (0 without tools/embed_glyphs.py: the
   Android build); then every token is text. */
int glyphs_built_in(void);

#endif
