#pragma once
/* mobile_controls.h — On-screen touch buttons for the mobile (Fold/tablet) build.
 * Buttons are drawn and hit-tested in canvas coordinates (640x480).
 * Guard all usage with #if defined(BRICKBLASTER_MOBILE).
 */
#if defined(BRICKBLASTER_MOBILE)

#include <raylib.h>

/* Sensitivity levels stored in ScreenState.button_speed / tilt_speed */
#define SENS_VERY_LOW  1
#define SENS_LOW       2
#define SENS_MED       3
#define SENS_HIGH      4
#define SENS_VERY_HIGH 5
#define SENS_COUNT     5

/* Button layout in canvas space (640x480).
 * Left/right: rectangular buttons at bottom of side margins.
 * Fire: circular button, centered above the right button on right margin. */
#define MC_LEFT_X    0
#define MC_LEFT_Y    290
#define MC_LEFT_W    112     /* = PLAY_X1, covers full left margin */
#define MC_LEFT_H    190
#define MC_RIGHT_X   528     /* = PLAY_X2 */
#define MC_RIGHT_Y   290
#define MC_RIGHT_W   112     /* = SCREEN_W - PLAY_X2, covers full right margin */
#define MC_RIGHT_H   190
/* Fire circle: center + radius */
#define MC_FIRE_CX   585
#define MC_FIRE_CY   215
#define MC_FIRE_R     50

/* Call once per frame (before reading any state). */
void mobile_controls_update(void);

/* Draw semi-transparent control buttons — call inside BeginTextureMode(canvas).
 * fire_label:   text for the fire/action button (e.g. "FIRE", "OK").
 * fire_enabled: 1=active (orange), 0=disabled (grayed out).
 * L/R buttons are always shown. */
void mobile_controls_draw(const char *fire_label, int fire_enabled);

/* Returns 1 if canvas_pos falls inside any control button (used to exclude
 * those touches from the normal paddle-snap logic). */
int mobile_controls_is_control_area(Vector2 canvas_pos);

/* Held state — use for paddle movement each frame. */
int mobile_left_down(void);
int mobile_right_down(void);

/* Edge-triggered (fires on first frame of press only).
 * Use for fire/launch and for letter cycling in name entry. */
int mobile_fire_pressed(void);
int mobile_left_pressed(void);
int mobile_right_pressed(void);

/* Returns 1 if a new touch appeared anywhere OUTSIDE all control buttons.
 * Used so tapping the play area during READY? state launches the ball. */
int mobile_game_tap_pressed(void);

#endif /* BRICKBLASTER_MOBILE */
