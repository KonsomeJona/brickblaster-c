#pragma once
/* ball.h — BrickBlaster Ball physics
 *
 * Ground truth: MAIN.ASM move_ball (line 3228) and related routines.
 *
 * MAIN.ASM:3250-3256  move_ball: pos_x += sens_x, pos_y += sens_y
 * MAIN.ASM:3430-3440  inc_speed_x: max_x limit = speed_max_x + angle
 * MAIN.ASM:3447-3456  inc_speed_y: max_y limit = speed_max_y - angle
 * MAIN.ASM:3463-3475  dec_speed_x: min_x limit = speed_min_x + angle
 * MAIN.ASM:3482-3493  dec_speed_y: min_y limit = speed_min_y - angle
 * MAIN.ASM:3548-3555  inverse_sens_x: neg sens_x, call detect_blocage
 * MAIN.ASM:3559-3566  inverse_sens_y: neg sens_y, call detect_blocage
 * MAIN.ASM:5355-5397  detect_blocage: anti-stuck via counter_1/counter_2
 *
 * Speed limits (Blaster.inc:408-413):
 *   speed_min_x = 1,  speed_max_x = 14
 *   speed_min_y = 2,  speed_max_y = 15
 *   angle_max   = 1   (only 0 or 1)
 *
 * The angle modifier INVERSELY affects which axis gets the tighter limit:
 *   When angle=1: X limit  = speed_max_x + 1 = 15  (looser)
 *                 Y limit  = speed_max_y - 1 = 14  (tighter)
 *                 X min    = speed_min_x + 1 = 2
 *                 Y min    = speed_min_y - 1 = 1
 *   When angle=0: X limit  = speed_max_x + 0 = 14
 *                 Y limit  = speed_max_y - 0 = 15
 *   (MAIN.ASM:3431 add eax,[edx.sprite_angle]  for X max)
 *   (MAIN.ASM:3448 sub eax,[edx.sprite_angle]  for Y max)
 *
 * ball_lost: ball is lost when bottom edge exits screen.
 *   MAIN.ASM computes this by comparing pos_y + ball_size_y >= screen_y.
 *   (SCREEN_H = 480, BALL_H = 9)
 *
 * is_magnetic: when On, ball stays attached to paddle until fire.
 *   MAIN.ASM:3231  cmp [edx.sprite_magnetic],Off  je @@ok
 *
 * Anti-stuck counters (Blaster.inc:387-388):
 *   BLOCAGE_COUNTER   = 60  frames
 *   BLOCAGE_COUNTER_2 = 3   repeated-position count before correction
 *   MAIN.ASM:5366  cmp [edx.sprite_counter_1],BLOCAGE_COUNTER
 *   MAIN.ASM:5385  cmp [edx.sprite_counter_2],BLOCAGE_COUNTER_2
 */

#include "constants.h"

typedef struct {
    int x, y;           /* pixel position top-left of 9x9 sprite            */
    int vx, vy;         /* velocity in pixels/frame (sprite_sens_x/y)       */
    int angle;          /* 0 or 1 — sprite_angle (Blaster.inc:413)          */
    int active;         /* 1 = ball is in play                               */
    int is_magnetic;    /* 1 = attached to paddle (sprite_magnetic != Off)  */
    int is_iron;        /* 1 = iron ball — passes through bricks             */
    int is_ghost;       /* 1 = ghost/bubble ball — explodes on first contact */
    int antistuck_1;    /* sprite_counter_1: 60-frame window counter         */
    int antistuck_2;    /* sprite_counter_2: repeated-position hit counter   */
    int prev_x, prev_y; /* saved position at start of 60-frame window       */
    int telepod_timer;  /* DELAI_TELEPOD countdown; >0 = ball is mid-teleport */
    int owner;          /* 0 = P1, 1 = P2 — multiplayer ownership for scoring/lives */
    /* F1-C: per-ball auto-speedup counter.
     * MAIN.ASM:3388-3394 detect_inc_speed reads/decrements
     *   [edx.sprite_speed_counter]
     * per-sprite — each ball has its own timer so multi-ball cadence is
     * independent. Initialized to Game.speed_level at ball_init and reset
     * to Game.speed_level at each auto-speedup trigger (MAIN.ASM:3400). */
    int speed_counter;
} Ball;

/* Initialise a ball at pixel position (x,y).
 * Sets velocity to 0, is_magnetic=1 (attached to paddle at start),
 * active=1, angle=0, all counters to 0. */
void ball_init(Ball *b, int x, int y);

/* Advance ball position by its velocity.
 * MAIN.ASM:3250-3256  pos_x += sens_x, pos_y += sens_y
 * Only moves if is_magnetic == 0. */
void ball_move(Ball *b);

/* Enforce MAIN.ASM speed limits on both axes.
 * Clamps |vx| to [speed_min_x+angle .. speed_max_x+angle]
 * Clamps |vy| to [speed_min_y-angle .. speed_max_y-angle]
 * Direction (sign) is preserved.
 * MAIN.ASM:3430-3493  inc/dec_speed_x/y routines */
void ball_clamp_speed(Ball *b);

/* Negate vx (bounce off vertical wall), then clamp.
 * MAIN.ASM:3548-3555  inverse_sens_x */
void ball_bounce_x(Ball *b);

/* Negate vy (bounce off horizontal wall/paddle), then clamp.
 * MAIN.ASM:3559-3566  inverse_sens_y */
void ball_bounce_y(Ball *b);

/* Returns 1 if ball has exited through the bottom of the screen.
 * Condition: y + BALL_H >= SCREEN_H  (MAIN.ASM: ball lost check) */
int ball_lost(const Ball *b);

/* Set velocity directly (used by launch logic and powerups). */
void ball_set_velocity(Ball *b, int vx, int vy);
