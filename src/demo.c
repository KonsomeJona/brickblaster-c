/* demo.c — BrickBlaster demo / attract mode
 *
 * All logic derived from MAIN.ASM demo_flag behavior.
 *
 * MAIN.ASM key references:
 *   @@demo entry:          MAIN.ASM:85-105
 *   demo_move_x_player_1:  MAIN.ASM:5131-5144
 *   demo exit check:       MAIN.ASM:1159-1168
 *   demo ball init:        MAIN.ASM:2761-2771
 *   DELAI_DEMO:            Blaster.inc:420  = 800
 */

#include "demo.h"
#include "paddle.h"
#include "ball.h"
#include <string.h>  /* memset */

#include "raylib.h"  /* IsKeyDown, IsMouseButtonPressed, GetMouseDelta */

/* --------------------------------------------------------------------------
 * demo_update_paddle
 *
 * Simple AI: set paddle X so paddle centre tracks ball X.
 *
 * MAIN.ASM:5131-5144  demo_move_x_player_1:
 *   cmp computer_flag,On je @@ok      ; if computer player, skip AI (handled elsewhere)
 *   cmp demo_flag,On     jne @@ok     ; guard: only run in demo mode
 *   mov eax,ball_1.sprite_pos_x       ; ball top-left X (ASM uses top-left coords)
 *   mov ebx,cursor_1.sprite_size_x    ; paddle width
 *   shr ebx,1                         ; ebx = paddle_w / 2
 *   sub eax,ebx                       ; paddle_x = ball_x - (paddle_w / 2)
 *   mov cursor_1.sprite_pos_x,eax     ; assign directly — NO clamping in ASM here
 * @@ok:
 *   ret
 *
 * Clamping: MOUSE.ASM:55-68  clamp applied after each move.  We call
 * paddle_clamp() after the assignment to match that flow.
 *
 * DEMO_MODE_FIX.md Priority 2 — paddle should follow ball CENTER:
 *   ASM top-left: paddle.x = ball.x - paddle_w/2  → paddle centre = ball.x
 *   In C (top-left coords): same formula applies directly.
 * -------------------------------------------------------------------------- */
void demo_update_paddle(Game *g) {
    int ball_x;
    int i;
    Ball *active_ball = NULL;

    /* Guard: only run when demo is active.
     * MAIN.ASM:5136  cmp demo_flag,On jne @@ok */
    if (!g->demo_active) return;

    /* Find first active ball — MAIN.ASM uses ball_1 (slot 0).
     * MAIN.ASM:5138  mov eax,ball_1.sprite_pos_x */
    for (i = 0; i < g->ball_count; i++) {
        if (g->balls[i].active && !g->balls[i].is_magnetic) {
            active_ball = &g->balls[i];
            break;
        }
    }

    if (!active_ball) return;  /* no active ball to track */

    /* MAIN.ASM:5138-5142  paddle_x = ball_x - (paddle_w / 2)
     * This places paddle centre under ball top-left X, which tracks the ball. */
    ball_x = active_ball->x;
    g->paddle.x = ball_x - g->paddle.w / 2;

    /* Apply play area clamp — MOUSE.ASM:55-68  after move, clamp to [PLAY_X1..PLAY_X2-w] */
    paddle_clamp(&g->paddle);
}

/* --------------------------------------------------------------------------
 * demo_check_activate
 *
 * Check if demo_timer has expired.
 *
 * MAIN.ASM:1165-1168:
 *   dec demo_counter
 *   cmp demo_counter,Off  ; Off = 0
 *   jne @@cont
 *   ret                   ; → back to intro (triggers @@demo)
 *
 * Returns 1 if game->demo_timer has just reached 0.
 * Returns 0 otherwise (timer still running or already 0).
 *
 * This is called in game_update() STEP 9 when demo_active == 1.
 * When it returns 1, caller should set game->state = STATE_MENU.
 * -------------------------------------------------------------------------- */
int demo_check_activate(Game *g) {
    if (!g->demo_active) return 0;

    /* MAIN.ASM:1165  dec demo_counter */
    g->demo_timer--;

    /* MAIN.ASM:1167  cmp demo_counter,Off jne @@cont */
    if (g->demo_timer <= 0) {
        /* MAIN.ASM:1168  ret → returns to intro loop → jmp intro → @@demo or new menu */
        g->demo_timer  = DELAI_DEMO;   /* reset for next demo run */
        g->demo_active = 0;
        return 1;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * demo_handle_ball_lost
 *
 * In demo mode, when all balls are lost, respawn immediately with the
 * same fixed demo velocity instead of decrementing lives.
 *
 * MAIN.ASM:2761-2771  (@@demo branch of init_start_game / init_ball):
 *   mov ball_1.sprite_status,On
 *   mov ball_1.sprite_sens_x,+3
 *   mov ball_1.sprite_sens_y,-4
 *
 * In the original, the demo just loops at the @@demo label after the
 * DELAI_DEMO countdown; lives are never checked for demo players.
 * We implement this as: respawn ball on paddle with demo velocity,
 * set state back to PLAYING.
 * -------------------------------------------------------------------------- */
void demo_handle_ball_lost(Game *g) {
    if (!g->demo_active) return;

    /* Respawn ball on paddle centre.
     * MAIN.ASM:5187-5194  ball_x = paddle_x + paddle_w/2 - ball_w/2 */
    ball_init(&g->balls[0],
              g->paddle.x + g->paddle.w / 2 - BALL_W / 2,
              g->paddle.y - BALL_H);

    /* MAIN.ASM:2762-2763  fixed demo ball velocity */
    g->balls[0].vx         = 3;    /* MAIN.ASM:2762  sprite_sens_x = +3 */
    g->balls[0].vy         = -4;   /* MAIN.ASM:2763  sprite_sens_y = -4 */
    g->balls[0].is_magnetic = 0;
    g->ball_count          = 1;

    g->state = STATE_PLAYING;
}

/* --------------------------------------------------------------------------
 * demo_ai_player_2
 *
 * Computer-controlled P2 paddle AI. Writes synthetic P2 input into the
 * shared FrameInput struct so game.c's existing P2 pipeline moves paddle_2,
 * fires lasers, and launches magnetic balls without new branches in game.c.
 *
 * ASM reference: MAIN.ASM:5131-5144  demo_move_x_player_1 — simple
 * ball-tracking AI (paddle_x = ball_x - paddle_w/2). The P2 twin operates
 * on cursor_2 / ball slots with sprite_player == player_2.
 *
 * Target selection:
 *   1. Prefer active, non-magnetic balls owned by P2 (ball.owner == 1).
 *   2. Fallback to any active non-magnetic ball.
 *   3. If only magnetic P2 ball exists (waiting on paddle), fire to launch.
 *
 * Movement: discrete p2_left / p2_right flags based on horizontal delta
 * with a small deadzone (matches paddle speed) to prevent jitter.
 *
 * Fire:
 *   - Magnetic ball attached → fire to launch (small delay via attach counter).
 *   - paddle_2.has_gun → fire every frame the cooldown allows (game.c gates it).
 * -------------------------------------------------------------------------- */
void demo_ai_player_2(const Game *g, FrameInput *out) {
    if (!g || !out) return;

    /* Only drive P2 when multiplayer is active. */
    if (g->game_mode <= 0) return;

    /* Reset P2 fields so any prior state from frame_input_poll_p2 is cleared. */
    out->p2_left    = 0;
    out->p2_right   = 0;
    out->p2_stick_x = 0.0f;
    out->p2_fire    = 0;

    /* Find best ball to track: prefer owner==1 (P2), fallback to any active. */
    const Ball *target = NULL;
    const Ball *fallback = NULL;
    int has_p2_magnetic = 0;
    for (int i = 0; i < g->ball_count; i++) {
        const Ball *b = &g->balls[i];
        if (!b->active) continue;
        if (b->is_magnetic) {
            if (b->owner == 1) has_p2_magnetic = 1;
            continue;  /* magnetic ball is on a paddle — don't track it */
        }
        if (b->owner == 1 && !target) target = b;
        if (!fallback) fallback = b;
    }
    if (!target) target = fallback;

    /* Magnetic P2 ball attached → AI presses fire to launch (game.c:1367-1384
     * uses input->p2_fire for owner==1 launches). No need to move paddle in
     * this case — ball is pinned to paddle_2 by game.c. */
    if (has_p2_magnetic) {
        /* Small delay: fire only every few frames so the launch looks natural
         * rather than instantaneous. The frame counter gives a cheap pulse. */
        if ((g->frame % 30) == 0) out->p2_fire = 1;
    }

    /* No active ball to chase → stay still. */
    if (!target) return;

    /* Steer paddle_2 toward target ball's X.
     * MAIN.ASM:5131-5144: paddle.x = ball.x - paddle_w/2 → centre paddle on ball.
     * We translate this into discrete left/right flags that game.c consumes
     * identically to keyboard input (deadzone = paddle speed to avoid jitter). */
    int paddle_cx = g->paddle_2.x + g->paddle_2.w / 2;
    int ball_cx   = target->x + BALL_W / 2;
    int delta     = ball_cx - paddle_cx;
    int deadzone  = g->paddle_2.speed;  /* 1 paddle-step */

    if (delta < -deadzone) {
        out->p2_left  = 1;
    } else if (delta > deadzone) {
        out->p2_right = 1;
    }

    /* Laser fire: if paddle has a gun, keep firing (game.c gates via
     * gun_cooldown, so we can safely hold fire pressed every frame). */
    if (g->paddle_2.has_gun) {
        out->p2_fire = 1;
    }
}
