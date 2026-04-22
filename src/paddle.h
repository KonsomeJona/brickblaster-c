#pragma once
/* paddle.h — BrickBlaster Paddle (cursor/vaisseau) system
 *
 * Ground truth: MAIN.ASM cursor/vaisseau sections and MOUSE.ASM Refresh_Keyboard.
 *
 * Movement speed:
 *   MOUSE.ASM:77  speed_counter dd 6   — paddle moves 6 pixels per key press
 *   MOUSE.ASM:49-72  @@keyboard_left/@@keyboard_right: sub/add speed_counter
 *
 * Paddle Y position:
 *   Blaster.inc:194  panel_option_pos_y = 458 - 12 = 446  (PADDLE_Y)
 *   MAIN.ASM:1463    screen_y - sprite_size_y → max_y (clamped there)
 *
 * Paddle widths (Blaster.inc):
 *   Normal:  vaisseau_size_x       = 74  (PADDLE_W)
 *   Large:   vaisseau_large_size_x = 105 (PADDLE_LARGE_W)
 *   Small:   vaisseau_small_size_x = 38  (PADDLE_SMALL_W)
 *   Height:  vaisseau_size_y       = 25  (PADDLE_H)
 *
 * Clamping to play area (MAIN.ASM:1454-1458, MOUSE.ASM:55-68):
 *   min_x = bord_x   = 112  (PLAY_X1)
 *   max_x = limite_x - sprite_size_x = 528 - paddle_width
 *   MOUSE.ASM:56-67  jl @@inf / jg @@sup: force to min/max
 *
 * Bounce zone logic is implemented directly in collision.c using CURSOR_BORDER
 * (Blaster.inc:451  cursor_border = 23).
 *
 * TODO F5 P1-ASM-37: paddle explosion animation — MAIN.ASM:2353-2409
 *   destroy_vaisseau cycles vaisseau_explo_* (8 frames @ speed 4, Blaster.inc:254-282)
 *   before game_over. Needs explo_timer (32 ticks) on life-loss / game-over,
 *   blocking ball respawn until countdown ends, and size-specific sprite
 *   selection (normal/large/small). Deferred — complex sequencing.
 *
 * TODO F5 P1-ASM-36: paddle telepod animation — 10-frame sequence
 *   (Blaster.inc:294-330) on POWERUP_TELEPOD collect. Deferred — complex.
 */

#include "constants.h"

typedef enum {
    PADDLE_SIZE_NORMAL = 0,  /* Blaster.inc:227  vaisseau_size_x = 74  */
    PADDLE_SIZE_SMALL  = 1,  /* Blaster.inc:291  vaisseau_small_size_x = 38 */
    PADDLE_SIZE_LARGE  = 2   /* Blaster.inc:286  vaisseau_large_size_x = 105 */
} PaddleSize;

typedef struct {
    int x, y;          /* pixel position top-left of paddle sprite       */
    int w, h;          /* current width and height                       */
    int min_x, max_x;   /* Horizontal movement clamp (MAIN.ASM sprite_min_x
                         * / sprite_max_x). Normally PLAY_X1 / PLAY_X2 - w
                         * (Init_Cursor MAIN.ASM:1454-1458, size-change
                         * handlers MAIN.ASM:2014-2018 / 2223-2227).
                         * In 2P with POWERUP_COLLISION active, updated every
                         * frame by game_update's detect_collision_cursor
                         * (MAIN.ASM:2274-2316). */
    PaddleSize size;    /* NORMAL / SMALL / LARGE                         */
    int reversed;       /* direction reversed powerup (Option_reverse)    */
    int has_gun;        /* shoot powerup active (mirrors laser_timer > 0) */
    int laser_timer;    /* per-player POWERUP_SHOOT / MINI_SHOOT countdown.
                         * ASM: count_tir_big_1/2, count_tir_left_1/2 etc.
                         * MAIN.ASM:6525,6541,6570,6594 — per-player counters
                         * for big/mini shoot. Fix for P2 collecting SHOOT
                         * cancelling P1's active laser (and vice versa). */
    int mini_laser;     /* 1 if laser_timer is MINI_SHOOT, 0 if big SHOOT */
    int size_timer;     /* per-player SMALL_SHIP / LARGE_SHIP countdown.
                         * MAIN.ASM:6491 option_small_ship_p, 6500 option_large_ship_p
                         * — per-player so P2 collecting LARGE_SHIP enlarges P2 only. */
    int reverse_timer;  /* per-player REVERSE countdown. MAIN.ASM:6562 option_reverse_p. */
    int gun_cooldown;   /* frames until next shot allowed                 */
    int gun_side;       /* 0=left, 1=right — alternates per shot (MAIN.ASM:1984) */
    int speed;          /* pixels per frame — MOUSE.ASM:77  speed_counter=6 */
    int prev_x;         /* previous frame X position — for magnetic ball tracking */
    int explo_timer;    /* ball-lost explosion animation countdown.
                         * MAIN.ASM:2353-2409 destroy_vaisseau +
                         * Blaster.inc:254-282 vaisseau_explo_* — 8 frames
                         * at speed 4 = 32 ticks total. While > 0 the
                         * paddle renders the explosion sprite and the
                         * ball respawn is deferred.  Frame index =
                         * 7 - (explo_timer - 1) / 4. */
} Paddle;

#define PADDLE_EXPLO_FRAMES  8   /* Blaster.inc:257  vaisseau_explo_nbs_anim */
#define PADDLE_EXPLO_SPEED   4   /* Blaster.inc:258  vaisseau_explo_speed    */
#define PADDLE_EXPLO_TICKS   (PADDLE_EXPLO_FRAMES * PADDLE_EXPLO_SPEED)  /* 32 */

/* Initialise paddle at screen centre, PADDLE_Y, normal width.
 * MAIN.ASM:1735  mov cursor_1.sprite_pos_x,eax  (centred via (limite_x-bord_x)/2)
 * MAIN.ASM:1466  mov [edx.sprite_pos_x],Off — then positioned in init */
void paddle_init(Paddle *p);

/* Move paddle by dx pixels, then clamp to play area.
 * MOUSE.ASM:49-72  Refresh_Keyboard keyboard_left/right
 * If reversed flag is set, dx is negated before applying.
 * MOUSE.ASM:33-47  cmp player_current_option,option_reverse_o → swap L/R */
void paddle_move(Paddle *p, int dx);

/* Change paddle width to match a size.
 * Blaster.inc:227,286,291 — size constants */
void paddle_set_size(Paddle *p, PaddleSize size);

/* Clamp paddle position to play area boundaries.
 * MOUSE.ASM:55-68  jl @@inf → set to min_x; jg @@sup → set to max_x
 *   min_x = PLAY_X1 = 112  (bord_x, Blaster.inc:445)
 *   max_x = PLAY_X2 - paddle_w = 528 - w  (limite_x - sprite_size_x) */
void paddle_clamp(Paddle *p);
