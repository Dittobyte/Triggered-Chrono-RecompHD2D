#include "input.h"

#include <string.h>

#include "settings.h"

/* port: controller options (input.h): the layout swaps and the stick's
   dead zone. 49% of full deflection is the 16000 / 32767 the code
   always used. */
static int swap_ab, swap_xy, deadzone_pct = INPUT_DEADZONE_DEFAULT;

void input_swap_set(int ab, int xy)
{
    swap_ab = ab != 0;
    swap_xy = xy != 0;
}

void input_swap_get(int *ab, int *xy)
{
    if (ab)
        *ab = swap_ab;
    if (xy)
        *xy = swap_xy;
}

void input_deadzone_set(int pct)
{
    deadzone_pct = pct < INPUT_DEADZONE_MIN ? INPUT_DEADZONE_MIN
                   : pct > INPUT_DEADZONE_MAX ? INPUT_DEADZONE_MAX : pct;
}

int input_deadzone_get(void)
{
    return deadzone_pct;
}

/* The stick's threshold, of 32767. */
static int deadzone_threshold(void)
{
    return deadzone_pct == INPUT_DEADZONE_DEFAULT ? 16000 : deadzone_pct * 32767 / 100;
}

static uint16_t swap_bits(uint16_t pad, uint16_t a, uint16_t b)
{
    uint16_t both = a | b, got = pad & both;
    if (got == a || got == b)
        pad ^= both;
    return pad;
}

uint16_t input_keyboard(const uint8_t *keys)
{
    static const struct { SDL_Scancode key; uint16_t bit; } map[] = {
        {SDL_SCANCODE_UP, PAD_UP},       {SDL_SCANCODE_DOWN, PAD_DOWN},
        {SDL_SCANCODE_LEFT, PAD_LEFT},   {SDL_SCANCODE_RIGHT, PAD_RIGHT},
        {SDL_SCANCODE_Z, PAD_B},         {SDL_SCANCODE_X, PAD_A},
        {SDL_SCANCODE_A, PAD_Y},         {SDL_SCANCODE_S, PAD_X},
        {SDL_SCANCODE_Q, PAD_L},         {SDL_SCANCODE_W, PAD_R},
        {SDL_SCANCODE_RETURN, PAD_START}, {SDL_SCANCODE_RSHIFT, PAD_SELECT},
    };
    uint16_t pad = 0;
    for (unsigned k = 0; k < sizeof map / sizeof map[0]; k++)
        if (keys[map[k].key])
            pad |= map[k].bit;
    return pad;
}

uint16_t input_controller(int (*button)(void *ctx, SDL_GameControllerButton b),
                          int (*axis)(void *ctx, SDL_GameControllerAxis a), void *ctx)
{
    static const struct { SDL_GameControllerButton b; uint16_t bit; } map[] = {
        {SDL_CONTROLLER_BUTTON_A, PAD_B},             /* south */
        {SDL_CONTROLLER_BUTTON_B, PAD_A},             /* east */
        {SDL_CONTROLLER_BUTTON_X, PAD_Y},             /* west */
        {SDL_CONTROLLER_BUTTON_Y, PAD_X},             /* north */
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, PAD_L},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, PAD_R},
        {SDL_CONTROLLER_BUTTON_START, PAD_START},
        {SDL_CONTROLLER_BUTTON_BACK, PAD_SELECT},
        {SDL_CONTROLLER_BUTTON_DPAD_UP, PAD_UP},
        {SDL_CONTROLLER_BUTTON_DPAD_DOWN, PAD_DOWN},
        {SDL_CONTROLLER_BUTTON_DPAD_LEFT, PAD_LEFT},
        {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, PAD_RIGHT},
    };
    uint16_t pad = 0;
    for (unsigned k = 0; k < sizeof map / sizeof map[0]; k++)
        if (button(ctx, map[k].b))
            pad |= map[k].bit;
    /* port: the layout swaps: the buttons the pad's face buttons make
       (not the keyboard's, not the touch controls'). */
    if (swap_ab)
        pad = swap_bits(pad, PAD_A, PAD_B);
    if (swap_xy)
        pad = swap_bits(pad, PAD_X, PAD_Y);
    const int dead = deadzone_threshold();   /* of 32767 */
    int x = axis(ctx, SDL_CONTROLLER_AXIS_LEFTX), y = axis(ctx, SDL_CONTROLLER_AXIS_LEFTY);
    if (x < -dead)
        pad |= PAD_LEFT;
    if (x > dead)
        pad |= PAD_RIGHT;
    if (y < -dead)
        pad |= PAD_UP;
    if (y > dead)
        pad |= PAD_DOWN;
    return pad;
}

void key_filter_init(key_filter *kf)
{
    memset(kf, 0, sizeof *kf);
    kf->last_button = -1000;
}

static int near_button(const key_filter *kf, long frame)
{
    long d = frame - kf->last_button;
    return kf->controller && d >= -KEY_FILTER_FRAMES && d <= KEY_FILTER_FRAMES;
}

void key_filter_key(key_filter *kf, SDL_Scancode sc, int down, long frame)
{
    if ((unsigned)sc >= SDL_NUM_SCANCODES)
        return;
    if (!down) {
        kf->held[sc] = kf->ignored[sc] = 0;
        return;
    }
    kf->pressed_at[sc] = frame;
    if (near_button(kf, frame)) {
        kf->ignored[sc] = 1;
        kf->held[sc] = 0;
    } else if (!kf->ignored[sc]) {
        kf->held[sc] = 1;
    }
}

void key_filter_button(key_filter *kf, long frame)
{
    kf->last_button = frame;
    if (!kf->controller)
        return;
    /* A key that arrived just before this button press is its duplicate. */
    for (int k = 0; k < SDL_NUM_SCANCODES; k++)
        if (kf->held[k] && frame - kf->pressed_at[k] <= KEY_FILTER_FRAMES) {
            kf->held[k] = 0;
            kf->ignored[k] = 1;
        }
}

/* port: the quit combo (input.h). */
enum { QC_IDLE, QC_HOLD, QC_ASK, QC_RELEASE };

void quit_combo_init(quit_combo *q)
{
    memset(q, 0, sizeof *q);
}

int quit_combo_update(quit_combo *q, long frame,
                      int (*button)(void *ctx, SDL_GameControllerButton b), void *ctx)
{
    int back = button(ctx, SDL_CONTROLLER_BUTTON_BACK) != 0;
    int start = button(ctx, SDL_CONTROLLER_BUTTON_START) != 0;
    int a = button(ctx, SDL_CONTROLLER_BUTTON_A) != 0;
    int b = button(ctx, SDL_CONTROLLER_BUTTON_B) != 0;
    int a_down = a && !q->prev_a, b_down = b && !q->prev_b;
    q->prev_a = a;
    q->prev_b = b;
    int r = QUIT_NONE;
    switch (q->state) {
    case QC_IDLE:
        if (back && start) {
            q->state = QC_HOLD;
            q->since = frame;
        }
        break;
    case QC_HOLD:
        if (!back || !start)
            q->state = QC_RELEASE;   /* let go early: nothing, once released */
        else if (frame - q->since >= QUIT_HOLD_FRAMES) {
            q->state = QC_ASK;
            q->since = frame;
            r = QUIT_ASK;
        }
        break;
    case QC_ASK:
        if (a_down) {
            q->state = QC_RELEASE;
            r = QUIT_YES;
        } else if (b_down || frame - q->since >= QUIT_ASK_FRAMES) {
            q->state = QC_RELEASE;
            r = QUIT_CANCEL;
        }
        break;
    default:   /* QC_RELEASE */
        if (!back && !start && !a && !b)
            q->state = QC_IDLE;
        break;
    }
    return r;
}

int quit_combo_masks(const quit_combo *q)
{
    return q->state != QC_IDLE;
}

/* port: pad_leds (settings.ini): "auto" lets a pad show its player
   number and colour; "off" turns the vendor drivers' player-LED hints
   off (SDL's default is on) and the game sets no colour. */
static int pad_leds_on = 1;

static void input_pad_leds_hints(void)
{
    const char *v = pad_leds_on ? "1" : "0";
#ifdef SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, v);
#endif
#ifdef SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED, v);
#endif
#ifdef SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_PLAYER_LED
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_PLAYER_LED, v);
#endif
}

int input_pad_leds(void)
{
    return pad_leds_on;
}

/* port: controller hints (input.h). SDL_SetHint at normal priority: an
   environment variable of the same name beats it. */
void input_sdl_hints(int native_steam)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, "1");
#ifndef __ANDROID__
    if (native_steam)
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1");
#else
    (void)native_steam;
#endif
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    /* Joy-Cons: a left and a right one are one pad; a single one is not
       turned on its side. Each hint is missing from older SDL 2 headers. */
#ifdef SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS, "1");
#endif
#ifdef SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS, "0");
#endif
    input_pad_leds_hints();
}

int input_under_steam_input(const char **why)
{
    static const char *const vars[] = {"SteamVirtualGamepadInfo",
                                       "SDL_GAMECONTROLLER_IGNORE_DEVICES"};
    for (unsigned k = 0; k < sizeof vars / sizeof *vars; k++) {
        const char *v = SDL_getenv(vars[k]);
        if (v && *v) {
            if (why)
                *why = vars[k];
            return 1;
        }
    }
    if (why)
        *why = NULL;
    return 0;
}

/* port: gyro aiming (input.h). */
static float clampf(float v, float m)
{
    return v > m ? m : v < -m ? -m : v;
}

void gyro_cam_step(gyro_cam *gc, const float rate[3], float dt, float strength)
{
    const float dead = 0.03f;   /* rad/s: sensor noise and a resting hand */
    const float deg = 57.2957795f;
    float rx = rate[0], ry = rate[1];
    rx = rx > dead ? rx - dead : rx < -dead ? rx + dead : 0.0f;
    ry = ry > dead ? ry - dead : ry < -dead ? ry + dead : 0.0f;
    float keep = 1.0f - dt * 1.5f;   /* back toward the preset's view, ~0.7 s */
    if (keep < 0.0f)
        keep = 0.0f;
    gc->tilt = clampf((gc->tilt + rx * dt * deg * strength) * keep, GYRO_MAX_DEG);
    gc->yaw = clampf((gc->yaw + ry * dt * deg * strength) * keep, GYRO_MAX_DEG);
}

/* port: the settings registry rows for the above (settings.ini
   swap_ab, swap_xy, stick_deadzone). */
static int get_swap_ab(void *u) { (void)u; return swap_ab; }
static void set_swap_ab(void *u, int v) { (void)u; swap_ab = v != 0; }
static int get_swap_xy(void *u) { (void)u; return swap_xy; }
static void set_swap_xy(void *u, int v) { (void)u; swap_xy = v != 0; }
static int get_deadzone(void *u) { (void)u; return deadzone_pct; }
static void set_deadzone(void *u, int v) { (void)u; input_deadzone_set(v); }
static int get_pad_leds(void *u) { (void)u; return pad_leds_on; }
static void set_pad_leds(void *u, int v)
{
    (void)u;
    pad_leds_on = v != 0;
    input_pad_leds_hints();   /* for pads opened from now on */
}

void input_register_settings(void)
{
    settings_register(&(setting){
        .key = "swap_ab", .label = "Swap A and B", .category = "Controls", .type = SETTING_BOOL,
        .get = get_swap_ab, .set = set_swap_ab,
        .help = "The controller's east and south buttons trade places in the game. The "
                "keyboard and the touch controls stay as they are."});
    settings_register(&(setting){
        .key = "swap_xy", .label = "Swap X and Y", .category = "Controls", .type = SETTING_BOOL,
        .get = get_swap_xy, .set = set_swap_xy,
        .help = "The controller's north and west buttons trade places in the game."});
    settings_register(&(setting){
        .key = "stick_deadzone", .label = "Stick dead zone", .category = "Controls",
        .type = SETTING_INT, .min = INPUT_DEADZONE_MIN, .max = INPUT_DEADZONE_MAX, .step = 1,
        .format = "%d%%", .get = get_deadzone, .set = set_deadzone,
        .help = "How far the left stick must tilt to walk. Lower: more sensitive."});
    static const char *const led_opts[] = {"Off", "Auto"}, *const led_vals[] = {"off", "auto"};
    settings_register(&(setting){
        .key = "pad_leds", .label = "Controller lights", .category = "Controls",
        .type = SETTING_ENUM, .options = led_opts, .values = led_vals, .count = 2,
        .get = get_pad_leds, .set = set_pad_leds,
        .help = "Auto: a PlayStation, Switch or Xbox 360 pad shows its player number, and a pad "
                "with a light bar takes a colour per player. Off: the game leaves the lights "
                "alone."});
}
