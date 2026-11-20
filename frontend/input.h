/* Keyboard and game controller state -> SNES pad bits (sched_set_joypad
 * layout). Pure functions over SDL state, so they can be tested without a
 * window. */
#ifndef CT_INPUT_H
#define CT_INPUT_H

#include <stdint.h>

#include <SDL.h>

#define PAD_B      0x8000
#define PAD_Y      0x4000
#define PAD_SELECT 0x2000
#define PAD_START  0x1000
#define PAD_UP     0x0800
#define PAD_DOWN   0x0400
#define PAD_LEFT   0x0200
#define PAD_RIGHT  0x0100
#define PAD_A      0x0080
#define PAD_X      0x0040
#define PAD_L      0x0020
#define PAD_R      0x0010

/* keys: held state per scancode (key_filter.held). Arrows, Z=B X=A A=Y S=X Q=L W=R,
   Enter=Start, Right Shift=Select. */
uint16_t input_keyboard(const uint8_t *keys);

/* button(b) / axis(a): SDL_GameControllerGetButton/GetAxis for one pad.
   Face buttons by position (south=B, east=A, west=Y, north=X, as on the
   SNES), shoulders L/R, Start, Back=Select, d-pad and left stick. */
uint16_t input_controller(int (*button)(void *ctx, SDL_GameControllerButton b),
                          int (*axis)(void *ctx, SDL_GameControllerAxis a), void *ctx);

/* port: the controller's layout and stick options (settings.ini swap_ab,
   swap_xy, stick_deadzone; the Controls tab). swap: the SNES A and B (X
   and Y) the controller's east and south (north and west) buttons make
   trade places; input_keyboard and the touch controls are not affected.
   deadzone: percent of full tilt the left stick must pass to press the
   d-pad (INPUT_DEADZONE_DEFAULT is what the code always used). */
#define INPUT_DEADZONE_MIN 5
#define INPUT_DEADZONE_MAX 90
#define INPUT_DEADZONE_DEFAULT 49
void input_swap_set(int ab, int xy);
void input_swap_get(int *ab, int *xy);
void input_deadzone_set(int pct);
int input_deadzone_get(void);
/* Register the three settings rows (settings.h). */
void input_register_settings(void);

/* Held keys, built from key events. While a game controller is active, a
   key press within KEY_FILTER_FRAMES of a controller button press (either
   order) is ignored until that key is released: Steam Input's desktop
   layout sends a keyboard key along with some controller buttons (B ->
   Escape, A -> Return), which would otherwise press a second SNES button. */
#define KEY_FILTER_FRAMES 2

typedef struct {
    uint8_t held[SDL_NUM_SCANCODES];      /* for input_keyboard() */
    uint8_t ignored[SDL_NUM_SCANCODES];   /* pressed, filtered until released */
    long pressed_at[SDL_NUM_SCANCODES];
    long last_button;                     /* frame of the last controller press */
    int controller;                       /* a game controller is active */
} key_filter;

void key_filter_init(key_filter *kf);
void key_filter_key(key_filter *kf, SDL_Scancode sc, int down, long frame);
void key_filter_button(key_filter *kf, long frame);

/* port: the controller's quit combo (docs/HANDHELD.md), for handhelds
   without a keyboard. Hold View (Back) and Menu (Start) together for
   QUIT_HOLD_FRAMES: the frontend asks "quit?"; then A quits, B (or
   QUIT_ASK_FRAMES with no answer) goes back to the game. While both are
   held, while the question is up and until A, B, View and Menu are all
   released after it, the pad's buttons are kept from the game, so the
   combo opens no menu and the answer presses nothing. */
#define QUIT_HOLD_FRAMES 120   /* 2 s */
#define QUIT_ASK_FRAMES 600    /* 10 s */

enum { QUIT_NONE, QUIT_ASK, QUIT_CANCEL, QUIT_YES };

typedef struct {
    int state;        /* idle, holding, asking, releasing */
    long since;       /* frame the hold or the question started */
    int prev_a, prev_b;
} quit_combo;

void quit_combo_init(quit_combo *q);
/* Once per frame with the pad's buttons. Returns QUIT_ASK on the frame the
   question should appear, QUIT_YES / QUIT_CANCEL on the answer, else
   QUIT_NONE. */
int quit_combo_update(quit_combo *q, long frame,
                      int (*button)(void *ctx, SDL_GameControllerButton b), void *ctx);
/* The controller's input should not reach the game this frame. */
int quit_combo_masks(const quit_combo *q);

/* port: SDL hints for controllers, set before SDL_Init (docs/HANDHELD.md):
   - SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK=1: the Steam Deck's built-in
     controller through SDL's own driver, back buttons (paddle1-4),
     trackpads and gyro included, when Steam Input isn't between them;
   - SDL_HINT_JOYSTICK_HIDAPI_STEAM=1 (when native_steam): the Steam
     Controller likewise (off by default in SDL 2; SDL 3 also drives the
     2026 Steam Controller with it). Needs read access to /dev/hidraw*
     (Steam's udev rules). Not on Android, where it means a Bluetooth
     permission prompt;
   - SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS=0: face buttons by
     position on Nintendo pads too, as input_controller assumes;
   - SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS=1 and ..._VERTICAL_JOY_CONS=0:
     a left and a right Joy-Con are one pad, a single one is used as it is;
   - the PS5, Switch and Xbox 360 player-LED hints, on, or off with
     settings.ini pad_leds=off. Each hint is guarded by #ifdef: the SDL
     2.30 headers on CI may lack one.
   A hint already in the environment wins. Under Steam Input, Steam hides
   the physical pads from SDL (SDL_GAMECONTROLLER_IGNORE_DEVICES): the
   virtual pad is used as before. */
void input_sdl_hints(int native_steam);
/* settings.ini pad_leds: 1 (auto, the default) or 0 (off). */
int input_pad_leds(void);
/* Tell a pad which player it is (0..2): SDL_GameControllerSetPlayerIndex,
   and a fixed light colour (P1 blue, P2 red, P3 green) if it has a light.
   Does nothing when pad_leds=off or pad is NULL. Call again when a pad
   changes slot. */
void input_pad_extras_apply(SDL_GameController *pad, int player);
/* Steam Input is giving this process its virtual pad instead of the real
   ones (Steam's environment variables); why gets the variable. */
int input_under_steam_input(const char **why);

/* port: gyro aiming for the 3D diorama's camera (optional, settings.ini
   gyro=on; docs/HANDHELD.md). Turning the pad tilts (pitch) and turns
   (yaw) the camera a little; it eases back to the preset's view when the
   pad is still. tilt and yaw in degrees, within +-GYRO_MAX_DEG. */
#define GYRO_MAX_DEG 8.0f

typedef struct {
    float tilt, yaw;
} gyro_cam;

/* rate: SDL_SENSOR_GYRO data (rad/s about x = pitch, y = yaw, z = roll),
   dt: seconds since the last step, strength: 1 = a degree of camera per
   degree turned. */
void gyro_cam_step(gyro_cam *gc, const float rate[3], float dt, float strength);

#endif
