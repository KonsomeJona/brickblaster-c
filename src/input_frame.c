/* input_frame.c — Platform-aware input polling
 *
 * Consolidates ALL platform-specific input into one file.
 * game.c reads only the FrameInput struct — zero #ifdefs, zero raylib calls.
 *
 * Call frame_input_poll() exactly once per frame, before game_update().
 */

#include "input_frame.h"
#include "letterbox.h"
#include "input_gamepad.h"
#include "constants.h"
#include <raylib.h>
#include <string.h>  /* memset */

#if defined(PLATFORM_ANDROID)
#include "input_wear.h"
#endif
#if defined(BRICKBLASTER_MOBILE)
#include "mobile_controls.h"
#include "input_tilt.h"
#endif

/* Speed multiplier tables: index 0=VeryLow, 1=Low, 2=Med, 3=High, 4=VeryHigh */
static const float s_btn_speed[] = { 0.6f, 1.0f, 1.4f, 2.0f, 2.8f };
static const float s_tilt_speed[] = { 0.4f, 0.8f, 1.5f, 2.5f, 3.5f };

void frame_input_poll(FrameInput *out, int drag_enabled, int tilt_enabled,
                      int button_speed, int tilt_speed,
                      int pause_button_hit, int in_game)
{
    memset(out, 0, sizeof(*out));

    /* Speed multipliers from settings (1-5 → array index 0-4) */
    {
        int bi = button_speed - 1;
        int ti = tilt_speed - 1;
        if (bi < 0 || bi > 4) bi = 2;  /* default Medium */
        if (ti < 0 || ti > 4) ti = 2;
        out->button_speed_mul = s_btn_speed[bi];
        out->tilt_speed_mul   = s_tilt_speed[ti];
    }

    /* Cache raylib input queries that should only be read once per frame. */
    Vector2 mouse_delta = GetMouseDelta();
    int touch_count = GetTouchPointCount();

    /* === Dev toggle === */
    out->dev_f9_pressed = IsKeyPressed(KEY_F9);

    /* === Keyboard: directional === */
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  out->move_left  = 1;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) out->move_right = 1;

    /* === Gamepad: directional + analog === */
    if (gamepad_left_held())  out->move_left  = 1;
    if (gamepad_right_held()) out->move_right = 1;
    out->stick_x = gamepad_stick_x();

    /* === Click: centralised left-button read (once per frame) === */
    {
        out->click_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (out->click_pressed) {
            Vector2 mp = GetMousePosition();
            out->click_screen_x = mp.x;
            out->click_screen_y = mp.y;
        }
    }

    /* === Fire: keyboard + mouse + gamepad === */
    out->fire_pressed = IsKeyPressed(KEY_SPACE) || gamepad_confirm();

#if defined(BRICKBLASTER_MOBILE)
    /* --- Mobile touch buttons --- */

    /* Directional: on-screen left/right buttons (always active) */
    if (mobile_left_down())  out->move_left  = 1;
    if (mobile_right_down()) out->move_right = 1;

    /* Fire: mobile fire button + game-area tap */
    if (mobile_fire_pressed()) out->fire_pressed = 1;
    if (mobile_game_tap_pressed()) out->fire_pressed = 1;

    /* Pointer snap (finger drag) */
    if (drag_enabled) {
        Letterbox lb = letterbox_get(GetScreenWidth(), GetScreenHeight());
        int tc = touch_count;
        for (int i = 0; i < tc; i++) {
            Vector2 tp = GetTouchPosition(i);
            Vector2 cp = letterbox_to_canvas(lb, tp);
            /* Skip touches outside the canvas area (letterbox/black bars) */
            if (cp.x < 0 || cp.x > SCREEN_W || cp.y < 0 || cp.y > SCREEN_H)
                continue;
            if (!mobile_controls_is_control_area(cp)) {
                /* Convert to game X coordinate */
                out->pointer_active = 1;
                out->pointer_game_x = cp.x;
                break;
            }
        }
    }

    /* Accelerometer tilt */
    if (tilt_enabled) {
        out->tilt_x = tilt_consume_x();
    }

#elif defined(PLATFORM_ANDROID)
    /* --- Wear OS crown --- */
    out->crown_delta = wear_consume_rotary_delta();
    if (wear_consume_crown_press()) out->fire_pressed = 1;

    /* Touch on Wear OS = pointer */
    if (touch_count > 0) {
        Vector2 tp = GetTouchPosition(0);
        Vector2 cp = letterbox_screen_to_canvas(tp);
        out->pointer_active = 1;
        out->pointer_game_x = cp.x;
    }

#else
    /* --- Desktop / Web --- */

    /* On web, touch events don't trigger IsMouseButtonPressed() because
     * raylib's touch handler calls preventDefault(), suppressing mouse
     * synthesis.  Edge-detect touch start (0→N transition) instead. */
    {
        static int prev_tc = 0;
        if (touch_count > 0 && prev_tc == 0) {
            Vector2 tp = GetTouchPosition(0);
            out->click_pressed = 1;
            out->click_screen_x = tp.x;
            out->click_screen_y = tp.y;
        }
        prev_tc = touch_count;
    }

    /* Mouse click = fire only during gameplay. */
    if (in_game && out->click_pressed)
        out->fire_pressed = 1;

    /* Pointer: touch first, then mouse delta */
    if (touch_count > 0) {
        Vector2 tp = GetTouchPosition(0);
        Vector2 cp = letterbox_screen_to_canvas(tp);
        out->pointer_active = 1;
        out->pointer_game_x = cp.x;
    } else {
        if (mouse_delta.x != 0.0f || mouse_delta.y != 0.0f) {
            Vector2 mp = { (float)GetMouseX(), (float)GetMouseY() };
            Vector2 cp = letterbox_screen_to_canvas(mp);
            out->pointer_active = 1;
            out->pointer_game_x = cp.x;
        }
    }
#endif

    /* === Pause === */
    out->pause_pressed = IsKeyPressed(KEY_P) || gamepad_start() || pause_button_hit;

    /* P2 defaults — filled on demand by frame_input_poll_p2(). */
    out->p2_left = 0;
    out->p2_right = 0;
    out->p2_stick_x = 0.0f;
    out->p2_fire = 0;

    /* === Any input (for demo exit) === */
    out->any_input =
        out->move_left || out->move_right ||
        out->fire_pressed || out->pause_pressed ||
        out->pointer_active || out->dev_f9_pressed ||
        (out->stick_x != 0.0f) ||
        (out->tilt_x != 0.0f) ||
        (out->crown_delta > 0.05f || out->crown_delta < -0.05f) ||
        IsKeyDown(KEY_SPACE) ||
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
        (mouse_delta.x != 0.0f || mouse_delta.y != 0.0f) ||
        (touch_count > 0) ||
        gamepad_any_input();
}

/* MOUSE.ASM:420-433 wait_keyboard_release + MAIN.ASM:266-270 wait_click loop.
 * ASM pattern:
 *   @@wait: call wait_synchro; call read_click; jnz @@wait
 *           call wait_keyboard_release
 * We spin on PollInputEvents + a 16ms pace (60Hz vsync equivalent) until
 * no input is currently held. Prevents a menu-button hold from being seen
 * as a fresh click by the next screen's read_click. */
void input_wait_click_release(void) {
    const double TIMEOUT_SEC = 2.0;  /* safety cap — match ASM max fade */
    double start = GetTime();
    for (;;) {
        PollInputEvents();
        int held = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
                   IsKeyDown(KEY_SPACE) ||
                   IsKeyDown(KEY_ENTER) ||
                   IsKeyDown(KEY_KP_ENTER) ||
                   IsKeyDown(KEY_LEFT_CONTROL) ||
                   IsKeyDown(KEY_RIGHT_CONTROL) ||
                   gamepad_any_input();
        if (!held) break;
        if (GetTime() - start > TIMEOUT_SEC) break;
        WaitTime(0.016);  /* ~60Hz pace — matches ASM wait_synchro */
    }
}

void frame_input_poll_p2(FrameInput *out, int mode) {
    if (!out) return;
    out->p2_left = 0; out->p2_right = 0;
    out->p2_stick_x = 0.0f; out->p2_fire = 0;

    if (mode == 1) {          /* keyboard */
        out->p2_left  = IsKeyDown(KEY_Q) || IsKeyDown(KEY_A);
        out->p2_right = IsKeyDown(KEY_D);
        out->p2_fire  = IsKeyPressed(KEY_F);
    } else if (mode == 2) {   /* joystick gamepad 1 */
        if (IsGamepadAvailable(1)) {
            out->p2_left  = IsGamepadButtonDown(1, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
            out->p2_right = IsGamepadButtonDown(1, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
            float sx = GetGamepadAxisMovement(1, GAMEPAD_AXIS_LEFT_X);
            if (sx > -GAMEPAD_DEADZONE && sx < GAMEPAD_DEADZONE) sx = 0.0f;
            out->p2_stick_x = sx;
            out->p2_fire = IsGamepadButtonPressed(1, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        }
    }
    /* mode 0 = computer (AI), leave all zero here — main.c calls
     * demo_ai_player_2() after this to fill p2_* fields when control_p2 == 0.
     * See demo.c:demo_ai_player_2 (MAIN.ASM:5131-5144 demo_move_x_player_*). */
}
