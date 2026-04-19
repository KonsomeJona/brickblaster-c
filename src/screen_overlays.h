#pragma once
/* screen_overlays.h — Ready/Pause/GameOver/Demo overlays
 * All overlay functions draw in canvas coordinates (640x480).
 * They must be called inside BeginTextureMode(dc.canvas)/EndTextureMode().
 *
 * Original uses FONTE bitmap font for all overlay text, rendered at
 * panel_info position (x=195, y=446, 270x22).
 *
 * MAIN.ASM:1261, 2757 ready screen
 * MAIN.ASM:1257-1276 pause
 * MAIN.ASM:4666-4695 game over
 * MAIN.ASM:6997      demo overlay
 */

#include "screen_manager.h"
#include "constants.h"
#include "input_frame.h"
#include "font.h"
#include "assets.h"
#include <raylib.h>

/* Game over timing (MAIN.ASM:4666) */
#define GAME_OVER_DELAY_FRAMES 180  /* 3 seconds at 60 FPS */

/* panel_info position from Blaster.inc */
#define PANEL_INFO_POS_X  195
#define PANEL_INFO_POS_Y  446   /* = 458-12 */
#define PANEL_INFO_W      270
#define PANEL_INFO_H       22

/* Initialise the overlay font — call once after assets are loaded. */
void overlays_init(Assets *assets);

/* Draw "ready ?" overlay — FONTE at panel_info position. */
void draw_ready_screen(ScreenState *state);

/* Draw "game paused" overlay — FONTE at panel_info position. */
void draw_pause_screen(ScreenState *state);

/* Draw game over overlay — "game over" via FONTE.
 * If game_mode==2 (dual): also shows "player 1/2 wins". */
void draw_game_over_screen(ScreenState *state, int *timer,
                           int game_mode, int winner);

/* Draw "demo" text overlay — FONTE. MAIN.ASM:6997 */
void draw_demo_overlay(void);

/* Draw "play again" overlay — flashed briefly after losing a life before
 * the ready overlay takes over (MAIN.ASM:4698 option_text_again). */
void draw_play_again_screen(ScreenState *state);

/* Handle resume/exit input on the pause screen.
 * Returns: 0=nothing, 1=resume requested.
 * EXIT sets state->game_mode directly. */
int pause_handle_input(ScreenState *state, const FrameInput *input);

#if defined(BRICKBLASTER_MOBILE)
int is_screen_too_small(void);
void draw_unfold_screen(void);
#endif
