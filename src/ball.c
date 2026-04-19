#include "ball.h"

/* ball.c — BrickBlaster Ball physics
 *
 * Ground truth: MAIN.ASM move_ball (line 3228) and related speed/bounce routines.
 * Integer math only — no floats anywhere in this file.
 */

/* Helper: absolute value for integers (no stdlib required) */
static int iabs(int v) { return v < 0 ? -v : v; }

/* Helper: sign of integer: returns 1 for positive, -1 for negative, 1 for zero */
static int isign(int v) { return v < 0 ? -1 : 1; }

/* --------------------------------------------------------------------------
 * ball_init
 *
 * MAIN.ASM:1466-1481  Init_Balls: sets pos_x/y, sens_x/y to 0, magnetic=On.
 * New ball always starts attached to the paddle (is_magnetic = 1).
 * -------------------------------------------------------------------------- */
void ball_init(Ball *b, int x, int y)
{
    b->x          = x;
    b->y          = y;
    b->vx         = 0;
    b->vy         = 0;
    b->angle      = 0;  /* Blaster.inc:413  angle_max=1, starts at 0 */
    b->active     = 1;
    b->is_magnetic = 1; /* MAIN.ASM:1481  mov [edx.sprite_magnetic],Off — but
                         * new balls start magnetic; Off here means "attached" */
    b->is_iron    = 0;
    b->is_ghost   = 0;
    b->antistuck_1 = 0;
    b->antistuck_2 = 0;
    b->prev_x     = x;
    b->prev_y     = y;
    b->telepod_timer = 0;
    b->speed_counter = 0;  /* F1-C: per-ball counter; reset by game tick to
                            * speed_level * 3 (60fps vs ASM 18fps scaling).
                            * MAIN.ASM:3388-3394 per-sprite sprite_speed_counter. */
}

/* --------------------------------------------------------------------------
 * ball_move
 *
 * MAIN.ASM:3250-3256  move_ball @@ok:
 *   mov eax,[edx.sprite_pos_x]
 *   add eax,[edx.sprite_sens_x]
 *   mov [edx.sprite_pos_x],eax
 *   mov eax,[edx.sprite_pos_y]
 *   add eax,[edx.sprite_sens_y]
 *   mov [edx.sprite_pos_y],eax
 *
 * Magnetic balls stay pinned (handled by caller in full game); here we skip
 * movement to reflect the check at MAIN.ASM:3231 "cmp sprite_magnetic,Off".
 * -------------------------------------------------------------------------- */
void ball_move(Ball *b)
{
    if (!b->active) return;
    if (b->is_magnetic) return; /* MAIN.ASM:3231  je @@ok skips move */

    b->x += b->vx; /* MAIN.ASM:3251  add eax,[edx.sprite_sens_x] */
    b->y += b->vy; /* MAIN.ASM:3255  add eax,[edx.sprite_sens_y] */
}

/* --------------------------------------------------------------------------
 * ball_clamp_speed
 *
 * The ASM uses inc_speed_x / inc_speed_y / dec_speed_x / dec_speed_y to
 * gradually adjust speed, but for the purpose of a clamp function we enforce
 * the hard limits directly.
 *
 * X-axis limits (MAIN.ASM:3430-3440 inc_speed_x, 3463-3475 dec_speed_x):
 *   MAIN.ASM:3430  mov eax,speed_max_x      ; 14
 *   MAIN.ASM:3431  add eax,[edx.sprite_angle]; + angle (0 or 1)
 *   → max_x = speed_max_x + angle  (14 or 15)
 *   MAIN.ASM:3463  mov eax,speed_min_x      ; 1
 *   MAIN.ASM:3464  add eax,[edx.sprite_angle]; + angle
 *   → min_x = speed_min_x + angle  (1 or 2)
 *
 * Y-axis limits (MAIN.ASM:3447-3456 inc_speed_y, 3482-3493 dec_speed_y):
 *   MAIN.ASM:3447  mov eax,speed_max_y      ; 15
 *   MAIN.ASM:3448  sub eax,[edx.sprite_angle]; - angle
 *   → max_y = speed_max_y - angle  (15 or 14)
 *   MAIN.ASM:3482  mov eax,speed_min_y      ; 2
 *   MAIN.ASM:3483  sub eax,[edx.sprite_angle]; - angle
 *   → min_y = speed_min_y - angle  (2 or 1)
 *
 * The sign (direction) is preserved; the absolute value is clamped.
 * -------------------------------------------------------------------------- */
void ball_clamp_speed(Ball *b)
{
    int max_x, min_x, max_y, min_y;
    int abs_vx, abs_vy;

    /* MAIN.ASM:3430-3431 and 3463-3464 */
    max_x = SPEED_MAX_X + b->angle; /* Blaster.inc:410  speed_max_x = 14 */
    min_x = SPEED_MIN_X + b->angle; /* Blaster.inc:408  speed_min_x =  1 */

    /* MAIN.ASM:3447-3448 and 3482-3483 */
    max_y = SPEED_MAX_Y - b->angle; /* Blaster.inc:411  speed_max_y = 15 */
    min_y = SPEED_MIN_Y - b->angle; /* Blaster.inc:409  speed_min_y =  2 */

    abs_vx = iabs(b->vx);
    abs_vy = iabs(b->vy);

    /* Clamp X speed — MAIN.ASM:3432-3436 jge/jle @@end guards */
    if (abs_vx > max_x) abs_vx = max_x;
    if (abs_vx < min_x) abs_vx = min_x;

    /* Clamp Y speed — MAIN.ASM:3449-3453 jge/jle @@end guards */
    if (abs_vy > max_y) abs_vy = max_y;
    if (abs_vy < min_y) abs_vy = min_y;

    /* Restore sign */
    b->vx = isign(b->vx) * abs_vx;
    b->vy = isign(b->vy) * abs_vy;
}

/* --------------------------------------------------------------------------
 * ball_detect_blocage  (internal)
 *
 * MAIN.ASM:5355-5397  detect_blocage
 * Anti-stuck mechanism: called after every bounce.  Tracks how many times
 * per BLOCAGE_COUNTER (60) bounces the ball returns to the same position.
 * If it does so BLOCAGE_COUNTER_2 (3) times → adjust angle to break pattern.
 *
 * Blaster.inc:387  BLOCAGE_COUNTER  = 60
 * Blaster.inc:388  BLOCAGE_COUNTER_2 = 3
 * -------------------------------------------------------------------------- */
static void ball_detect_blocage(Ball *b)
{
    int lim_x;

    /* MAIN.ASM:5364-5374: if counter_1==0 or counter_1==BLOCAGE_COUNTER → @@init:
     *   reset both counters and snapshot current position */
    if (b->antistuck_1 == 0 || b->antistuck_1 == BLOCAGE_COUNTER) {
        b->antistuck_1 = 0;
        b->antistuck_2 = 0;
        b->prev_x = b->x;  /* MAIN.ASM:5371-5374  sprite_bak_x */
        b->prev_y = b->y;  /* sprite_bak_y */
    }

    b->antistuck_1++;  /* MAIN.ASM:5376  inc [sprite_counter_1] */

    /* MAIN.ASM:5378-5383: if position changed from snapshot → not stuck, exit */
    if (b->x != b->prev_x || b->y != b->prev_y) return;

    b->antistuck_2++;  /* MAIN.ASM:5384  inc [sprite_counter_2] */
    if (b->antistuck_2 < BLOCAGE_COUNTER_2) return;  /* MAIN.ASM:5385-5386 */

    b->antistuck_2 = 0;  /* MAIN.ASM:5387  mov [sprite_counter_2],0 */

    /* MAIN.ASM:5388-5391: detect_sens_x → inc_angle_x or dec_angle_x */
    if (b->angle == 0) {
        /* inc_angle_x: MAIN.ASM:4443-4463 */
        if (b->angle >= ANGLE_MAX) return;            /* MAIN.ASM:4446-4447 @@max */
        lim_x = SPEED_MAX_X + b->angle;              /* MAIN.ASM:4449-4450 */
        if (b->vx >= lim_x || b->vx <= -lim_x) return; /* MAIN.ASM:4451-4455 @@max */
        b->angle++;                                   /* MAIN.ASM:4457  inc [sprite_angle] */
        b->vy += 1;                                   /* MAIN.ASM:4458  inc [sprite_sens_y] */
        b->vx += (b->vx < 0 ? -1 : 1);              /* MAIN.ASM:4460  add eax,incrementation */
    } else {
        /* dec_angle_x: MAIN.ASM:4467-4478 */
        if (b->angle <= 0) return;                    /* MAIN.ASM:4470-4471 @@min */
        b->angle--;                                   /* MAIN.ASM:4472  dec [sprite_angle] */
        b->vy -= 1;                                   /* MAIN.ASM:4473  dec [sprite_sens_y] */
        b->vx -= (b->vx < 0 ? -1 : 1);              /* MAIN.ASM:4475  sub eax,incrementation */
    }
}

/* --------------------------------------------------------------------------
 * ball_bounce_x
 *
 * MAIN.ASM:3548-3555  inverse_sens_x:
 *   mov eax,[edx.sprite_sens_x]
 *   neg eax
 *   mov [edx.sprite_sens_x],eax
 *   call detect_blocage
 *   ret
 * -------------------------------------------------------------------------- */
void ball_bounce_x(Ball *b)
{
    b->vx = -b->vx;           /* MAIN.ASM:3552  neg eax */
    ball_detect_blocage(b);   /* MAIN.ASM:3553  call detect_blocage */
}

/* --------------------------------------------------------------------------
 * ball_bounce_y
 *
 * MAIN.ASM:3559-3566  inverse_sens_y:
 *   mov eax,[edx.sprite_sens_y]
 *   neg eax
 *   mov [edx.sprite_sens_y],eax
 *   call detect_blocage
 *   ret
 * -------------------------------------------------------------------------- */
void ball_bounce_y(Ball *b)
{
    b->vy = -b->vy;           /* MAIN.ASM:3563  neg eax */
    ball_detect_blocage(b);   /* MAIN.ASM:3564  call detect_blocage */
}

/* --------------------------------------------------------------------------
 * ball_lost
 *
 * Ball is lost when its bottom edge is at or below the screen bottom.
 * The ASM detects this in the wall collision routine and the main loop.
 * Condition: pos_y + ball_size_y >= screen_y
 *   BALL_H    = 9    Blaster.inc:334  ball_size
 *   SCREEN_H  = 480  Blaster.inc:424  screen_y
 * -------------------------------------------------------------------------- */
int ball_lost(const Ball *b)
{
    return (b->y + BALL_H >= SCREEN_H); /* BALL_H=9, SCREEN_H=480 */
}

/* --------------------------------------------------------------------------
 * ball_set_velocity
 *
 * Convenience setter — used by launch logic (MAIN.ASM:2763-2769 launch speed
 * for cooperative mode) and powerup handlers.
 * After setting, clamp is NOT applied automatically; caller should clamp if
 * needed.
 * -------------------------------------------------------------------------- */
void ball_set_velocity(Ball *b, int vx, int vy)
{
    b->vx = vx;
    b->vy = vy;
}
