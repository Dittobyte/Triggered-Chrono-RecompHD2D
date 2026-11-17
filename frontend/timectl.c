/* SPDX-FileCopyrightText: 2026 jerbanpls
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * port: time controls (timectl.h). */
#include "timectl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "actions.h"
#include "savestate.h"
#include "sched.h"
#include "settings.h"
#include "timectl_ring.h"

static const int speeds[] = {25, 50, 100, 200, 300, 400};
#define N_SPEEDS (int)(sizeof speeds / sizeof *speeds)

static timectl_host host;
static int speed_pct = 100;
static int rewind_secs = 5;
static tcr *ring;
static long last_cap = -1000, last_seen = -1000;
static int rewinding, rew_first, rew_ticks, rew_end, rew_loaded;
static int step_pending, step_running;
static int rew_hold;   /* rewind ticks left in a timed rewind (timectl_rewind_for) */

/* ---- pure helpers ---- */

uint64_t timectl_frame_ticks(uint64_t freq, int pct)
{
    if (pct < 1)
        pct = 1;
    double t = (double)freq * (double)TIMECTL_FRAME_NS / 1e9 * 100.0 / (double)pct;
    return (uint64_t)(t + 0.5);
}

int timectl_show_frame(int pct, long f)
{
    return pct < 200 || f % (pct / 100) == 0;
}

int timectl_ring_entries(int seconds)
{
    if (seconds <= 0)
        return 0;
    /* 60.1 frames a second, a capture every TIMECTL_CAPTURE_EVERY; the one in full counts */
    return seconds * 61 / TIMECTL_CAPTURE_EVERY + 1;
}

/* ---- host ---- */

static void say(const char *msg)
{
    if (host.osd)
        host.osd(msg);
    else
        fprintf(stderr, "ct: %s\n", msg);
}

void timectl_set_host(const timectl_host *h)
{
    if (h)
        host = *h;
}

/* ---- speed ---- */

static int nearest(int pct)
{
    int best = 2;   /* 100% */
    for (int k = 0; k < N_SPEEDS; k++)
        if (abs(speeds[k] - pct) < abs(speeds[best] - pct))
            best = k;
    return best;
}

int timectl_speed_pct(void) { return speed_pct; }

static void apply_speed(int pct, int announce)
{
    pct = speeds[nearest(pct)];
    if (pct == speed_pct)
        return;
    speed_pct = pct;
    if (host.audio_restart)
        host.audio_restart();   /* the sound is dropped from now (or comes back): no stale queue */
    if (host.pacing_reset)
        host.pacing_reset();
    if (announce) {
        char msg[32];
        if (pct == 100)
            snprintf(msg, sizeof msg, "Normal speed");
        else
            snprintf(msg, sizeof msg, "Speed %d%%", pct);
        say(msg);
    }
}

void timectl_speed_set(int pct)
{
    int was = speed_pct;
    apply_speed(pct, 1);
    if (speed_pct != was)
        settings_changed();
}

void timectl_speed_step(int dir)
{
    int k = nearest(speed_pct) + (dir > 0 ? 1 : dir < 0 ? -1 : 0);
    timectl_speed_set(speeds[k < 0 ? 0 : k >= N_SPEEDS ? N_SPEEDS - 1 : k]);
}

int timectl_pace_pct(int ffwd) { return speed_pct * (ffwd < 1 ? 1 : ffwd); }

/* ---- the ring ---- */

static int ring_wanted(void) { return rewind_secs > 0 && !host.recording; }

static void ring_drop(void)
{
    tcr_free(ring);
    ring = NULL;
    last_cap = last_seen = -1000;
}

void timectl_after_frame(void)
{
    step_running = 0;
    if (rewinding)
        return;
    if (!ring_wanted()) {
        if (ring)
            ring_drop();
        return;
    }
    long f = sched_frame_count();
    if (f != last_seen + 1 && ring)   /* a state was loaded, or frames went by unseen */
        tcr_clear(ring);
    last_seen = f;
    if (ring && !tcr_count(ring))
        last_cap = -1000;
    if (f - last_cap < TIMECTL_CAPTURE_EVERY || savestate_ready(NULL))
        return;
    if (!ring) {
        ring = tcr_new(timectl_ring_entries(rewind_secs), TIMECTL_REWIND_BYTES);
        if (!ring)
            return;
    }
    size_t len;
    uint8_t *b = savestate_save(&len, NULL);
    if (!b)
        return;
    if (tcr_push(ring, b, len, f) == 0)
        last_cap = f;
    free(b);
}

/* ---- rewind ---- */

int timectl_rewinding(void) { return rewinding; }

static void load_newest(void)
{
    size_t len;
    long fr;
    const uint8_t *b = tcr_newest(ring, &len, &fr);
    const char *why = NULL;
    if (b && !savestate_load(b, len, &why)) {
        rew_loaded = 1;
        if (host.after_load)
            host.after_load();   /* the cartridge's own save RAM stays */
    } else {
        fprintf(stderr, "ct: rewind: cannot load a state: %s\n", why ? why : "none");
        rew_end = 1;
    }
}

void timectl_rewind(int down)
{
    if (!down) {
        if (!rewinding)
            return;
        rewinding = 0;
        rew_hold = 0;
        if (rew_loaded && ring) {   /* play on from the state it stopped at */
            last_seen = last_cap = sched_frame_count();
        }
        if (host.audio_restart)
            host.audio_restart();
        if (host.pacing_reset)
            host.pacing_reset();
        return;
    }
    if (rewinding)
        return;
    if (host.recording) {
        say("Not while recording (--record)");
        return;
    }
    if (rewind_secs <= 0) {
        say("Rewind is off (rewind seconds)");
        return;
    }
    if (!ring || !tcr_count(ring)) {
        say("Nothing to rewind yet");
        return;
    }
    rewinding = 1;
    rew_first = 1;
    rew_ticks = 0;
    rew_end = 0;
    rew_loaded = 0;
    if (host.audio_restart)
        host.audio_restart();
}

/* The quick menu's Rewind: hold the rewind for about `frames` frames of
   game time (a state back is 4 frames per 2 ticks), then let go. */
void timectl_rewind_for(int frames)
{
    if (rewinding)
        return;   /* already held by the action */
    timectl_rewind(1);
    if (rewinding)
        rew_hold = (frames > 0 ? frames : 1) / 2 + 2;
}

void timectl_rewind_tick(void)
{
    if (!rewinding || !ring)
        return;
    if (rew_hold && --rew_hold == 0) {
        timectl_rewind(0);
        return;
    }
    if (rew_first) {   /* the newest state is the one we are at (or just before) */
        rew_first = 0;
        load_newest();
        return;
    }
    if (rew_end || (++rew_ticks & 1))   /* one state every other tick: 4 frames per 2 */
        return;
    if (tcr_drop_newest(ring)) {   /* the oldest: stay there until let go */
        rew_end = 1;
        say("Oldest state");
        return;
    }
    load_newest();
}

/* ---- frame step ---- */

void timectl_frame_step(void)
{
    if (rewinding)
        return;
    if (host.paused && host.paused())
        step_pending = 1;
    else if (host.pause_toggle)
        host.pause_toggle();   /* the next press steps */
}

int timectl_paused_step_pending(void) { return step_pending; }

void timectl_step_taken(void)
{
    step_pending = 0;
    step_running = 1;
}

int timectl_audio_muted(void)
{
    return speed_pct != 100 || rewinding || step_running;
}

/* ---- settings and actions ---- */

static const char *const speed_labels[N_SPEEDS] = {"0.25x", "0.5x", "1x", "2x", "3x", "4x"};
static const char *const speed_values[N_SPEEDS] = {"25", "50", "100", "200", "300", "400"};

static int get_speed(void *u) { (void)u; return nearest(speed_pct); }
static void set_speed(void *u, int v)
{
    (void)u;
    apply_speed(speeds[v < 0 ? 0 : v >= N_SPEEDS ? N_SPEEDS - 1 : v], 1);
}
static int get_secs(void *u) { (void)u; return rewind_secs; }
static void set_secs(void *u, int v)
{
    (void)u;
    rewind_secs = v < 0 ? 0 : v > TIMECTL_REWIND_MAX_SECONDS ? TIMECTL_REWIND_MAX_SECONDS : v;
    if (!rewind_secs)
        ring_drop();
    else if (ring)
        tcr_limits(ring, timectl_ring_entries(rewind_secs), TIMECTL_REWIND_BYTES);
}

static void on_action(ct_action a, int down, void *user)
{
    (void)user;
    if (a == ACTION_REWIND)
        timectl_rewind(down);
    else if (a == ACTION_FRAMESTEP && down)
        timectl_frame_step();
}

void timectl_init(void)
{
    settings_register(&(setting){.label = "Time", .category = "Controls", .type = SETTING_INFO,
                                 .flags = SETTING_HEADING});
    settings_register(&(setting){
        .key = "slow_speed", .label = "Game speed", .category = "Controls",
        .type = SETTING_ENUM, .options = speed_labels, .values = speed_values,
        .count = N_SPEEDS, .get = get_speed, .set = set_speed,
        .help = "Slow motion or faster than normal; the sound is off unless 1x."});
    settings_register(&(setting){
        .key = "rewind_seconds", .label = "Rewind", .category = "Controls",
        .type = SETTING_INT, .min = 0, .max = TIMECTL_REWIND_MAX_SECONDS, .step = 1,
        .format = "%d s", .get = get_secs, .set = set_secs,
        .help = "Seconds of play kept to rewind through (hold Backspace); 0 is off. Not "
                "while recording."});
    actions_set_handler(ACTION_REWIND, on_action, NULL);
    actions_set_handler(ACTION_FRAMESTEP, on_action, NULL);
}
