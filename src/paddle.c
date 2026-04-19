#include "paddle.h"

/* paddle.c — BrickBlaster Paddle (cursor/vaisseau) system
 *
 * Ground truth: MAIN.ASM cursor init/bounce sections + MOUSE.ASM Refresh_Keyboard.
 * Integer math only — no floats in this file.
 */

/* --------------------------------------------------------------------------
 * paddle_init
 *
 * MAIN.ASM:1735-1748  cursor init in Init_Cursor:
 *   Centre X = (limite_x - bord_x) / 2 + bord_x - vaisseau_size_x/2
 *            = (528 - 112) / 2 + 112 - 74/2
 *            = 208 + 112 - 37 = 283
 *   (SCREEN_CENTER - PADDLE_W/2 = 320 - 37 = 283)
 *   Y = PADDLE_Y = 416  (MAIN.ASM:1774  cursor_1.sprite_pos_y = limite_y = 416)
 *
 * MOUSE.ASM:77  speed_counter dd 6  — default paddle speed = 6 px/frame
 * -------------------------------------------------------------------------- */
void paddle_init(Paddle *p)
{
    p->size     = PADDLE_SIZE_NORMAL;
    p->w        = PADDLE_W;          /* Blaster.inc:227  vaisseau_size_x = 74  */
    p->h        = PADDLE_H;          /* Blaster.inc:228  vaisseau_size_y = 25  */

    /* Centre horizontally within play area — MAIN.ASM:1735 */
    p->x = SCREEN_CENTER - PADDLE_W / 2; /* 320 - 37 = 283 */
    p->y = PADDLE_Y;                      /* MAIN.ASM:1774  limite_y = 416 */

    p->reversed    = 0;
    p->has_gun     = 0;
    p->laser_timer = 0;
    p->mini_laser  = 0;
    p->size_timer  = 0;
    p->reverse_timer = 0;
    p->gun_cooldown = 0;
    p->speed       = PADDLE_SPEED; /* MOUSE.ASM:77  speed_counter dd 6 */
    p->prev_x      = p->x;
}

/* --------------------------------------------------------------------------
 * paddle_set_size
 *
 * Blaster.inc:227  vaisseau_size_x       = 74  (normal)
 * Blaster.inc:291  vaisseau_small_size_x = 38  (small)
 * Blaster.inc:286  vaisseau_large_size_x = 105 (large)
 * Height is always 25 for all sizes (Blaster.inc:228,287,292).
 * -------------------------------------------------------------------------- */
void paddle_set_size(Paddle *p, PaddleSize size)
{
    p->size = size;
    switch (size) {
        case PADDLE_SIZE_SMALL:
            p->w = PADDLE_SMALL_W; /* Blaster.inc:291  vaisseau_small_size_x = 38 */
            p->h = PADDLE_SMALL_H; /* Blaster.inc:292  vaisseau_small_size_y = 25 */
            break;
        case PADDLE_SIZE_LARGE:
            p->w = PADDLE_LARGE_W; /* Blaster.inc:286  vaisseau_large_size_x = 105 */
            p->h = PADDLE_LARGE_H; /* Blaster.inc:287  vaisseau_large_size_y = 25  */
            break;
        case PADDLE_SIZE_NORMAL:
        default:
            p->w = PADDLE_W;       /* Blaster.inc:227  vaisseau_size_x = 74 */
            p->h = PADDLE_H;       /* Blaster.inc:228  vaisseau_size_y = 25 */
            break;
    }
}

/* --------------------------------------------------------------------------
 * paddle_clamp
 *
 * MOUSE.ASM:55-68  Refresh_Keyboard @@limite:
 *   mov eax,cursor_2.sprite_pos_x
 *   cmp eax,cursor_2.sprite_min_x
 *   jl @@inf          ; if pos_x < min_x → set to min_x
 *   cmp eax,cursor_2.sprite_max_x
 *   jg @@sup          ; if pos_x > max_x → set to max_x
 *
 * MAIN.ASM:1454-1458  Init_Cursor sets:
 *   min_x = bord_x = 112       (PLAY_X1)
 *   max_x = limite_x - sprite_size_x = 528 - paddle_w  (PLAY_X2 - w)
 * -------------------------------------------------------------------------- */
void paddle_clamp(Paddle *p)
{
    int min_x, max_x;

    min_x = PLAY_X1;           /* Blaster.inc:445  bord_x = 112 */
    max_x = PLAY_X2 - p->w;   /* Blaster.inc:447  limite_x = 528, minus paddle width */

    if (p->x < min_x) p->x = min_x; /* MOUSE.ASM:62  mov cursor_2.sprite_pos_x,min_x */
    if (p->x > max_x) p->x = max_x; /* MOUSE.ASM:66  mov cursor_2.sprite_pos_x,max_x */
}

/* --------------------------------------------------------------------------
 * paddle_move
 *
 * MOUSE.ASM:33-73  Refresh_Keyboard:
 *   Check reverse powerup → swap left/right keys.
 *   MOUSE.ASM:50-51  sub cursor_2.sprite_pos_x,speed_counter  (move left)
 *   MOUSE.ASM:71-72  add cursor_2.sprite_pos_x,speed_counter  (move right)
 *   Then call @@limite to clamp.
 *
 * If reversed flag is set (POWERUP_REVERSE / option_reverse_o):
 *   MOUSE.ASM:33  cmp player_2.player_current_option,option_reverse_o
 *   MOUSE.ASM:42-45  swap left→right and right→left bindings
 * -------------------------------------------------------------------------- */
void paddle_move(Paddle *p, int dx)
{
    /* MOUSE.ASM:33-47  reverse powerup swaps direction */
    if (p->reversed) {
        dx = -dx; /* MOUSE.ASM:42  cmp right → keyboard_left; 44 cmp left → keyboard_right */
    }

    p->x += dx; /* MOUSE.ASM:51/72  sub/add cursor_pos_x,speed_counter */

    paddle_clamp(p);
}

