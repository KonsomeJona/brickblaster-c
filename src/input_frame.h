#pragma once
/* input_frame.h — Platform-agnostic input state for one frame
 *
 * Filled once per frame by frame_input_poll() in input_frame.c,
 * then passed to game_update() as const FrameInput*.
 * game.c reads ONLY this struct — no raylib input calls, no platform ifdefs.
 */

typedef struct {
    /* Paddle movement — discrete direction (keyboard, D-pad, mobile buttons) */
    int   move_left;        /* 1 = left held this frame */
    int   move_right;       /* 1 = right held this frame */
    float stick_x;          /* -1.0..1.0 analog stick (gamepad), 0 in deadzone */
    float crown_delta;      /* Wear OS rotary encoder delta (0 on other platforms) */
    float tilt_x;           /* -1.0..1.0 accelerometer tilt (mobile TILT mode, 0 elsewhere) */
    float button_speed_mul; /* pad button speed multiplier (1.0=low, 1.4=med, 2.0=high) */
    float tilt_speed_mul;   /* tilt sensitivity multiplier (0.8=low, 1.5=med, 2.5=high) */

    /* Pointer (mouse/touch) — already in GAME coordinates (post-letterbox) */
    int   pointer_active;   /* 1 = pointer moved/touched this frame */
    float pointer_game_x;   /* game-space X (0..640), valid only if pointer_active */

    /* Fire (edge-triggered) */
    int   fire_pressed;     /* 1 = fire/launch was pressed this frame */

    /* Click (edge-triggered) — centralised mouse/touch click for menus */
    int   click_pressed;    /* 1 = left mouse button pressed this frame */

    /* Screen-space click/tap position (valid when click_pressed == 1).
     * On desktop: mouse position.  On web touch: last touch position. */
    float click_screen_x;
    float click_screen_y;

    /* Pause (edge-triggered) */
    int   pause_pressed;    /* 1 = pause toggle triggered this frame */

    /* Any input (for demo exit) */
    int   any_input;        /* 1 = any key/button/touch/mouse-move detected */

    /* Dev */
    int   dev_f9_pressed;   /* 1 = F9 pressed (dev test toggle) */

    /* Player 2 input (multiplayer).
     * Source chosen in menu 3 (ctrl player 2): computer/keyboard/joystick.
     * Filled by frame_input_poll_p2() when nbs_player > 1. */
    int   p2_left;          /* ZQSD Q or gamepad1 left */
    int   p2_right;         /* ZQSD D or gamepad1 right */
    float p2_stick_x;       /* gamepad1 stick */
    int   p2_fire;          /* F key or gamepad1 A */
} FrameInput;

/* Poll all input sources and fill the FrameInput struct.
 * Must be called exactly once per frame, before game_update().
 *
 * drag_enabled: 1=finger drag moves paddle (mobile)
 * tilt_enabled: 1=accelerometer tilt moves paddle (mobile)
 * pause_button_hit: 1 if the pause button was tapped (computed by main.c)
 * in_game: 1 if in gameplay state (mouse click = fire); 0 during menus */
void frame_input_poll(FrameInput *out, int drag_enabled, int tilt_enabled,
                      int button_speed, int tilt_speed,
                      int pause_button_hit, int in_game);

/* Poll P2 input based on control mode.
 * mode: 0=computer (zero input here; main.c calls demo_ai_player_2() after
 *       this to fill p2_* fields), 1=keyboard (ZQSD+F),
 *       2=joystick (gamepad 1 stick + A). */
void frame_input_poll_p2(FrameInput *out, int mode);

/* MOUSE.ASM:420-433 wait_keyboard_release + MAIN.ASM:266-270 wait_click loop.
 * Busy-wait (vsync-paced) until mouse button, all gamepad buttons, and the
 * common keys (space/enter/ctrl) are released. Used on screen transitions
 * after a button click so the incoming screen doesn't read the still-held
 * input as a fresh click. Mirrors ASM pattern at get_menu entry (:269). */
void input_wait_click_release(void);
