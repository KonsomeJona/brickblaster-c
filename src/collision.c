/* collision.c — BrickBlaster trajectory-interpolation collision system
 *
 * Ground truth: MAIN.ASM detect_colision_brique (line 3831)
 * Cross-check:  brickblaster-godot/src/core/collision_system.gd
 *               brickblaster-godot/docs/PHYSICS.md
 *
 * INTEGER MATH ONLY — no floats anywhere in this file.
 */
#include "collision.h"
#include <string.h>  /* memset */

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

static int iabs(int v) { return v < 0 ? -v : v; }

/* --------------------------------------------------------------------------
 * Direction-change lookup table
 *
 * MAIN.ASM:3913-3932  change_direction:
 *
 *   index = new_direction, which is built as:
 *     vel_sign_bits = (vx<0 ? 1 : 0) | (vy<0 ? 2 : 0)   — MAIN.ASM:3869-3878
 *     corner_bits   = bit0=corner1_hit, bit1=corner2_hit  — MAIN.ASM:3853/3859
 *     new_direction = (vel_sign_bits << 2) | corner_bits  — MAIN.ASM:3877-3878
 *
 *   The 16 cases map directly to the cmp eax,xxxb / je inverse_sens_x/y chain:
 *
 *   0b0001 (1):  cmp eax,0001b je inverse_sens_x   → FACE_X
 *   0b0010 (2):  cmp eax,0010b je inverse_sens_y   → FACE_Y
 *   0b0011 (3):  fallthrough   → FACE_BOTH (jmp inverse_sens_xy)
 *   0b0100 (4):  fallthrough   → FACE_BOTH
 *   0b0101 (5):  cmp eax,0101b je inverse_sens_x   → FACE_X
 *   0b0110 (6):  cmp eax,0110b je inverse_sens_y   → FACE_Y
 *   0b0111 (7):  fallthrough   → FACE_BOTH
 *   0b1000 (8):  fallthrough   → FACE_BOTH
 *   0b1001 (9):  cmp eax,1001b je inverse_sens_y   → FACE_Y
 *   0b1010 (10): cmp eax,1010b je inverse_sens_x   → FACE_X
 *   0b1011 (11): fallthrough   → FACE_BOTH
 *   0b1100 (12): fallthrough   → FACE_BOTH
 *   0b1101 (13): cmp eax,1101b je inverse_sens_y   → FACE_Y
 *   0b1110 (14): cmp eax,1110b je inverse_sens_x   → FACE_X
 *   0b1111 (15): fallthrough   → FACE_BOTH
 *   0 (0):       not reached   → FACE_NONE
 *
 * MAIN.ASM:3908-3932
 * -------------------------------------------------------------------------- */
static const CollisionFace s_dir_lookup[16] = {
    FACE_NONE,  /* 0000 (0): impossible — new_direction starts Off, never stored as 0 */
    FACE_X,     /* 0001 (1): corner1 hit, vx>=0, vy>=0  → inverse_sens_x              */
    FACE_Y,     /* 0010 (2): corner2 hit, vx>=0, vy>=0  → inverse_sens_y              */
    FACE_BOTH,  /* 0011 (3): both corners               → inverse_sens_xy              */
    FACE_BOTH,  /* 0100 (4): fallthrough                → inverse_sens_xy              */
    FACE_X,     /* 0101 (5): corner1 hit, vx<0, vy>=0   → inverse_sens_x              */
    FACE_Y,     /* 0110 (6): corner2 hit, vx<0, vy>=0   → inverse_sens_y              */
    FACE_BOTH,  /* 0111 (7): both corners               → inverse_sens_xy              */
    FACE_BOTH,  /* 1000 (8): fallthrough                → inverse_sens_xy              */
    FACE_Y,     /* 1001 (9): corner1 hit, vx>=0, vy<0   → inverse_sens_y              */
    FACE_X,     /* 1010 (10):corner2 hit, vx>=0, vy<0   → inverse_sens_x              */
    FACE_BOTH,  /* 1011 (11):both corners               → inverse_sens_xy              */
    FACE_BOTH,  /* 1100 (12):fallthrough                → inverse_sens_xy              */
    FACE_Y,     /* 1101 (13):corner1 hit, vx<0, vy<0    → inverse_sens_y              */
    FACE_X,     /* 1110 (14):corner2 hit, vx<0, vy<0    → inverse_sens_x              */
    FACE_BOTH   /* 1111 (15):both corners               → inverse_sens_xy              */
};

/* --------------------------------------------------------------------------
 * Trajectory table for one axis
 *
 * MAIN.ASM:3714-3752  create_table_de_trajectoire_axe_x
 * Fixed-point 16.16 arithmetic throughout.
 *
 * Entry [0]: depart_fp >> 1  (SAR eax,1 — shifts the 16.16 value right one bit,
 *            which gives (depart_pixel / 2) as an integer — a "half" starting point
 *            that ensures the first check is slightly before the ball's current edge)
 * Entries [1..n-1]: accumulated linear interpolation.
 *   Each step: delta = dest_fp - current_fp; step = delta / remaining; current += step
 *   Pixel value: current >> 16  (SAR ebx,16)
 *
 * Parameters are already in 16.16 fixed-point (shifted left 16 by caller).
 * out[] must have space for 'n' entries.
 * -------------------------------------------------------------------------- */
static void build_trajectory_axis(int depart_fp, int dest_fp, int n, int out[])
{
    /* MAIN.ASM:3716-3718  first entry: SAR eax,1 on the 16.16 value.
     * NOT >>16: the ASM shifts right by 1 bit, giving depart_fp/2 as a
     * fractional position — a half-step before the ball's current edge,
     * so the first sub-step checks slightly behind the ball. */
    out[0] = depart_fp >> 1;

    if (n <= 1) return;

    {
        int current = depart_fp;
        int remaining = n - 1; /* MAIN.ASM:3719  dec ebp (ebp starts at nbs_iteration_max) */
        int i;
        for (i = 1; i < n; i++) {
            /* MAIN.ASM:3721-3731  idiv loop:
             *   delta = dest_fp - current_fp
             *   step  = delta / remaining   (idiv ebp)
             *   current += step
             *   out[i] = current >> 16      (SAR ebx,16) */
            int delta = dest_fp - current;
            int step  = delta / remaining;   /* MAIN.ASM:3724  cdq; idiv ebp */
            current  += step;                /* MAIN.ASM:3727  add ebx,eax; mov depart,ebx */
            out[i]    = current >> 16;       /* MAIN.ASM:3728  sar ebx,16 */
            remaining--;                     /* MAIN.ASM:3731  dec ebp */
        }
    }
}

/* --------------------------------------------------------------------------
 * check_brick_at_point
 *
 * MAIN.ASM:3947-4078  detect_brique:
 *   - Skip if x >= limite_x or y >= limite_y  (MAIN.ASM:3951-3954)
 *   - Convert pixel → grid: col = (x-bord_x)>>5, row = (y-bord_y)>>4
 *     (MAIN.ASM:3958-3970  sub ebx,bord_x; shr ebx,5; sub eax,bord_y; shr eax,4)
 *   - index = row*nbs_brique_x + col
 *   - Read level_tbl byte
 *   - absente  → no hit (clc, ret)
 *   - invalide → no hit (clc, ret)
 *   - >= incassable → hit (stc, ret) — MAIN.ASM:3981  cmp al,incassable; jae @@collision
 *   - Out-of-bounds → no hit
 *   - level_flag check → skip if already hit this frame
 *
 * Returns 1 if this point hits a collidable surface, 0 otherwise.
 * Sets *out_index to grid index when returning 1.
 *
 * Iron ball rule: is_iron passes through BRICK_NORMAL but bounces INCASSABLE.
 * Transparent bricks: BRICK_TRANSPARENT — no collision at all.
 *
 * level_flag[]: one byte per grid cell; 1 = already processed this frame.
 * -------------------------------------------------------------------------- */
/* P0-ASM-4 fix: `check_brick_at_point` now mirrors MAIN.ASM:3996
 * `dec B [esi+ebx]` precisely — HP is decremented EACH time a corner lands
 * on a non-transparent non-indestructible brick cell. Previously the C
 * port only decremented once after the loop ended, so multi-corner hits
 * within a single frame were silently lost.
 *
 * Returns 1 if the point hit a bounceable surface (for new_direction
 * encoding). Applies brick damage in-line when applicable and sets
 * *out_destroyed to 1 iff this specific hit destroyed the brick. */
static int check_brick_at_point(
    int x, int y,
    Brick *bricks, int brick_count,
    unsigned char *level_flag,
    int is_iron,
    int ball_owner,
    int *out_index,
    int *out_destroyed)
{
    int col, row, index;

    if (out_destroyed) *out_destroyed = 0;

    /* MAIN.ASM:3951-3954  out-of-play-area check:
     *   cmp ebx,limite_x; jae @@end  (limite_x=528)
     *   cmp eax,limite_y; jae @@end  (limite_y=416)
     * Note: limite_y = 416 = limite_x - bord_x = 528 - 112 = play width.
     * The ASM uses this value for the Y check (reusing the width constant).
     * This means bricks at y >= 416 (rows 26-29) are never collided — exact ASM. */
    if (x < PLAY_X1 || x >= PLAY_X2) return 0;
    if (y < PLAY_Y1 || y >= PLAY_W) return 0;  /* MAIN.ASM:3953  cmp eax,limite_y (416=PLAY_W) */

    /* MAIN.ASM:3958-3962  col = (x - bord_x) >> 5 (shr ebx,5 = div 32)
     *   if negative after subtraction → clamp to 0 */
    col = x - PLAY_X1;
    if (col < 0) col = 0;
    col >>= 5; /* div BRICK_W (32) */

    /* MAIN.ASM:3965-3970  row = (y - bord_y) >> 4 (shr eax,4 = div 16) */
    row = y - PLAY_Y1;
    if (row < 0) row = 0;
    row >>= 4; /* div BRICK_H (16) */

    index = row * BRICK_COLS + col; /* MAIN.ASM:3972-3973  imul eax,nbs_brique_x; add ebx,eax */

    /* MAIN.ASM:3983-3986  bounds check */
    if (index < 0 || index >= brick_count) return 0;

    /* MAIN.ASM:3988-3991  level_flag — skip if already hit this frame */
    if (level_flag[index]) return 0;

    {
        Brick *b = &bricks[index];

        /* MAIN.ASM:3977-3980  absente/invalide → no collision */
        if (!b->active) return 0;
        if (b->raw == INVALIDE) return 0;
        if (b->raw == ABSENTE)  return 0;

        /* Transparent brick: ball passes through (no bounce) but brick
         * takes damage.
         * MAIN.ASM:3996  dec B [esi+ebx] — HP decremented BEFORE transparent check.
         * MAIN.ASM:4035-4042  if transparent → clc, ret (no bounce, ball passes through). */
        if (b->type == BRICK_TRANSPARENT) {
            level_flag[index] = 1;
            {
                int d = brick_hit(b, 1);  /* MAIN.ASM:3996  dec HP */
                if (out_destroyed) *out_destroyed = d;
            }
            b->last_hit_owner = ball_owner;  /* MAIN.ASM:4024 sprite_player */
            *out_index = index;
            return 0;  /* no bounce — ball passes through */
        }

        /* Iron ball passes through normal bricks (no bounce) but still
         * destroys them. Handled specially in collision_bricks loop. */
        if (is_iron && b->type == BRICK_NORMAL) return 0;

        /* Mark as hit this frame */
        level_flag[index] = 1; /* MAIN.ASM:3991  mov B [edi+ebx],On */

        /* P0-ASM-4: apply damage AT THE POINT of the hit, per
         * MAIN.ASM:3996 `dec B [esi+ebx]`. Indestructible/teleporter are
         * filtered inside brick_hit (always returns 0). */
        if (b->type == BRICK_NORMAL) {
            int d = brick_hit(b, 1);
            if (out_destroyed) *out_destroyed = d;
            b->last_hit_owner = ball_owner;
        }

        *out_index = index;
        return 1;
    }
}

/* --------------------------------------------------------------------------
 * Iron ball: destroy a normal brick at the given corner pixel without bounce.
 * MAIN.ASM:4032-4055  iron ball detect_brique decrements HP but skips bounce.
 *
 * P0-ASM-4: each corner independently damages its own brick.
 * -------------------------------------------------------------------------- */
static void iron_destroy_at(int cx, int cy,
                            Brick *bricks, int brick_count,
                            unsigned char *level_flag,
                            int ball_owner,
                            BrickCollision *result)
{
    int col, row, ii;
    if (cx < PLAY_X1 || cx >= PLAY_X2 || cy < PLAY_Y1 || cy >= PLAY_W) return;
    col = (cx - PLAY_X1) >> 5;
    row = (cy - PLAY_Y1) >> 4;
    ii  = row * BRICK_COLS + col;
    if (ii < 0 || ii >= brick_count) return;
    if (!bricks[ii].active || bricks[ii].type != BRICK_NORMAL || level_flag[ii]) return;
    level_flag[ii] = 1;
    {
        int d = brick_hit(&bricks[ii], 1);
        bricks[ii].last_hit_owner = ball_owner;
        if (!result->hit) {
            result->hit = 1;
            result->brick_index = ii;
            result->destroyed = d;
        }
        if (result->hit_count < MAX_BRICK_HITS_PER_FRAME) {
            result->hit_indices[result->hit_count]    = ii;
            result->destroyed_flag[result->hit_count] = d;
            result->hit_count++;
        }
    }
}

/* --------------------------------------------------------------------------
 * collision_bricks
 *
 * MAIN.ASM:3831-3888  detect_colision_brique
 *
 * Step 1: calc_nbs_iteration_max
 *   n = max(|vx|,|vy|) + 1        MAIN.ASM:3573-3587
 *   capped at MAX_SUBSTEPS (18)   MAIN.ASM:3590-3596
 *
 * Step 2: calc_coordonnee_de_depart  MAIN.ASM:3601-3705
 *   Select two leading corners based on velocity quadrant:
 *   mode_bits = (vx<0 ? 1:0) | (vy<0 ? 2:0)
 *   mode1 (bits==00 or bits==11 — same-sign diagonal):
 *     corner1 = top-right: (pos_x+ball_w, pos_y)     depart_x1/y1
 *     corner2 = bot-left:  (pos_x, pos_y+ball_h)     depart_x2/y2
 *   mode2 (bits==01 or bits==10 — cross-sign):
 *     corner1 = top-left:  (pos_x, pos_y)
 *     corner2 = bot-right: (pos_x+ball_w, pos_y+ball_h)
 *   Destinations = start + velocity (same logic, +sens_x/y added)
 *
 * Step 3: create_table_de_trajectoire_axe_x / _y  MAIN.ASM:3708-3803
 *   Build interpolated pixel positions (see build_trajectory_axis above).
 *
 * Step 4/5: Loop, check both corners, accumulate corner_hit_bits + new_direction
 *   MAIN.ASM:3845-3883
 *
 * Step 6: change_direction via lookup table  MAIN.ASM:3905-3932
 * -------------------------------------------------------------------------- */
BrickCollision collision_bricks(Ball *ball, Brick *bricks, int brick_count)
{
    BrickCollision result;
    int n;
    int mode_bits;
    int depart_x1, depart_y1, depart_x2, depart_y2;
    int dest_x1,   dest_y1,   dest_x2,   dest_y2;
    int table_x1[MAX_SUBSTEPS], table_y1[MAX_SUBSTEPS];
    int table_x2[MAX_SUBSTEPS], table_y2[MAX_SUBSTEPS];
    unsigned char level_flag[BRICK_COUNT];
    int vel_sign_bits;
    int new_direction;
    int i;
    int px, py, bsw, bsh;

    memset(&result, 0, sizeof(result));
    result.hit         = 0;
    result.brick_index = -1;
    result.face        = FACE_NONE;
    result.destroyed   = 0;
    result.hit_count   = 0;

    if (!ball->active || ball->is_magnetic) return result;

    /* ---- Step 1: calc_nbs_iteration_max  MAIN.ASM:3570-3597 ---- */
    {
        int ax = iabs(ball->vx);  /* MAIN.ASM:3573-3576  abs(sprite_sens_x) */
        int ay = iabs(ball->vy);  /* MAIN.ASM:3578-3581  abs(sprite_sens_y) */
        n = (ax >= ay ? ax : ay); /* MAIN.ASM:3583-3585  cmp eax,ebx; jae @@ok3; mov eax,ebx */
        n += 1;                   /* MAIN.ASM:3587-3588  mov nbs_iteration_max,eax; inc */
        if (n > MAX_SUBSTEPS)     /* MAIN.ASM:3590-3596  cmp; ja @@break */
            n = MAX_SUBSTEPS;
        if (n <= 0) return result; /* MAIN.ASM:3835-3836  or eax,eax; jz @@end */
    }

    /* ---- Step 2: calc_coordonnee_de_depart  MAIN.ASM:3601-3705 ---- */
    px  = ball->x;
    py  = ball->y;
    bsw = BALL_W;  /* Blaster.inc:334  ball_size = 9 */
    bsh = BALL_H;

    /* MAIN.ASM:3606-3615  mode_bits: bit0=(vx<0), bit1=(vy<0) */
    mode_bits  = (ball->vx < 0) ? 1 : 0;
    mode_bits |= (ball->vy < 0) ? 2 : 0;

    if (mode_bits == 0 || mode_bits == 3) {
        /* mode1: same-sign diagonal (both positive or both negative)
         * MAIN.ASM:3616-3619  cmp ebx,00b je @@mode1; cmp ebx,11b je @@mode1
         * Then @@mode1 at MAIN.ASM:3663: */
        /* corner1 = top-right  MAIN.ASM:3664-3667 */
        depart_x1 = (px + bsw) << 16;
        depart_y1 = py         << 16;
        /* corner2 = bot-left   MAIN.ASM:3673-3680 */
        depart_x2 = px         << 16;
        depart_y2 = (py + bsh) << 16;
        /* destinations          MAIN.ASM:3683-3703 */
        dest_x1 = (px + bsw + ball->vx) << 16;
        dest_y1 = (py        + ball->vy) << 16;
        dest_x2 = (px        + ball->vx) << 16;
        dest_y2 = (py + bsh  + ball->vy) << 16;
    } else {
        /* mode2: cross-sign diagonal  @@mode2 at MAIN.ASM:3620 */
        /* corner1 = top-left  MAIN.ASM:3621-3624 */
        depart_x1 = px         << 16;
        depart_y1 = py         << 16;
        /* corner2 = bot-right MAIN.ASM:3629-3636 */
        depart_x2 = (px + bsw) << 16;
        depart_y2 = (py + bsh) << 16;
        /* destinations        MAIN.ASM:3640-3660 */
        dest_x1 = (px        + ball->vx) << 16;
        dest_y1 = (py        + ball->vy) << 16;
        dest_x2 = (px + bsw  + ball->vx) << 16;
        dest_y2 = (py + bsh  + ball->vy) << 16;
    }

    /* ---- Step 3: build trajectory tables  MAIN.ASM:3708-3803 ---- */
    build_trajectory_axis(depart_x1, dest_x1, n, table_x1);
    build_trajectory_axis(depart_y1, dest_y1, n, table_y1);
    build_trajectory_axis(depart_x2, dest_x2, n, table_x2);
    build_trajectory_axis(depart_y2, dest_y2, n, table_y2);

    /* ---- Step 4 & 5: collision loop  MAIN.ASM:3842-3883 ---- */
    memset(level_flag, 0, sizeof(level_flag)); /* MAIN.ASM:3887  call clear_level_flag */

    /* Velocity sign bits for new_direction encoding  MAIN.ASM:3869-3876 */
    vel_sign_bits  = (ball->vx < 0) ? 1 : 0;  /* MAIN.ASM:3870-3871  cmp [sprite_sens_x],0; jns @@ok5 */
    vel_sign_bits |= (ball->vy < 0) ? 2 : 0;  /* MAIN.ASM:3873-3875  cmp [sprite_sens_y],0; jns @@ok6 */

    new_direction = 0; /* MAIN.ASM:3842  mov new_direction,Off */

    for (i = 0; i < n; i++) {
        int corner_bits = 0;
        int idx1 = -1, idx2 = -1;
        int d1 = 0, d2 = 0;

        /* Check corner 1  MAIN.ASM:3849-3853
         * P0-ASM-4: check_brick_at_point now applies damage in-place
         * (MAIN.ASM:3996 dec B [esi+ebx]). Destruction is reported via d1. */
        if (check_brick_at_point(table_x1[i], table_y1[i],
                                  bricks, brick_count, level_flag,
                                  ball->is_iron, ball->owner, &idx1, &d1)) {
            corner_bits |= 0x01; /* MAIN.ASM:3853  or ebp,01b */
        }
        /* Record this corner's hit (bounceable or transparent) */
        if (idx1 >= 0 && result.hit_count < MAX_BRICK_HITS_PER_FRAME) {
            result.hit_indices[result.hit_count]    = idx1;
            result.destroyed_flag[result.hit_count] = d1;
            result.hit_count++;
        }

        /* Check corner 2  MAIN.ASM:3855-3859 */
        if (check_brick_at_point(table_x2[i], table_y2[i],
                                  bricks, brick_count, level_flag,
                                  ball->is_iron, ball->owner, &idx2, &d2)) {
            corner_bits |= 0x02; /* MAIN.ASM:3859  or ebp,10b */
        }
        if (idx2 >= 0 && result.hit_count < MAX_BRICK_HITS_PER_FRAME) {
            result.hit_indices[result.hit_count]    = idx2;
            result.destroyed_flag[result.hit_count] = d2;
            result.hit_count++;
        }

        /* Iron ball: destroy normal bricks along the path without bouncing.
         * MAIN.ASM:4032-4055  detect_brique decrements HP, bounce is skipped. */
        if (ball->is_iron) {
            iron_destroy_at(table_x1[i], table_y1[i],
                            bricks, brick_count, level_flag, ball->owner, &result);
            iron_destroy_at(table_x2[i], table_y2[i],
                            bricks, brick_count, level_flag, ball->owner, &result);
        }

        /* MAIN.ASM:3861-3866  cmp ebp,0 je @@cont;
         *                      cmp [sprite_rebond],Off je @@cont;
         *                      cmp new_direction,Off jne @@cont */
        if (corner_bits != 0 && new_direction == 0) {
            /* MAIN.ASM:3877-3879  shl ebx,2; or ebx,ebp; mov new_direction,ebx */
            new_direction = (vel_sign_bits << 2) | corner_bits;

            /* Record first bounceable brick hit (compat with single-hit callers) */
            if (result.brick_index == -1) {
                int pick_idx = (corner_bits & 0x01) ? idx1 : idx2;
                int pick_d   = (corner_bits & 0x01) ? d1   : d2;
                result.hit = 1;
                result.brick_index = pick_idx;
                result.destroyed   = pick_d;
            }
        }
        /* MAIN.ASM:3881  add edi,4  (advance to next table slot) — handled by i++ */
    }

    /* ---- Step 6: change_direction  MAIN.ASM:3905-3932 ---- */
    if (new_direction != 0) {
        result.face = s_dir_lookup[new_direction & 0xF];

        /* Apply bounce to ball velocity */
        if (result.face == FACE_X || result.face == FACE_BOTH) {
            ball_bounce_x(ball); /* MAIN.ASM:3548  inverse_sens_x: neg vx */
        }
        if (result.face == FACE_Y || result.face == FACE_BOTH) {
            ball_bounce_y(ball); /* MAIN.ASM:3559  inverse_sens_y: neg vy */
        }

        /* P0-ASM-4: HP decrement already applied inside check_brick_at_point
         * (MAIN.ASM:3996 dec B [esi+ebx]). No extra brick_hit here. */
    }

    return result;
}

/* --------------------------------------------------------------------------
 * collision_walls
 *
 * MAIN.ASM:3497-3535  detect_colision_wall:
 *
 *   X-axis check (MAIN.ASM:3500-3513):
 *     new_x = pos_x + sens_x
 *     if new_x >= max_x → bounce X   [sprite_max_x set to limite_x - sprite_size_x]
 *     if new_x <= min_x → bounce X   [sprite_min_x set to bord_x]
 *
 *   Y-axis check (MAIN.ASM:3515-3528):
 *     new_y = pos_y + sens_y
 *     if new_y >= max_y → bounce Y   [sprite_max_y = screen_y - sprite_size_y]
 *     if new_y <= min_y → bounce Y   [sprite_min_y = bord_y]
 *
 * For the ball:
 *   min_x = PLAY_X1 = 112         (bord_x)
 *   max_x = PLAY_X2 - BALL_W      (limite_x - ball_size = 528 - 9 = 519)
 *   min_y = PLAY_Y1 = 0           (bord_y)
 *   max_y = SCREEN_H - BALL_H     (480 - 9 = 471)  [ball lost if below — no bounce]
 *
 * Note: the bottom wall is NOT bounced here.  ball_lost() detects that separately.
 * The ASM uses sprite_max_y = screen_y - sprite_size_y but for balls the max_y
 * wall bounce applies; the "lost" path is handled by the ball destruction check.
 * In practice the top wall bounces, and the left/right walls bounce.
 * The bottom wall is not a bounce — the ball is considered lost.
 * -------------------------------------------------------------------------- */
void collision_walls(Ball *ball, int ghost_active)
{
    /* Iter 2 fix #10: ghost balls bounce off walls normally; no wrap.
     * Verified against MAIN.ASM — option_ghost_p (6776-) only spawns ghost
     * balls; wall handling in detect_colision_wall (3497) is unchanged.
     * The `ghost_active` parameter is kept for API compatibility. */
    (void)ghost_active;

    /* MAIN.ASM:3500-3506  X-axis predictive check (pos_x + sens_x) */
    {
        int nx = ball->x + ball->vx;

        /* Right wall: nx >= PLAY_X2 - BALL_W  MAIN.ASM:3503 */
        if (nx >= PLAY_X2 - BALL_W) {
            ball_bounce_x(ball);
            ball->x = PLAY_X2 - BALL_W;
        }
        /* Left wall: nx <= PLAY_X1  MAIN.ASM:3505-3506 */
        else if (nx <= PLAY_X1) {
            ball_bounce_x(ball);
            ball->x = PLAY_X1;
        }
    }

    /* MAIN.ASM:3515-3521  Y-axis predictive check (pos_y + sens_y) */
    {
        int ny = ball->y + ball->vy;

        /* Top wall: ny <= PLAY_Y1  MAIN.ASM:3520-3521  cmp eax,[sprite_min_y]; jbe */
        if (ny <= PLAY_Y1) {
            ball_bounce_y(ball);                /* MAIN.ASM:3527  call inverse_sens_y */
            ball->y = PLAY_Y1;
        }
        /* Bottom: no bounce — ball_lost() handles detection separately. */
    }
}

/* --------------------------------------------------------------------------
 * collision_paddle
 *
 * MAIN.ASM:4152-4271  detect_colision_cursor_player_1
 *
 * Conditions (MAIN.ASM:4164-4179):
 *   if sprite_sens_y < 0: return  (js @@end — ball moving up, skip)
 *   ball_bottom = pos_y + size_y
 *   if ball_bottom < paddle.pos_y: return  (jb @@end)
 *   ball_right  = pos_x + size_x
 *   if ball_right < paddle.pos_x: return  (jb @@end)
 *   ball_left   = pos_x
 *   if ball_left >= paddle.pos_x + paddle.size_x: return  (jb @@inverse_sens_y skips)
 *
 * On hit (MAIN.ASM:4207-4271  @@inverse_sens_y path):
 *   1. neg sprite_sens_y  (bounce up)
 *   2. Determine hit zone based on sens_x direction and ball position vs cursor_border
 *
 * Zone logic for positive vx (MAIN.ASM:4223-4243):
 *   hit_x = pos_x + size_x  (right edge of ball when moving right)
 *   if hit_x < paddle.pos_x + cursor_border:  inc_angle_x  (left edge zone)
 *   elif hit_x > paddle.pos_x + paddle.w - cursor_border: inc_angle_x (right edge)
 *   else: dec_angle_x  (center zone)
 *
 * Zone logic for negative vx (MAIN.ASM:4253-4271  @@sens_negatif_player_1):
 *   hit_x = pos_x  (left edge of ball when moving left)
 *   if hit_x > paddle.pos_x + paddle.w - cursor_border: inc_angle_x
 *   elif hit_x < paddle.pos_x + cursor_border: inc_angle_x
 *   else: dec_angle_x
 * -------------------------------------------------------------------------- */
void collision_paddle(Ball *ball, const Paddle *paddle, int magnetic_active)
{
    int ball_bottom, ball_right, ball_left;
    int hit_x;
    int left_edge, right_edge;

    /* MAIN.ASM:4164-4166  cmp [sprite_sens_y],0; or eax,eax; js @@end */
    if (ball->vy < 0) return;  /* ball moving up — no paddle collision */

    /* MAIN.ASM:4167-4170  ball_bottom vs paddle top */
    ball_bottom = ball->y + BALL_H;
    if (ball_bottom < paddle->y) return;

    /* MAIN.ASM:4171-4174  ball right vs paddle left */
    ball_right = ball->x + BALL_W;
    if (ball_right < paddle->x) return;

    /* MAIN.ASM:4175-4179  ball left vs paddle right */
    ball_left = ball->x;
    if (ball_left >= paddle->x + paddle->w) return;

    /* Collision confirmed — @@inverse_sens_y at MAIN.ASM:4207 */

    /* Magnetic paddle: ball sticks to paddle on contact.
     * P1-ASM-11 fix: ASM does NOT zero velocity here.
     * MAIN.ASM:4215  call detect_magnetic_player_1 — gates on magnetic_flag bit.
     * MAIN.ASM:4420-4439 detect_magnetic: mov sprite_magnetic,ebp (paddle ptr)
     *   and stores decal_x. Does NOT touch sprite_sens_x/y.
     * MAIN.ASM:4219-4221 — AFTER detect_magnetic, vy is negated normally (bounce).
     *
     * So on catch: keep vx, flip vy (normal bounce), mark ball magnetic.
     * The per-frame move_ball (MAIN.ASM:3228-3296) then pins the ball to the
     * paddle via sprite_magnetic while keeping velocity intact. On fire,
     * move_ball clears sprite_magnetic and resumes moving by (vx, vy).
     *
     * Pin to paddle top so the ball visually sticks. */
    if (magnetic_active) {
        ball->is_magnetic = 1;  /* now stuck to paddle */
        ball->vy = -ball->vy;   /* MAIN.ASM:4220  neg sprite_sens_y (normal bounce) */
        ball->y  = paddle->y - BALL_H;
        return;
    }

    /* MAIN.ASM:4219-4221  neg sprite_sens_y (bounce up — no clamp here) */
    ball->vy = -ball->vy;

    /* Zone boundaries  MAIN.ASM:4228-4238 */
    left_edge  = paddle->x + CURSOR_BORDER;
    right_edge = paddle->x + paddle->w - CURSOR_BORDER;

    if (ball->vx >= 0) {
        /* Ball moving right (or zero): use right edge of ball  MAIN.ASM:4226-4243 */
        hit_x = ball->x + BALL_W;

        if (hit_x < left_edge) {
            /* MAIN.ASM:4231  jb inverse_sens_x — left zone while moving right */
            ball_bounce_x(ball);
        } else if (hit_x > right_edge) {
            /* MAIN.ASM:4239  call detect_sens_x; call inc_angle_x — right zone */
            /* inc_angle_x: MAIN.ASM:4443-4463 */
            if (ball->angle < ANGLE_MAX) {
                int lim_x = SPEED_MAX_X + ball->angle;     /* MAIN.ASM:4449-4450 */
                if (ball->vx < lim_x && ball->vx > -lim_x) { /* MAIN.ASM:4451-4455 */
                    ball->angle++;
                    ball->vy += 1;                          /* MAIN.ASM:4458  inc [sprite_sens_y] */
                    ball->vx += (ball->vx < 0 ? -1 : 1);   /* MAIN.ASM:4460  add eax,incrementation */
                }
            }
        } else {
            /* MAIN.ASM:4241  call dec_angle_x — center zone */
            /* dec_angle_x: MAIN.ASM:4467-4478 */
            if (ball->angle > 0) {
                ball->angle--;
                ball->vy -= 1;                              /* MAIN.ASM:4473  dec [sprite_sens_y] */
                ball->vx -= (ball->vx < 0 ? -1 : 1);       /* MAIN.ASM:4475  sub eax,incrementation */
            }
        }
    } else {
        /* Ball moving left: use left edge of ball  MAIN.ASM:4253-4271 @@sens_negatif */
        hit_x = ball->x;

        if (hit_x > right_edge) {
            /* MAIN.ASM:4261  ja inverse_sens_x — right zone while moving left */
            ball_bounce_x(ball);
        } else if (hit_x < left_edge) {
            /* MAIN.ASM:4267  call detect_sens_x; call inc_angle_x — left zone */
            /* inc_angle_x: MAIN.ASM:4443-4463 */
            if (ball->angle < ANGLE_MAX) {
                int lim_x = SPEED_MAX_X + ball->angle;     /* MAIN.ASM:4449-4450 */
                if (ball->vx < lim_x && ball->vx > -lim_x) { /* MAIN.ASM:4451-4455 */
                    ball->angle++;
                    ball->vy += 1;                          /* MAIN.ASM:4458  inc [sprite_sens_y] */
                    ball->vx += (ball->vx < 0 ? -1 : 1);   /* MAIN.ASM:4460  add eax,incrementation */
                }
            }
        } else {
            /* MAIN.ASM:4269  call dec_angle_x — center zone */
            /* dec_angle_x: MAIN.ASM:4467-4478 */
            if (ball->angle > 0) {
                ball->angle--;
                ball->vy -= 1;                              /* MAIN.ASM:4473  dec [sprite_sens_y] */
                ball->vx -= (ball->vx < 0 ? -1 : 1);       /* MAIN.ASM:4475  sub eax,incrementation */
            }
        }
    }
}
