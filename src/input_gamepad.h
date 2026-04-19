#pragma once
/* input_gamepad.h — Gamepad / Xbox controller input helpers
 *
 * Wraps Raylib gamepad API with deadzone handling and convenience functions.
 * Supports any XInput-compatible controller (Xbox, generic USB, PlayStation).
 *
 * Original ASM: Blaster.inc:21  JOYSTICK = 32
 */

#include "raylib.h"

/* Analog stick deadzone — ignore small movements */
#define GAMEPAD_DEADZONE 0.3f

/* Always use gamepad 0 (first connected controller) */
#define GP 0

/* --- Held (continuous) directional input for gameplay --- */

static inline int gamepad_left_held(void) {
    if (!IsGamepadAvailable(GP)) return 0;
    return IsGamepadButtonDown(GP, GAMEPAD_BUTTON_LEFT_FACE_LEFT) ||
           GetGamepadAxisMovement(GP, GAMEPAD_AXIS_LEFT_X) < -GAMEPAD_DEADZONE;
}

static inline int gamepad_right_held(void) {
    if (!IsGamepadAvailable(GP)) return 0;
    return IsGamepadButtonDown(GP, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) ||
           GetGamepadAxisMovement(GP, GAMEPAD_AXIS_LEFT_X) > GAMEPAD_DEADZONE;
}

/* --- Pressed (edge-triggered) directional input for menus --- */

static inline int gamepad_up_pressed(void) {
    if (!IsGamepadAvailable(GP)) return 0;
    return IsGamepadButtonPressed(GP, GAMEPAD_BUTTON_LEFT_FACE_UP);
}

static inline int gamepad_down_pressed(void) {
    if (!IsGamepadAvailable(GP)) return 0;
    return IsGamepadButtonPressed(GP, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
}

static inline int gamepad_left_pressed(void) {
    if (!IsGamepadAvailable(GP)) return 0;
    return IsGamepadButtonPressed(GP, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
}

static inline int gamepad_right_pressed(void) {
    if (!IsGamepadAvailable(GP)) return 0;
    return IsGamepadButtonPressed(GP, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
}

/* --- Action buttons --- */

/* A button (Xbox) / Cross (PlayStation) — confirm / fire */
static inline int gamepad_confirm(void) {
    if (!IsGamepadAvailable(GP)) return 0;
    return IsGamepadButtonPressed(GP, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
}

/* B button (Xbox) / Circle (PlayStation) — back / cancel */
static inline int gamepad_back(void) {
    if (!IsGamepadAvailable(GP)) return 0;
    return IsGamepadButtonPressed(GP, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
}

/* Start button — pause / unpause */
static inline int gamepad_start(void) {
    if (!IsGamepadAvailable(GP)) return 0;
    return IsGamepadButtonPressed(GP, GAMEPAD_BUTTON_MIDDLE_RIGHT);
}

/* --- Analog stick --- */

/* Left stick X with deadzone applied. Returns -1.0 to 1.0, 0.0 in deadzone. */
static inline float gamepad_stick_x(void) {
    if (!IsGamepadAvailable(GP)) return 0.0f;
    float x = GetGamepadAxisMovement(GP, GAMEPAD_AXIS_LEFT_X);
    if (x > -GAMEPAD_DEADZONE && x < GAMEPAD_DEADZONE) return 0.0f;
    return x;
}

/* --- Utility --- */

/* Any gamepad button or stick movement — for demo exit detection */
static inline int gamepad_any_input(void) {
    if (!IsGamepadAvailable(GP)) return 0;
    for (int b = 0; b <= GAMEPAD_BUTTON_RIGHT_THUMB; b++) {
        if (IsGamepadButtonDown(GP, b)) return 1;
    }
    float lx = GetGamepadAxisMovement(GP, GAMEPAD_AXIS_LEFT_X);
    float ly = GetGamepadAxisMovement(GP, GAMEPAD_AXIS_LEFT_Y);
    if (lx > GAMEPAD_DEADZONE || lx < -GAMEPAD_DEADZONE) return 1;
    if (ly > GAMEPAD_DEADZONE || ly < -GAMEPAD_DEADZONE) return 1;
    return 0;
}
