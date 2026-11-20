/* SPDX-FileCopyrightText: 2026 jerbanpls
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * port: time controls (docs/USER-GUIDE.md, "Rewind, slow motion and frame
 * step"): slow motion and fast speeds, rewind, and the frame step.
 *
 * Speed. timectl owns the base speed: 25, 50, 100, 200, 300 or 400 percent
 * of the game's 60.0988 Hz (the setting slow_speed). Fast-forward (qol.h,
 * the hold on Tab / L5, or its toggle) multiplies it by its own factor
 * (2-4), so 50% with fast-forward at 3x runs at 150%. main.c paces by
 * timectl_pace_pct(ffwd) and shows one frame in pct/100 from 200%; the
 * game's sound is dropped whenever the speed is not 100% (as in
 * fast-forward), while rewinding, and for a stepped frame.
 *
 * Rewind. Every 4 frames, at the frame edge, a save state (runtime/
 * savestate.h) goes into a ring (timectl_ring.h: the newest in full, the
 * older ones as XOR deltas with their zero runs squeezed out), as many as
 * `rewind_seconds` (default 5, 0 is off) allow, and never more than about
 * 32 MB. Holding the rewind action loads them back, newest first, one
 * every other frame: about 2x speed backwards, silent. Letting go plays
 * on from the state it stopped at. The cartridge's own save RAM is
 * kept as it is (saves_after_state_load), as for a quick load, so
 * rewinding never undoes an in-game save. The ring starts over when the
 * frame count jumps (a state loaded, the menu's load). Refused with an on-screen
 * message while --record runs (as a quick load is): a rewound frame would
 * be written twice.
 *
 * Frame step. With the game paused (qol.h pause), one press of the step
 * action runs exactly one frame; not paused, it pauses first. */
#ifndef CT_TIMECTL_H
#define CT_TIMECTL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- the contract (frontend/quick menu, bindings) ---- */

/* Once at start-up with the other settings (registers slow_speed and
   rewind_seconds in the registry, hooks the rewind and frame step actions). */
void timectl_init(void);
int timectl_speed_pct(void);          /* 25, 50, 100, 200, 300, 400 */
void timectl_speed_set(int pct);      /* the nearest of those; saved in settings.ini */
void timectl_speed_step(int dir);     /* +1 next faster, -1 next slower; stops at the ends */
void timectl_rewind(int down);        /* hold: 1 starts, 0 resumes */
int timectl_rewinding(void);
void timectl_rewind_for(int frames);  /* hold for about `frames` frames of game time, then let go */
void timectl_frame_step(void);
int timectl_paused_step_pending(void);

/* ---- what main.c wires in ---- */

typedef struct {
    void (*audio_restart)(void);   /* no stale sound: stop playback, empty the queue */
    void (*pacing_reset)(void);    /* the frame timer starts over */
    void (*osd)(const char *msg);  /* an on-screen message */
    int (*paused)(void);           /* the game is paused (qol_paused) */
    void (*pause_toggle)(void);    /* qol_pause_toggle */
    void (*after_load)(void);      /* after a state is loaded: saves_after_state_load */
    int recording;                 /* --record is running: no rewind */
} timectl_host;
void timectl_set_host(const timectl_host *h);

/* After each sched_run_frame, between frames (with qol_after_frame): takes
   the capture when it is time. */
void timectl_after_frame(void);
/* Instead of a frame, once per frame time, while timectl_rewinding(). */
void timectl_rewind_tick(void);
/* The main loop took the pending step: it runs one frame now. */
void timectl_step_taken(void);
/* No sound: speed other than 100%, rewinding, or the frame being stepped. */
int timectl_audio_muted(void);
/* The pace in percent: the base speed times fast-forward's factor (1-4). */
int timectl_pace_pct(int ffwd);

/* ---- pure helpers (tests) ---- */

#define TIMECTL_FRAME_NS 16639267ull   /* 1 / 60.0988 Hz */
#define TIMECTL_CAPTURE_EVERY 4        /* frames between captures */
#define TIMECTL_REWIND_BYTES (32u << 20)
#define TIMECTL_REWIND_MAX_SECONDS 30
/* Timer ticks (of freq per second) one frame takes at pct percent. */
uint64_t timectl_frame_ticks(uint64_t freq, int pct);
/* Is frame f drawn at this pace (from 200%, one frame in pct/100)? */
int timectl_show_frame(int pct, long f);
/* Ring entries for rewind_seconds (0: none). */
int timectl_ring_entries(int seconds);

#ifdef __cplusplus
}
#endif

#endif
