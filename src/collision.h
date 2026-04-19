#pragma once
/* collision.h — BrickBlaster trajectory-interpolation collision system
 *
 * Ground truth: MAIN.ASM detect_colision_brique (line 3831), detect_brique (line 3947),
 *               detect_colision_wall (line 3497), detect_colision_cursor_player_1 (line 4152)
 *
 * Key algorithm (MAIN.ASM:3831-3888  detect_colision_brique):
 *   1. calc_nbs_iteration_max   — n = max(|vx|,|vy|)+1, capped at speed_max+2+angle_max=18
 *   2. calc_coordonnee_de_depart — pick two leading corners based on velocity quadrant
 *   3. create_table_de_trajectoire_axe_x/y — build interpolated position tables (16.16 fp)
 *   4. Loop n iterations: check both corners against brick grid at each sub-step
 *   5. Accumulate hit corner bits; compute new_direction encoding
 *   6. change_direction — resolve new_direction to x/y/xy inversion
 *
 * Trajectory interpolation (MAIN.ASM:3714-3752):
 *   - Fixed-point 16.16 arithmetic throughout (shift left 16)
 *   - First table entry: depart >> 1  (SAR eax,1 on 16.16 gives depart/2 as integer)
 *   - Subsequent entries: delta / remaining_steps accumulated into current position
 *   - Convert to pixel: current >> 16  (SAR ebx,16)
 *
 * Corner selection (MAIN.ASM:3601-3705  calc_coordonnee_de_depart):
 *   mode_bits = (vx<0 ? 1 : 0) | (vy<0 ? 2 : 0)
 *   mode1 (same-sign diagonal: bits==00 or bits==11):
 *     corner1 = (pos_x+ball_w, pos_y),       corner2 = (pos_x, pos_y+ball_h)
 *   mode2 (cross-sign diagonal: bits==01 or bits==10):
 *     corner1 = (pos_x, pos_y),              corner2 = (pos_x+ball_w, pos_y+ball_h)
 *
 * Direction change lookup (MAIN.ASM:3913-3932  change_direction):
 *   new_direction = (vel_sign_bits << 2) | corner_hit_bits
 *   vel_sign_bits: bit0=(vx<0), bit1=(vy<0)
 *   corner_hit_bits: bit0=corner1_hit, bit1=corner2_hit
 *   Lookup table maps 16 cases to FACE_X / FACE_Y / FACE_BOTH / FACE_NONE
 *
 * MAX_SUBSTEPS = speed_max + 2 + angle_max = 15 + 2 + 1 = 18
 * (Blaster.inc:412-413, MAIN.ASM:3590-3596)
 */

#include "ball.h"
#include "brick.h"
#include "paddle.h"

/* Maximum sub-steps for trajectory interpolation.
 * MAIN.ASM:3590  cmp nbs_iteration_max,speed_max+2+angle_max
 * Blaster.inc:412  speed_max=15, Blaster.inc:413  angle_max=1  → 15+2+1 = 18 */
#define MAX_SUBSTEPS  18

/* Which face of a brick the ball hit — derived from new_direction encoding.
 * MAIN.ASM:3913-3932  change_direction: cases map to inverse_sens_x/y/xy */
typedef enum {
    FACE_NONE   = 0,  /* no brick hit (iron ball through normal, transparent, etc.) */
    FACE_X      = 1,  /* hit left or right face → negate vx  (inverse_sens_x)       */
    FACE_Y      = 2,  /* hit top or bottom face → negate vy  (inverse_sens_y)        */
    FACE_BOTH   = 3   /* corner hit → negate both             (inverse_sens_xy)       */
} CollisionFace;

/* P0-ASM-4: per MAIN.ASM:3996 the level table is decremented for EACH corner
 * that lands on a brick cell during a single frame. We record up to
 * MAX_BRICK_HITS_PER_FRAME distinct cells. In practice the trajectory
 * iteration yields at most 2 corners x n sub-steps hits, but since the
 * level_flag[] array prevents repeat hits on the same brick, the distinct
 * count is bounded by the number of unique cells touched. 8 is a comfortable
 * upper bound for normal ball speeds. */
#define MAX_BRICK_HITS_PER_FRAME 8

typedef struct {
    int           hit;          /* 1 if any collidable brick was struck                 */
    int           brick_index;  /* 0-389 grid index of first brick hit, -1 if none      */
    CollisionFace face;         /* which axes to invert                                  */
    int           destroyed;    /* 1 if the FIRST struck brick's hp reached 0 (compat)   */
    /* Multi-brick-per-frame (P0-ASM-4): list of ALL bricks damaged this frame,
     * in order encountered. Includes transparent bricks (no bounce) and
     * normal-bounce bricks. `destroyed_flag[i]` is 1 if bricks[i] was
     * destroyed by this frame's hit. */
    int           hit_count;
    int           hit_indices[MAX_BRICK_HITS_PER_FRAME];
    int           destroyed_flag[MAX_BRICK_HITS_PER_FRAME];
} BrickCollision;

/* --------------------------------------------------------------------------
 * collision_bricks
 *
 * Move ball one frame using trajectory interpolation (detect_colision_brique).
 * Checks both leading corners at each sub-step.  On hit:
 *   - Applies bounce (ball_bounce_x / ball_bounce_y via face)
 *   - Calls brick_hit on the struck brick (hp--)
 *   - Iron ball (is_iron==1) passes through BRICK_NORMAL bricks but still
 *     bounces off BRICK_INDESTRUCTIBLE and BRICK_TELEPORTER.
 *   - Transparent bricks (BRICK_TRANSPARENT) never collide.
 *
 * bricks:      full 390-element grid; inactive bricks (active==0) are skipped.
 * brick_count: must be BRICK_COUNT (390) in normal gameplay.
 *
 * Returns BrickCollision; caller uses brick_index to spawn powerup / score.
 *
 * MAIN.ASM:3831  detect_colision_brique
 * -------------------------------------------------------------------------- */
BrickCollision collision_bricks(Ball *ball, Brick *bricks, int brick_count);

/* --------------------------------------------------------------------------
 * collision_walls
 *
 * Resolve ball vs play-area walls.  Does NOT move the ball — call before
 * ball_move() each frame.  Bounces as per detect_colision_wall (MAIN.ASM:3497).
 *
 * Bounds (Blaster.inc:445-448):
 *   Left  wall: ball_x <= PLAY_X1 (112)  → bounce_x, clamp to PLAY_X1
 *   Right wall: ball_x + BALL_W >= PLAY_X2 (528) → bounce_x, clamp
 *   Top   wall: ball_y <= PLAY_Y1 (0)    → bounce_y, clamp
 *   Bottom:     ball_y + BALL_H >= SCREEN_H → lost (ball_lost() handles this)
 *
 * Iter 2 fix #10: ghost balls bounce off walls normally; `ghost_active` is
 * ignored but kept for API compatibility. Verified against MAIN.ASM:3497.
 *
 * MAIN.ASM:3497  detect_colision_wall
 * -------------------------------------------------------------------------- */
void collision_walls(Ball *ball, int ghost_active);

/* --------------------------------------------------------------------------
 * collision_paddle
 *
 * Resolve ball vs paddle.  Conditions (MAIN.ASM:4164-4181):
 *   - ball vy > 0  (ball moving downward — js @@end)
 *   - ball bottom (y + BALL_H) >= paddle top (paddle.y)
 *   - ball right  (x + BALL_W) >  paddle left (paddle.x)
 *   - ball left   (x)          <  paddle right (paddle.x + paddle.w)
 *
 * On hit:
 *   - Negates vy (always bounce up)
 *   - Adjusts angle and vx based on hit zone (cursor_border = 23px edges)
 *     MAIN.ASM:4226-4271  @@sens_negatif_player_1 / inc_angle_x / dec_angle_x
 *
 * MAIN.ASM:4152  detect_colision_cursor_player_1
 * -------------------------------------------------------------------------- */
/* magnetic_active: non-zero if POWERUP_MAGNETIC applies for this paddle/ball
 * combination (per-player bit in game magnetic_flag — MAIN.ASM:6755-6759
 * option_magnetic_p sets PLAYER_ONE / PLAYER_TWO bit for current_player).
 * Caller is responsible for checking the correct bit. MAIN.ASM:4401 / 4412
 * `test magnetic_flag,PLAYER_ONE/_TWO` gate in detect_magnetic_player_N.
 *
 * P1-ASM-11 fix: the ASM magnetic catch does NOT zero vx/vy. It only flips
 * sens_y (the normal bounce-up) and sets sprite_magnetic=ebp (paddle pointer).
 * We preserve vx/vy on catch and pin the ball to the paddle's top via
 * per-frame x/y tracking in game.c. */
void collision_paddle(Ball *ball, const Paddle *paddle, int magnetic_active);
