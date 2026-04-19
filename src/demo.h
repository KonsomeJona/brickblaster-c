#pragma once
/* demo.h — BrickBlaster demo / attract mode
 *
 * Implements the demo_flag behavior from MAIN.ASM.
 *
 * ============================================================================
 * ASM DEMO FLAG (MAIN.ASM:85-105  @@demo: / 1159-1168 demo exit check)
 * ============================================================================
 *
 * Demo entry (MAIN.ASM:85-105  @@demo:):
 *   mov eax,1; call get_random; inc eax
 *   mov nbs_player,eax              ; 1 or 2 random players
 *   mov eax,1; call get_random; add eax,'0'
 *   mov world,al                    ; random world 0 or 1
 *   mov game_mode,PLAYING
 *   mov demo_counter,DELAI_DEMO     ; = 800 (Blaster.inc:420)
 *   mov demo_flag,On
 *   call new_game
 *
 * Demo idle check (MAIN.ASM:1159-1168  step 9 in main: loop):
 *   cmp computer_flag,On je @@cont
 *   cmp demo_flag,On     jne @@cont
 *   cmp test_flag,On     je @@cont
 *   call read_click; jnz @@exit     ; any input exits demo
 *   dec demo_counter
 *   cmp demo_counter,Off jne @@cont
 *   ret                             ; → back to intro (STATE_MENU)
 *
 * Demo paddle AI (MAIN.ASM:5131-5144  demo_move_x_player_1):
 *   cmp computer_flag,On je @@ok
 *   cmp demo_flag,On     jne @@ok
 *   mov eax,ball_1.sprite_pos_x     ; ball top-left X
 *   mov ebx,cursor_1.sprite_size_x  ; paddle width
 *   shr ebx,1                       ; ebx = paddle_w / 2
 *   sub eax,ebx                     ; eax = ball.x - paddle_w/2
 *   mov cursor_1.sprite_pos_x,eax   ; set paddle X directly (no clamping here)
 *   ret
 *
 * Translation to C top-left coordinates (game.c uses top-left for both):
 *   paddle.x = ball.x - paddle.w / 2
 *   Then paddle_clamp() is applied afterward to keep paddle in play area.
 *
 * Demo ball init (MAIN.ASM:2761-2771  called from @@demo / init_start_game):
 *   mov ball_1.sprite_sens_x,+3     ; vx = +3
 *   mov ball_1.sprite_sens_y,-4     ; vy = -4
 *   (already handled in main.c @@demo branch)
 *
 * Demo effects on game rules (DEMO_MODE_FIX.md):
 *   - No score counting (MAIN.ASM never adds score when demo_flag=On)
 *   - No powerup effects applied (powerup collect still plays sound but no state change)
 *   - Ball lost → respawn immediately (no life decrement in demo)
 *   - Any input → exit demo → return to menu
 *
 * ============================================================================
 * API
 * ============================================================================
 */

#include "game.h"

/*
 * demo_update_paddle — override paddle position with simple AI.
 *
 * Called each frame when game->demo_active == 1.
 * MAIN.ASM:5131-5144  demo_move_x_player_1:
 *   paddle.x = ball.x - paddle.w / 2
 *
 * Uses ball 0 (the primary ball). If no active ball, no-op.
 * After setting paddle.x, clamps to play area boundaries.
 * MOUSE.ASM:55-68  paddle clamp applied after every move.
 */
void demo_update_paddle(Game *g);

/*
 * demo_check_activate — check if demo should activate (called from menu idle).
 *
 * Returns 1 if demo_timer just expired and demo should start.
 * Returns 0 otherwise.
 *
 * Note: This is not used for the menu idle (handled in menu.c with idle_timer).
 * It is exposed here for external callers that want to trigger demo from
 * a custom idle counter (e.g., game screen idle).
 *
 * MAIN.ASM:282  general idle timer before demo starts.
 */
int demo_check_activate(Game *g);

/*
 * demo_handle_ball_lost — in demo mode, respawn ball instead of losing a life.
 *
 * MAIN.ASM demo mode: ball lost → reinit ball on paddle at fixed velocity.
 * No life decrement happens in demo (MAIN.ASM never calls detect_game_over
 * when demo_flag=On — the frame loop short-circuits via read_click exit).
 *
 * MAIN.ASM:2762-2763  sprite_sens_x=+3, sprite_sens_y=-4 (fixed demo velocity)
 */
void demo_handle_ball_lost(Game *g);

/*
 * demo_ai_player_2 — synthesise P2 input for computer-controlled P2.
 *
 * Called from main.c each frame (after frame_input_poll_p2) when
 * state.control_p2 == 0 and nbs_player > 1. Writes synthetic values into
 * the shared FrameInput (p2_left / p2_right / p2_stick_x / p2_fire) so
 * game.c's existing P2 input pipeline moves paddle_2 and fires lasers
 * / launches magnetic balls without any new branches in game.c.
 *
 * MAIN.ASM:5131-5144  demo_move_x_player_1 (implicit P2 twin at cursor_2).
 *
 * Behaviour:
 *   - Prefer a ball owned by P2 (ball.owner == 1); fallback to any active ball.
 *   - Steer paddle_2 toward tracked ball.x with a small deadzone to avoid jitter.
 *   - If paddle_2 has a magnetic ball attached, fire after a short delay.
 *   - If paddle_2.has_gun, fire lasers opportunistically.
 */
/* FrameInput typedef comes via game.h → input_frame.h */
void demo_ai_player_2(const Game *g, FrameInput *out);
