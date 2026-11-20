/* SPDX-FileCopyrightText: 2026 jerbanpls
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * port: the F1 quick menu (quickmenu.h): a bottom sheet of big tiles drawn
 * with Dear ImGui's draw lists over the paused picture. Layout, focus,
 * scrolling and the pointer are ours (as menu.cpp's navigation is);
 * ImGui only draws. Everything a tile changes is a settings registry row
 * (settings.h) or an action (actions.h), so F10 and settings.ini agree. */
#include "quickmenu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "actions.h"
#include "glyphs.h"
#include "menu.h"
#include "settings.h"
#include "timectl.h"
}

#include "ui_backend.h"

namespace {

constexpr unsigned REPEAT_FIRST_MS = 380, REPEAT_MS = 110;
constexpr int STICK_ON = 16000, STICK_OFF = 9000;
constexpr float SLIDE_MS = 160.0f;
constexpr unsigned TOAST_MS = 2200;

/* ---- the tiles ---- */

enum Section { S_GAME, S_TIME, S_LOOK, S_OVERLAYS, S_CONTROLS, S_SAVES, S_MORE, S_COUNT };
const char *const section_names[S_COUNT] = {"Game",     "Time",  "Look", "Overlays",
                                            "Controls", "Saves", "More"};

enum Kind { K_AUTO, K_BUTTON, K_TOGGLE, K_STEP, K_SLIDER };
enum { TF_CLOSE = 1, TF_CONFIRM = 2, TF_WIDE = 4 /* a row of its own: a slider */ };

struct Tile {
    Section section;
    const char *title;
    Kind kind;
    /* A settings row: by key, or by category + label (rows with no key).
       The kind follows the row's type unless it is a button's. */
    const char *key, *cat, *label;
    /* No row: what the tile does and shows. */
    void (*run)(void);
    int (*enabled)(void);
    int (*get)(void);
    void (*set)(int);
    int min, max, step;
    const char *(*text)(char *buf, size_t n);
    unsigned flags;
    const setting *row;   /* resolved when the menu opens */
};

struct Hooks {
    quickmenu_hooks h;
} HK = {};

int slot_sel = 1;     /* slots 1-4 for the slot tiles */
int mute_saved = 100;

void qm_close_now(void);

void run_resume(void) { qm_close_now(); }
void run_quicksave(void) { actions_emit(ACTION_QUICKSAVE, 1); }
void run_quickload(void) { actions_emit(ACTION_QUICKLOAD, 1); }
void run_slot_next(void) { slot_sel = slot_sel % 4 + 1; }
const char *text_slot(char *buf, size_t n)
{
    snprintf(buf, n, "Slot %d", slot_sel);
    return buf;
}
void run_save_slot(void)
{
    if (HK.h.save_slot)
        HK.h.save_slot(slot_sel);
}
void run_load_slot(void)
{
    if (HK.h.load_slot)
        HK.h.load_slot(slot_sel);
}

const setting *volume_row(void) { return settings_find("volume"); }
int get_muted(void)
{
    const setting *v = volume_row();
    return v && settings_get_value(v) == 0;
}
void set_muted(int on)
{
    const setting *v = volume_row();
    if (!v)
        return;
    if (on) {
        int cur = settings_get_value(v);
        if (cur > 0)
            mute_saved = cur;
        settings_set_value(v, 0);
    } else {
        settings_set_value(v, mute_saved > 0 ? mute_saved : 100);
    }
}
int have_volume(void) { return volume_row() != NULL; }

void run_look(void)
{
    if (HK.h.look_cycle)
        HK.h.look_cycle();
}
int look_enabled(void) { return HK.h.look_cycle && (!HK.h.look_enabled || HK.h.look_enabled()); }
const char *text_look(char *buf, size_t n)
{
    const setting *p = settings_find("preset");
    return p ? settings_value_text(p, buf, n) : "";
}

/* ---- tiles with no row of their own ---- */

void run_framestep(void) { actions_emit(ACTION_FRAMESTEP, 1); }
int rewind_enabled(void)
{
    const setting *s = settings_find("rewind_seconds");
    return s && settings_get_value(s) > 0;
}
void run_rewind(void) { timectl_rewind_for(60); }   /* about a second of game time */

/* Restore: step over the dated backups ("Restore backup 1..6", saves.c),
   then press Restore twice. */
constexpr int SAVES_PICK = 6;
int restore_sel = 0;
const setting *restore_row(int k)
{
    char lab[40];
    snprintf(lab, sizeof lab, "Restore backup %d", k + 1);
    for (int i = 0; i < settings_count(); i++) {
        const setting *s = settings_at(i);
        if (s->category && s->label && !strcmp(s->category, "Save states") && !strcmp(s->label, lab))
            return s;
    }
    return NULL;
}
int restore_enabled(void)
{
    const setting *r = restore_row(restore_sel);
    return r && settings_is_enabled(r);
}
int get_restore_sel(void) { return restore_sel; }
void set_restore_sel(int v)
{
    const setting *r = restore_row(v);
    restore_sel = r && settings_is_enabled(r) ? v : 0;   /* past the last backup: round to the newest */
}
const char *text_restore_sel(char *buf, size_t n)
{
    const setting *r = restore_row(restore_sel);
    if (!r || !r->text)
        return "-";
    return r->text(r->user, buf, n);
}
void run_restore(void)
{
    const setting *r = restore_row(restore_sel);
    if (r && r->run)
        r->run(r->user);
}

void run_all_settings(void)
{
    qm_close_now();
    menu_open();
}

/* The tiles, section by section (a section with no tile is not drawn).
   Rows named by key or by category + label come from the settings
   registry (main.c, qol.c); a tile whose row is not registered is left
   out. */
Tile TILES[] = {
    /* ---- Game ---- */
    {S_GAME, "Resume", K_BUTTON, 0, 0, 0, run_resume, 0, 0, 0, 0, 0, 0, 0, TF_CLOSE, 0},
    {S_GAME, "Quick save", K_BUTTON, 0, 0, 0, run_quicksave, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_GAME, "Quick load", K_BUTTON, 0, 0, 0, run_quickload, 0, 0, 0, 0, 0, 0, 0, TF_CLOSE, 0},
    {S_GAME, "Mute", K_TOGGLE, 0, 0, 0, 0, have_volume, get_muted, set_muted, 0, 1, 1, 0, 0, 0},
    {S_GAME, "Slot", K_BUTTON, 0, 0, 0, run_slot_next, 0, 0, 0, 0, 0, 0, text_slot, 0, 0},
    {S_GAME, "Save to slot", K_BUTTON, 0, 0, 0, run_save_slot, 0, 0, 0, 0, 0, 0, text_slot, 0, 0},
    {S_GAME, "Load slot", K_BUTTON, 0, 0, 0, run_load_slot, 0, 0, 0, 0, 0, 0, text_slot, TF_CLOSE,
     0},
    {S_GAME, "Volume", K_AUTO, "volume", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TF_WIDE, 0},
    {S_GAME, "Reset", K_AUTO, 0, "Save states", "Reset the game", 0, 0, 0, 0, 0, 0, 0, 0,
     TF_CONFIRM | TF_CLOSE, 0},
    /* ---- Time (timectl.c rows; Rewind and Frame step are buttons) ---- */
    {S_TIME, "Game speed", K_AUTO, "slow_speed", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_TIME, "Rewind length", K_AUTO, "rewind_seconds", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_TIME, "Frame step", K_BUTTON, 0, 0, 0, run_framestep, 0, 0, 0, 0, 0, 0, 0, TF_CLOSE, 0},
    {S_TIME, "Rewind", K_BUTTON, 0, 0, 0, run_rewind, rewind_enabled, 0, 0, 0, 0, 0, 0, TF_CLOSE,
     0},
    /* ---- Look ---- */
    {S_LOOK, "Look", K_BUTTON, 0, 0, 0, run_look, look_enabled, 0, 0, 0, 0, 0, text_look, 0, 0},
    {S_LOOK, "Aspect", K_AUTO, "aspect", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_LOOK, "Integer scale", K_AUTO, 0, "Display", "Integer scale", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_LOOK, "CRT look", K_AUTO, 0, "Display", "CRT look", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_LOOK, "Fullscreen", K_AUTO, "fullscreen", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    /* ---- Overlays (overlays.c rows) ---- */
    {S_OVERLAYS, "FPS", K_AUTO, "overlay_fps", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_OVERLAYS, "Clock", K_AUTO, "overlay_clock", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_OVERLAYS, "Battery", K_AUTO, "overlay_battery", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_OVERLAYS, "Play time", K_AUTO, "overlay_playtime", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_OVERLAYS, "Corner", K_AUTO, "overlay_corner", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    /* ---- Controls (input.c and touch.c rows) ---- */
    {S_CONTROLS, "Swap A/B", K_AUTO, "swap_ab", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_CONTROLS, "Swap X/Y", K_AUTO, "swap_xy", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_CONTROLS, "Dead zone", K_AUTO, "stick_deadzone", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TF_WIDE, 0},
    {S_CONTROLS, "Touch size", K_AUTO, "touch_size", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_CONTROLS, "Touch opacity", K_AUTO, "touch_opacity", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {S_CONTROLS, "Touch hand", K_AUTO, "touch_hand", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    /* ---- Saves (saves.c rows; Restore picks a dated backup, then asks twice) ---- */
    {S_SAVES, "Back up now", K_AUTO, 0, "Save states", "Back up now", 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0},
    {S_SAVES, "Backup", K_STEP, 0, 0, 0, 0, restore_enabled, get_restore_sel, set_restore_sel, 0,
     SAVES_PICK - 1, 1, text_restore_sel, 0, 0},
    {S_SAVES, "Restore", K_BUTTON, 0, 0, 0, run_restore, restore_enabled, 0, 0, 0, 0, 0, 0,
     TF_CONFIRM, 0},
    /* ---- More ---- */
    {S_MORE, "All settings...", K_BUTTON, 0, 0, 0, run_all_settings, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
constexpr int N_TILES = (int)(sizeof TILES / sizeof *TILES);

const setting *find_row(const Tile &t)
{
    if (t.key)
        return settings_find(t.key);
    if (t.cat && t.label)
        for (int k = 0; k < settings_count(); k++) {
            const setting *s = settings_at(k);
            if (s->category && s->label && !strcmp(s->category, t.cat) && !strcmp(s->label, t.label))
                return s;
        }
    return NULL;
}

bool needs_row(const Tile &t) { return t.key || t.cat; }

Kind kind_of(const Tile &t)
{
    if (!t.row)
        return t.kind == K_AUTO ? K_BUTTON : t.kind;
    switch (t.row->type) {
    case SETTING_BOOL: return K_TOGGLE;
    case SETTING_ENUM: return K_STEP;
    case SETTING_INT: return K_SLIDER;
    default: return K_BUTTON;
    }
}

bool t_enabled(const Tile &t)
{
    if (t.row)
        return settings_is_enabled(t.row);
    return !t.enabled || t.enabled();
}

void t_range(const Tile &t, int *mn, int *mx, int *st)
{
    if (t.row && t.row->type == SETTING_INT) {
        *mn = t.row->min, *mx = t.row->max, *st = t.row->step > 0 ? t.row->step : 1;
    } else if (t.row && t.row->type == SETTING_ENUM) {
        *mn = 0, *mx = (t.row->count > 0 ? t.row->count : 1) - 1, *st = 1;
    } else if (t.row) {
        *mn = 0, *mx = 1, *st = 1;
    } else {
        *mn = t.min, *mx = t.max, *st = t.step > 0 ? t.step : 1;
    }
}

int t_get(const Tile &t)
{
    return t.row ? settings_get_value(t.row) : t.get ? t.get() : 0;
}

void t_set(const Tile &t, int v)
{
    if (t.row)
        settings_set_value(t.row, v);
    else if (t.set)
        t.set(v);
}

const char *t_text(const Tile &t, char *buf, size_t n)
{
    buf[0] = 0;
    if (t.row)
        return t.row->type == SETTING_ACTION && !t.row->text ? buf
                                                              : settings_value_text(t.row, buf, n);
    if (t.text)
        return t.text(buf, n);
    return buf;
}

/* ---- state ---- */

struct Dir {
    int x, y;
};

struct Rect {
    float x, y, w, h;   /* content coordinates, then screen */
};

struct Ptr {
    bool active, mouse;
    SDL_FingerID fid;
    float x0, y0, x, y;
    int tile, mode;   /* mode: 0 none, 1 scroll, 2 slider, 3 outside the sheet */
    float scroll0;
};

struct Layout {
    float W, H, k, margin, gap, tw, th, head_h, foot_h, sec_h;
    int cols;
    float sx, sy, sw, sh;           /* the sheet, screen, as drawn (slide applied) */
    float vy, vh;                   /* the tile viewport inside it */
    float content_h;
    Rect r[N_TILES];                /* content coordinates, by position in vis[] */
    float sec_y[S_COUNT];           /* content y of each section's heading, -1 none */
};

struct QM {
    bool open;
    long opened;
    Tile *vis[N_TILES];
    int nvis;
    int focus;
    float goal_x;     /* the column an up / down walk keeps (content x), -1: the focus' own */
    int confirm;      /* index in vis of a tile waiting for its second press, -1 */
    Dir pad, stick, keys;
    int cur_x, cur_y;
    unsigned held_ms, next_ms;
    float scroll;
    bool ensure;
    float slide;      /* 0..1 */
    char toast[64];
    unsigned toast_ms;
    float last_W, last_H;
    Layout L;
    bool have_layout;
    Ptr p;
} Q = {};

void qm_close_now(void)
{
    Q.open = false;
    Q.p.active = false;
}

void toast(const char *t)
{
    snprintf(Q.toast, sizeof Q.toast, "%s", t);
    Q.toast_ms = TOAST_MS;
}

void rebuild_vis(void)
{
    Q.nvis = 0;
    for (int s = 0; s < S_COUNT; s++)
        for (int i = 0; i < N_TILES; i++) {
            Tile &t = TILES[i];
            if (t.section != s)
                continue;
            t.row = needs_row(t) ? find_row(t) : NULL;
            if (needs_row(t) && !t.row)
                continue;
            Q.vis[Q.nvis++] = &t;
        }
}

/* ---- layout ---- */

void make_layout(Layout &L, float W, float H)
{
    L.W = W, L.H = H;
    float k = fminf(W, H) / 800.0f * ui_menu_scale();
    L.k = k = fmaxf(0.5f, fminf(k, 2.6f));
    L.margin = 16 * k;
    L.gap = 10 * k;
    L.cols = W >= H ? 4 : 3;
    L.sw = fminf(W, 1500.0f * k);
    L.sx = (W - L.sw) * 0.5f;
    L.th = 88 * k;
    L.head_h = 46 * k;
    L.foot_h = 42 * k;
    L.sec_h = 30 * k;
    for (;;) {   /* a tile is never narrower than 72 px (scaled) */
        L.tw = (L.sw - 2 * L.margin - (L.cols - 1) * L.gap) / L.cols;
        if (L.tw >= 72 * k || L.cols <= 1)
            break;
        L.cols--;
    }
    float y = 0;
    for (int s = 0; s < S_COUNT; s++)
        L.sec_y[s] = -1;
    int i = 0;
    while (i < Q.nvis) {
        Section s = Q.vis[i]->section;
        L.sec_y[s] = y;
        y += L.sec_h;
        int c = 0;
        for (; i < Q.nvis && Q.vis[i]->section == s; i++, c++) {
            if (Q.vis[i]->flags & TF_WIDE) {   /* a row of its own */
                c = (c + L.cols - 1) / L.cols * L.cols;
                L.r[i] = {L.sx + L.margin, y + c / L.cols * (L.th + L.gap),
                          L.cols * L.tw + (L.cols - 1) * L.gap, L.th};
                c += L.cols - 1;
                continue;
            }
            int col = c % L.cols, row = c / L.cols;
            L.r[i] = {L.sx + L.margin + col * (L.tw + L.gap), y + row * (L.th + L.gap), L.tw, L.th};
        }
        y += ((c + L.cols - 1) / L.cols) * (L.th + L.gap);
    }
    L.content_h = y > 0 ? y - L.gap : 0;
    float max_sheet = H * 0.82f;
    float view = L.content_h;
    if (L.head_h + view + L.foot_h > max_sheet)
        view = max_sheet - L.head_h - L.foot_h;
    L.vh = view;
    L.sh = L.head_h + view + L.foot_h + 10 * k;
    float e = 1.0f - Q.slide;
    e = e * e;   /* ease out */
    L.sy = H - L.sh + L.sh * e;
    L.vy = L.sy + L.head_h;
}

void relayout(void)
{
    float W = Q.last_W > 0 ? Q.last_W : 1280, H = Q.last_H > 0 ? Q.last_H : 800;
    make_layout(Q.L, W, H);
    Q.have_layout = true;
    float maxs = fmaxf(0.0f, Q.L.content_h - Q.L.vh);
    Q.scroll = fmaxf(0.0f, fminf(Q.scroll, maxs));
}

Rect screen_rect(int i)
{
    Rect r = Q.L.r[i];
    r.y = r.y - Q.scroll + Q.L.vy;
    return r;
}

void ensure_focus_visible(void)
{
    if (Q.focus < 0 || Q.focus >= Q.nvis)
        return;
    const Rect &r = Q.L.r[Q.focus];
    float top = r.y, bot = r.y + r.h;
    /* the section heading above a first row belongs with it */
    int s = Q.vis[Q.focus]->section;
    if (Q.L.sec_y[s] >= 0 && r.y - Q.L.sec_y[s] <= Q.L.sec_h + 1)
        top = Q.L.sec_y[s];
    if (top < Q.scroll)
        Q.scroll = top;
    if (bot > Q.scroll + Q.L.vh)
        Q.scroll = bot - Q.L.vh;
    float maxs = fmaxf(0.0f, Q.L.content_h - Q.L.vh);
    Q.scroll = fmaxf(0.0f, fminf(Q.scroll, maxs));
}

/* ---- actions on tiles ---- */

bool adjustable(const Tile &t)
{
    Kind k = kind_of(t);
    return k == K_SLIDER || k == K_STEP;
}

/* Left / right on an adjustable tile: 1 if it moved the value. */
int adjust(Tile &t, int dir)
{
    if (!t_enabled(t))
        return 0;
    int mn, mx, st;
    t_range(t, &mn, &mx, &st);
    int v = t_get(t), nv = v + dir * st;
    if (nv < mn || nv > mx)
        return 0;
    t_set(t, nv);
    return 1;
}

void activate(int i)
{
    if (i < 0 || i >= Q.nvis)
        return;
    Tile &t = *Q.vis[i];
    if (!t_enabled(t)) {
        toast("Not available here");
        return;
    }
    switch (kind_of(t)) {
    case K_TOGGLE:
        t_set(t, !t_get(t));
        break;
    case K_STEP: {
        int mn, mx, st;
        t_range(t, &mn, &mx, &st);
        int nv = t_get(t) + st;
        t_set(t, nv > mx ? mn : nv);   /* A goes round */
        break;
    }
    case K_SLIDER:
        break;   /* left / right, or drag */
    default:
        if ((t.flags & TF_CONFIRM) && Q.confirm != i) {
            Q.confirm = i;
            toast("Press again to confirm");
            return;
        }
        Q.confirm = -1;
        if (t.row && t.row->run)
            t.row->run(t.row->user);
        else if (t.run)
            t.run();
        if (t.flags & TF_CLOSE)
            qm_close_now();
        break;
    }
}

void set_slider_at(Tile &t, float x, const Rect &r)
{
    if (!t_enabled(t))
        return;
    int mn, mx, st;
    t_range(t, &mn, &mx, &st);
    float pad = 12 * Q.L.k;
    float f = (x - (r.x + pad)) / (r.w - 2 * pad);
    f = fmaxf(0.0f, fminf(1.0f, f));
    int steps = (mx - mn) / st;
    int v = mn + (int)lroundf(f * steps) * st;
    if (v != t_get(t))
        t_set(t, v);
}

/* ---- focus movement ---- */

void focus_to(int i)
{
    if (i < 0 || i >= Q.nvis)
        return;
    if (Q.focus != i)
        Q.confirm = -1;
    Q.focus = i;
    Q.goal_x = -1;
    Q.ensure = true;
}

bool move_focus(int dx, int dy)
{
    if (Q.focus < 0 || !Q.nvis)
        return false;
    const Rect &f = Q.L.r[Q.focus];
    float fcx = f.x + f.w * 0.5f, fcy = f.y + f.h * 0.5f;
    /* up / down keep the column they started in across short rows and
       wide tiles, so a tile behind a stepper is still reachable */
    float gx = Q.goal_x >= 0 ? Q.goal_x : fcx;
    int best = -1;
    float bd = 1e18f;
    for (int i = 0; i < Q.nvis; i++) {
        if (i == Q.focus)
            continue;
        const Rect &r = Q.L.r[i];
        float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
        float ddx = cx - fcx, ddy = cy - fcy;
        float d;
        if (dx) {
            if (fabsf(ddy) > f.h * 0.5f || ddx * dx <= 0)
                continue;
            d = fabsf(ddx);
        } else {
            if (ddy * dy <= f.h * 0.5f)
                continue;
            d = fabsf(ddy) * 1000.0f + fabsf(cx - gx);   /* the nearest row, then the nearest column */
        }
        if (d < bd)
            bd = d, best = i;
    }
    if (best < 0)
        return false;
    focus_to(best);
    if (dy)
        Q.goal_x = gx;
    return true;
}

void page(int dir)
{
    if (Q.focus < 0)
        return;
    int s = Q.vis[Q.focus]->section;
    if (dir < 0) {
        int i = Q.focus;
        while (i > 0 && Q.vis[i - 1]->section == s)
            i--;   /* the section's first tile */
        if (i != Q.focus) {
            focus_to(i);
            return;
        }
        if (i > 0) {   /* the one before it */
            int ps = Q.vis[i - 1]->section, j = i - 1;
            while (j > 0 && Q.vis[j - 1]->section == ps)
                j--;
            focus_to(j);
        }
    } else {
        for (int i = Q.focus; i < Q.nvis; i++)
            if (Q.vis[i]->section != s) {
                focus_to(i);
                return;
            }
    }
}

void dir_press(int dx, int dy)
{
    if (dy) {
        move_focus(0, dy);
        return;
    }
    if (Q.focus >= 0 && Q.focus < Q.nvis) {
        Tile &t = *Q.vis[Q.focus];
        if (adjustable(t) && t_enabled(t) && adjust(t, dx))
            return;
    }
    move_focus(dx, 0);
}

void update_dir(void)
{
    int x = Q.pad.x ? Q.pad.x : Q.keys.x ? Q.keys.x : Q.stick.x;
    int y = Q.pad.y ? Q.pad.y : Q.keys.y ? Q.keys.y : Q.stick.y;
    if (x == Q.cur_x && y == Q.cur_y)
        return;
    Q.cur_x = x, Q.cur_y = y;
    Q.held_ms = 0;
    Q.next_ms = REPEAT_FIRST_MS;
    if (y)
        dir_press(0, y);
    else if (x)
        dir_press(x, 0);
}

int stick_dir(int v, int cur)
{
    if (v <= -STICK_ON)
        return -1;
    if (v >= STICK_ON)
        return 1;
    if (cur && (v * cur) > STICK_OFF)
        return cur;
    return 0;
}

void back(void)
{
    if (Q.confirm >= 0)
        Q.confirm = -1;
    else
        qm_close_now();
}

/* ---- pointer: mouse and touch ---- */

int tile_at(float x, float y)
{
    if (!Q.have_layout || y < Q.L.vy || y > Q.L.vy + Q.L.vh)
        return -1;
    for (int i = 0; i < Q.nvis; i++) {
        Rect r = screen_rect(i);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
            return i;
    }
    return -1;
}

bool in_sheet(float x, float y)
{
    return Q.have_layout && y >= Q.L.sy && x >= Q.L.sx && x < Q.L.sx + Q.L.sw;
}

void ptr_down(bool mouse, SDL_FingerID fid, float x, float y)
{
    if (Q.p.active || !Q.have_layout)
        return;
    Ptr &p = Q.p;
    p = {};
    p.active = true, p.mouse = mouse, p.fid = fid;
    p.x0 = p.x = x, p.y0 = p.y = y;
    p.tile = -1;
    if (!in_sheet(x, y)) {
        p.mode = 3;
        return;
    }
    p.tile = tile_at(x, y);
    p.scroll0 = Q.scroll;
    if (p.tile >= 0) {
        focus_to(p.tile);
        Tile &t = *Q.vis[p.tile];
        if (kind_of(t) == K_SLIDER) {
            p.mode = 2;
            set_slider_at(t, x, screen_rect(p.tile));
        }
    }
}

void ptr_move(float x, float y)
{
    Ptr &p = Q.p;
    if (!p.active)
        return;
    p.x = x, p.y = y;
    if (p.mode == 2) {
        set_slider_at(*Q.vis[p.tile], x, screen_rect(p.tile));
    } else if (p.mode == 0 && fabsf(y - p.y0) > 10 * Q.L.k) {
        p.mode = 1;
    }
    if (p.mode == 1) {
        float maxs = fmaxf(0.0f, Q.L.content_h - Q.L.vh);
        Q.scroll = fmaxf(0.0f, fminf(p.scroll0 - (y - p.y0), maxs));
    }
}

void ptr_up(void)
{
    Ptr p = Q.p;
    Q.p.active = false;
    if (!p.active)
        return;
    if (p.mode == 3) {
        if (fabsf(p.x - p.x0) < 14 * Q.L.k && fabsf(p.y - p.y0) < 14 * Q.L.k && !in_sheet(p.x, p.y))
            qm_close_now();
        return;
    }
    if (p.mode != 0 || p.tile < 0 || tile_at(p.x, p.y) != p.tile)
        return;
    Tile &t = *Q.vis[p.tile];
    Rect r = screen_rect(p.tile);
    if (adjustable(t) && kind_of(t) == K_STEP && t_enabled(t)) {
        float f = (p.x - r.x) / r.w;
        if (f < 0.28f) {
            adjust(t, -1);
            return;
        }
        if (f > 0.72f) {
            adjust(t, 1);
            return;
        }
    }
    activate(p.tile);
}

/* Mouse positions are window points; the sheet is in output pixels. */
void mouse_pos(int mx, int my, float *x, float *y)
{
    float sx = 1, sy = 1;
    SDL_Window *w = ui_window();
    if (w && Q.last_W > 0) {
        int ww = 0, wh = 0;
        SDL_GetWindowSize(w, &ww, &wh);
        if (ww > 0 && wh > 0)
            sx = Q.last_W / ww, sy = Q.last_H / wh;
    }
    *x = mx * sx;
    *y = my * sy;
}

}   // namespace

extern "C" {

void quickmenu_set_hooks(const quickmenu_hooks *h)
{
    HK.h = h ? *h : quickmenu_hooks{};
}

void quickmenu_open(void)
{
    if (Q.open || !ui_have_host())
        return;
    settings_refresh();
    rebuild_vis();
    Q.open = true;
    Q.opened++;
    Q.pad = Q.stick = Q.keys = Dir{0, 0};
    Q.cur_x = Q.cur_y = 0;
    Q.confirm = -1;
    Q.scroll = 0;
    Q.slide = 0;
    Q.toast[0] = 0;
    Q.toast_ms = 0;
    Q.p = {};
    Q.focus = Q.nvis ? 0 : -1;
    Q.goal_x = -1;
    restore_sel = 0;   /* Restore starts at the newest backup */
    Q.ensure = true;
    relayout();
}

void quickmenu_close(void) { qm_close_now(); }
int quickmenu_is_open(void) { return Q.open; }
long quickmenu_opened_count(void) { return Q.opened; }

int quickmenu_event(const SDL_Event *ev)
{
    if (!Q.open)
        return 0;
    switch (ev->type) {
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        int down = ev->type == SDL_CONTROLLERBUTTONDOWN;
        switch (ev->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: Q.pad.y = down ? -1 : Q.pad.y == -1 ? 0 : Q.pad.y; break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: Q.pad.y = down ? 1 : Q.pad.y == 1 ? 0 : Q.pad.y; break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: Q.pad.x = down ? -1 : Q.pad.x == -1 ? 0 : Q.pad.x; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: Q.pad.x = down ? 1 : Q.pad.x == 1 ? 0 : Q.pad.x; break;
        default:
            if (!down)
                break;
            switch (ev->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A: activate(Q.focus); break;
            case SDL_CONTROLLER_BUTTON_B: back(); break;
            case SDL_CONTROLLER_BUTTON_START: qm_close_now(); break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: page(-1); break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: page(1); break;
            default: break;
            }
        }
        update_dir();
        return 1;
    }
    case SDL_CONTROLLERAXISMOTION:
        if (ev->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
            Q.stick.x = stick_dir(ev->caxis.value, Q.stick.x);
        else if (ev->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
            Q.stick.y = stick_dir(ev->caxis.value, Q.stick.y);
        else
            return 1;
        update_dir();
        return 1;
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        int down = ev->type == SDL_KEYDOWN;
        SDL_Scancode sc = ev->key.keysym.scancode;
        switch (sc) {
        case SDL_SCANCODE_UP: Q.keys.y = down ? -1 : Q.keys.y == -1 ? 0 : Q.keys.y; break;
        case SDL_SCANCODE_DOWN: Q.keys.y = down ? 1 : Q.keys.y == 1 ? 0 : Q.keys.y; break;
        case SDL_SCANCODE_LEFT: Q.keys.x = down ? -1 : Q.keys.x == -1 ? 0 : Q.keys.x; break;
        case SDL_SCANCODE_RIGHT: Q.keys.x = down ? 1 : Q.keys.x == 1 ? 0 : Q.keys.x; break;
        default:
            if (!down || ev->key.repeat)
                break;
            switch (sc) {
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER:
            case SDL_SCANCODE_SPACE: activate(Q.focus); break;
            case SDL_SCANCODE_ESCAPE:
            case SDL_SCANCODE_BACKSPACE: back(); break;
            case SDL_SCANCODE_Q:
            case SDL_SCANCODE_PAGEUP: page(-1); break;
            case SDL_SCANCODE_W:
            case SDL_SCANCODE_PAGEDOWN:
            case SDL_SCANCODE_TAB: page(1); break;
            default: return 0;   /* not ours: the caller's (the quick menu key closes it) */
            }
        }
        update_dir();
        return 1;
    }
    case SDL_MOUSEMOTION:
        if (ev->motion.which == SDL_TOUCH_MOUSEID)
            return 1;   /* a finger's mouse copy: the finger events are used */
        {
            float x, y;
            mouse_pos(ev->motion.x, ev->motion.y, &x, &y);
            if (Q.p.active && Q.p.mouse)
                ptr_move(x, y);
            else if (!Q.p.active) {
                int t = tile_at(x, y);
                if (t >= 0 && t != Q.focus && (ev->motion.xrel || ev->motion.yrel))
                    focus_to(t);
            }
        }
        return 1;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return 1;
        if (ev->button.button != SDL_BUTTON_LEFT)
            return 1;
        {
            float x, y;
            mouse_pos(ev->button.x, ev->button.y, &x, &y);
            if (ev->type == SDL_MOUSEBUTTONDOWN)
                ptr_down(true, 0, x, y);
            else if (Q.p.active && Q.p.mouse) {
                Q.p.x = x, Q.p.y = y;
                ptr_up();
            }
        }
        return 1;
    case SDL_MOUSEWHEEL:
        if (ev->wheel.which == SDL_TOUCH_MOUSEID)
            return 1;
        Q.scroll -= ev->wheel.y * Q.L.th;
        relayout();
        return 1;
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
    case SDL_FINGERUP: {
        float x = ev->tfinger.x * Q.last_W, y = ev->tfinger.y * Q.last_H;
        if (ev->type == SDL_FINGERDOWN) {
            ptr_down(false, ev->tfinger.fingerId, x, y);
        } else if (Q.p.active && !Q.p.mouse && Q.p.fid == ev->tfinger.fingerId) {
            if (ev->type == SDL_FINGERMOTION) {
                ptr_move(x, y);
            } else {
                Q.p.x = x, Q.p.y = y;
                ptr_up();
            }
        }
        return 1;
    }
    case SDL_WINDOWEVENT:
    default:
        return 0;
    }
}

void quickmenu_tick(unsigned ms)
{
    if (!Q.open)
        return;
    if (Q.slide < 1.0f)
        Q.slide = fminf(1.0f, Q.slide + ms / SLIDE_MS);
    if (Q.toast_ms)
        Q.toast_ms = ms >= Q.toast_ms ? 0 : Q.toast_ms - ms;
    if (!Q.cur_x && !Q.cur_y)
        return;
    Q.held_ms += ms;
    while (Q.held_ms >= Q.next_ms) {
        Q.next_ms += REPEAT_MS;
        if (Q.cur_y)
            dir_press(0, Q.cur_y);
        else
            dir_press(Q.cur_x, 0);
    }
}

/* ---- drawing ---- */

namespace {

const ImU32 C_TEXT = IM_COL32(236, 238, 244, 255);
const ImU32 C_DIM = IM_COL32(150, 158, 174, 255);
const ImU32 C_FAINT = IM_COL32(96, 104, 120, 255);
const ImU32 C_ACCENT = IM_COL32(242, 193, 78, 255);
const ImU32 C_ACCENT_BG = IM_COL32(242, 193, 78, 40);
const ImU32 C_PANEL = IM_COL32(16, 19, 27, 246);
const ImU32 C_TILE = IM_COL32(34, 39, 52, 255);
const ImU32 C_TILE_OFF = IM_COL32(26, 30, 40, 255);
const ImU32 C_LINE = IM_COL32(255, 255, 255, 24);
const ImU32 C_PILL_OFF = IM_COL32(70, 76, 92, 255);

void triangle(ImDrawList *dl, ImVec2 c, float r, int dir, ImU32 col)
{
    if (dir < 0)
        dl->AddTriangleFilled(ImVec2(c.x - r * 0.6f, c.y), ImVec2(c.x + r * 0.5f, c.y - r),
                              ImVec2(c.x + r * 0.5f, c.y + r), col);
    else
        dl->AddTriangleFilled(ImVec2(c.x + r * 0.6f, c.y), ImVec2(c.x - r * 0.5f, c.y - r),
                              ImVec2(c.x - r * 0.5f, c.y + r), col);
}

void draw_tile(ImDrawList *dl, int i, const Rect &r, float k)
{
    Tile &t = *Q.vis[i];
    bool on = t_enabled(t), sel = i == Q.focus;
    Kind kd = kind_of(t);
    ImVec2 a(r.x, r.y), b(r.x + r.w, r.y + r.h);
    float rad = 12 * k;
    dl->AddRectFilled(a, b, on ? C_TILE : C_TILE_OFF, rad);
    if (sel) {
        dl->AddRectFilled(a, b, C_ACCENT_BG, rad);
        dl->AddRect(a, b, C_ACCENT, rad, fmaxf(2.0f, 3 * k));
    } else {
        dl->AddRect(a, b, C_LINE, rad, 1.0f);
    }
    char buf[96];
    const char *val = t_text(t, buf, sizeof buf);
    bool armed = Q.confirm == i;
    float tsz = 17 * k, vsz = 24 * k, pad = 14 * k;
    ImU32 tcol = !on ? C_FAINT : sel ? C_TEXT : IM_COL32(214, 218, 228, 255);
    ImU32 vcol = !on ? C_FAINT : sel ? C_ACCENT : C_TEXT;
    bool has_value = kd != K_BUTTON || (val && *val);
    if (kd == K_BUTTON && !has_value) {   /* a plain button: its name, large and centred */
        const char *nm = armed ? "Again to confirm" : t.title;
        ImVec2 ts = ui_text_size(vsz, nm);
        ui_text(dl, vsz, ImVec2(r.x + (r.w - ts.x) * 0.5f, r.y + (r.h - ts.y) * 0.5f),
                armed ? C_ACCENT : tcol, nm);
        return;
    }
    ui_text(dl, tsz, ImVec2(r.x + pad, r.y + 9 * k), tcol, t.title);
    float cy = r.y + r.h * 0.5f + 8 * k;
    switch (kd) {
    case K_TOGGLE: {
        bool v = t_get(t) != 0;
        float ph = 28 * k, pw = 54 * k;
        ImVec2 pa(r.x + pad, r.y + r.h - pad - ph + 2 * k), pb(pa.x + pw, pa.y + ph);
        dl->AddRectFilled(pa, pb, v && on ? C_ACCENT : C_PILL_OFF, ph * 0.5f);
        ImVec2 kc(v ? pb.x - ph * 0.5f : pa.x + ph * 0.5f, pa.y + ph * 0.5f);
        dl->AddCircleFilled(kc, ph * 0.38f, on ? IM_COL32(250, 250, 252, 255) : C_DIM);
        ui_text(dl, tsz * 1.05f, ImVec2(pb.x + 12 * k, pa.y + (ph - tsz * 1.05f) * 0.5f), vcol,
                v ? "On" : "Off");
        break;
    }
    case K_STEP: {
        int mn, mx, st;
        t_range(t, &mn, &mx, &st);
        int v = t_get(t);
        ImVec2 ts = ui_text_size(vsz, val);
        float ar = 8 * k;
        triangle(dl, ImVec2(r.x + pad + ar, cy), ar, -1,
                 on && v > mn ? (sel ? C_ACCENT : C_DIM) : IM_COL32(255, 255, 255, 30));
        triangle(dl, ImVec2(r.x + r.w - pad - ar, cy), ar, 1,
                 on && v < mx ? (sel ? C_ACCENT : C_DIM) : IM_COL32(255, 255, 255, 30));
        ui_text(dl, vsz, ImVec2(r.x + (r.w - ts.x) * 0.5f, cy - ts.y * 0.5f), vcol, val);
        break;
    }
    case K_SLIDER: {
        int mn, mx, st;
        t_range(t, &mn, &mx, &st);
        int v = t_get(t);
        ImVec2 ts = ui_text_size(vsz, val);
        ui_text(dl, vsz, ImVec2(r.x + r.w - pad - ts.x, r.y + 5 * k), vcol, val);
        float bx0 = r.x + 12 * k, bx1 = r.x + r.w - 12 * k, by = r.y + r.h - 20 * k;
        float f = mx > mn ? (float)(v - mn) / (float)(mx - mn) : 0;
        dl->AddRectFilled(ImVec2(bx0, by), ImVec2(bx1, by + 8 * k), IM_COL32(255, 255, 255, 34), 4 * k);
        dl->AddRectFilled(ImVec2(bx0, by), ImVec2(bx0 + (bx1 - bx0) * f, by + 8 * k),
                          on ? (sel ? C_ACCENT : C_DIM) : C_FAINT, 4 * k);
        dl->AddCircleFilled(ImVec2(bx0 + (bx1 - bx0) * f, by + 4 * k), 8 * k,
                            on ? (sel ? C_ACCENT : C_TEXT) : C_FAINT);
        break;
    }
    default: {   /* a button with a value: Look, Save to slot, Back up now (a status line) */
        char fit[96];
        const char *shown = armed ? "Again to confirm" : val;
        if (ui_text_size(vsz, shown).x > r.w - 2 * pad) {   /* too long for the tile: end it with "..." */
            snprintf(fit, sizeof fit, "%s", shown);
            size_t n = strlen(fit);
            while (n > 4 && ui_text_size(vsz, fit).x > r.w - 2 * pad) {
                n--;
                snprintf(fit + (n > 3 ? n - 3 : 0), sizeof fit - (n > 3 ? n - 3 : 0), "...");
            }
            shown = fit;
        }
        ImVec2 ts = ui_text_size(vsz, shown);
        ui_text(dl, vsz, ImVec2(r.x + pad, r.y + r.h - pad - ts.y + 2 * k), armed ? C_ACCENT : vcol, shown);
        break;
    }
    }
}

float prompt(ImDrawList *dl, float x, float y, float px, const char *tok, const char *tok2,
             const char *text)
{
    float gh = px * 1.25f;
    x += ui_draw_glyph(dl, tok, ImVec2(x, y - gh * 0.5f), gh, px, C_TEXT);
    if (tok2) {
        x += px * 0.15f;
        x += ui_draw_glyph(dl, tok2, ImVec2(x, y - gh * 0.5f), gh, px, C_TEXT);
    }
    x += px * 0.35f;
    ui_text(dl, px * 0.9f, ImVec2(x, y - px * 0.45f), C_TEXT, text);
    return x + ui_text_size(px * 0.9f, text).x + px * 1.4f;
}

void build_ui(float W, float H)
{
    Q.last_W = W, Q.last_H = H;
    relayout();
    if (Q.ensure) {
        ensure_focus_visible();
        Q.ensure = false;
    }
    const Layout &L = Q.L;
    float k = L.k;
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(4, 6, 10, (int)(150 * Q.slide)));
    ImVec2 p0(L.sx, L.sy), p1(L.sx + L.sw, H + 40 * k);
    dl->AddRectFilled(p0, p1, C_PANEL, 18 * k, ImDrawFlags_RoundCornersTop);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 22), 18 * k, 1.0f, ImDrawFlags_RoundCornersTop);
    /* the grab bar */
    dl->AddRectFilled(ImVec2(W * 0.5f - 22 * k, L.sy + 7 * k), ImVec2(W * 0.5f + 22 * k, L.sy + 11 * k),
                      IM_COL32(255, 255, 255, 50), 2 * k);
    /* header */
    float hy = L.sy + 14 * k, hsz = 24 * k;
    ui_text(dl, hsz, ImVec2(L.sx + L.margin, hy), C_ACCENT, "Quick menu");
    {
        const char *t = Q.toast_ms ? Q.toast : "Paused";
        ImVec2 ts = ui_text_size(17 * k, t);
        ui_text(dl, 17 * k, ImVec2(L.sx + L.sw - L.margin - ts.x, hy + 5 * k),
                Q.toast_ms ? C_ACCENT : C_DIM, t);
    }
    /* the tiles, clipped to the viewport */
    dl->PushClipRect(ImVec2(L.sx, L.vy), ImVec2(L.sx + L.sw, L.vy + L.vh), true);
    int shown = 0;
    for (int s = 0; s < S_COUNT; s++) {
        if (L.sec_y[s] < 0)
            continue;
        float y = L.sec_y[s] - Q.scroll + L.vy;
        if (y + L.sec_h >= L.vy && y <= L.vy + L.vh)
            ui_text(dl, 15 * k, ImVec2(L.sx + L.margin + 2 * k, y + 6 * k), C_DIM, section_names[s]);
    }
    for (int i = 0; i < Q.nvis; i++) {
        Rect r = screen_rect(i);
        if (r.y + r.h < L.vy || r.y > L.vy + L.vh)
            continue;
        draw_tile(dl, i, r, k);
        shown++;
    }
    dl->PopClipRect();
    if (L.content_h > L.vh) {   /* a scroll bar */
        float frac = L.vh / L.content_h, maxs = L.content_h - L.vh;
        float bx = L.sx + L.sw - 8 * k, by0 = L.vy + (L.vh - L.vh * frac) * (Q.scroll / maxs);
        dl->AddRectFilled(ImVec2(bx, L.vy), ImVec2(bx + 4 * k, L.vy + L.vh), IM_COL32(255, 255, 255, 18),
                          2 * k);
        dl->AddRectFilled(ImVec2(bx, by0), ImVec2(bx + 4 * k, by0 + L.vh * frac),
                          IM_COL32(255, 255, 255, 90), 2 * k);
    }
    /* the hint line: the buttons of the device in use */
    float fy = L.vy + L.vh + L.foot_h * 0.5f + 2 * k;
    dl->AddLine(ImVec2(L.sx + L.margin, L.vy + L.vh + 2 * k), ImVec2(L.sx + L.sw - L.margin, L.vy + L.vh + 2 * k),
                C_LINE, 1.0f);
    bool kb = glyphs_in_use() == GLYPHS_KEYBOARD;
    float px = 19 * k, fx = L.sx + L.margin;
    bool adj = Q.focus >= 0 && Q.focus < Q.nvis && adjustable(*Q.vis[Q.focus]);
    fx = prompt(dl, fx, fy, px, kb ? "key:Arrows" : "dpad", NULL, adj ? "Move, adjust" : "Move");
    fx = prompt(dl, fx, fy, px, kb ? "key:Return" : "a", NULL, "Select");
    fx = prompt(dl, fx, fy, px, kb ? "key:Escape" : "b", NULL, Q.confirm >= 0 ? "Cancel" : "Close");
    if (L.content_h > L.vh)
        fx = prompt(dl, fx, fy, px, kb ? "key:Q" : "l1", kb ? "key:W" : "r1", "Page");
    (void)fx;
    (void)shown;
}

}   // namespace

void quickmenu_draw(int w, int h)
{
    if (!Q.open || !ui_ensure())
        return;
    ui_frame_begin(w, h);
    ImGuiIO &io = ImGui::GetIO();
    ImGui::NewFrame();
    build_ui(io.DisplaySize.x, io.DisplaySize.y);
    ui_frame_end();
}

int quickmenu_tile_count(void) { return Q.nvis; }
const char *quickmenu_tile_title(int n) { return n >= 0 && n < Q.nvis ? Q.vis[n]->title : ""; }
int quickmenu_columns(void) { return Q.L.cols; }
int quickmenu_focus(void) { return Q.open ? Q.focus : -1; }

int quickmenu_tile_rect(int n, int *x, int *y, int *w, int *h)
{
    if (n < 0 || n >= Q.nvis || !Q.have_layout)
        return -1;
    Rect r = screen_rect(n);
    if (r.y + r.h < Q.L.vy || r.y > Q.L.vy + Q.L.vh)
        return -1;
    *x = (int)r.x, *y = (int)r.y, *w = (int)r.w, *h = (int)r.h;
    return 0;
}

const char *quickmenu_tile_value(int n, char *buf, unsigned size)
{
    buf[0] = 0;
    if (n < 0 || n >= Q.nvis)
        return buf;
    const Tile &t = *Q.vis[n];
    if (kind_of(t) == K_TOGGLE)
        snprintf(buf, size, "%s", t_get(t) ? "On" : "Off");
    else
        t_text(t, buf, size);
    return buf;
}

}   // extern "C"
