#include "monster.h"
#include "raylib.h"   /* GetRandomValue */
#include <string.h>   /* memset */

/* --------------------------------------------------------------------------
 * monster_init_level
 *
 * MAIN.ASM:2927-2942  init_Monster:
 *   mov counter_monster, Off
 *   mov eax, current_level; dec eax; and eax, 011b
 *   → variant = (level - 1) & 3
 * -------------------------------------------------------------------------- */
void monster_init_level(Monster *monsters, int level_num)
{
    int i;
    int variant = (level_num - 1) & 3;  /* MAIN.ASM:2933  and eax,011b */
    for (i = 0; i < NBS_MONSTER; i++) {
        memset(&monsters[i], 0, sizeof(Monster));
        monsters[i].variant = variant;
    }
}

/* --------------------------------------------------------------------------
 * monster_create
 *
 * MAIN.ASM:2950-2974  Create_Monster:
 *   inc counter_monster
 *   cmp counter_monster, delay  → if >= delay, call add_monster
 *
 * MAIN.ASM:2978-3038  Add_Monster:
 *   Find first inactive slot, set:
 *     pos_y = bord_y (0)
 *     pos_x = random(384) + bord_x   (random X in play area)
 *     sens_x = 1, sens_y = 1
 *     animation = 16 frames at speed 5
 * -------------------------------------------------------------------------- */
int monster_create(Monster *monsters, int *spawn_counter, Difficulty diff,
                   const int delai_override[3])
{
    int delay, i, idx;

    /* F3 P1-ASM-36: Prefer cfg-injected value (FILE.ASM:985-989 reads
     * Freq_Create_Monster into monster_delai_*). Fall back to compiled
     * defaults (FILE.ASM:1130-1132) when no override is supplied or the
     * entry is 0. */
    switch (diff) {
    case DIFFICULTY_HARD:   idx = 2; delay = MONSTER_DELAI_HARD;   break;
    case DIFFICULTY_MEDIUM: idx = 1; delay = MONSTER_DELAI_MEDIUM; break;
    default:                idx = 0; delay = MONSTER_DELAI_EASY;   break;
    }
    if (delai_override && delai_override[idx] > 0) {
        delay = delai_override[idx];
    }

    (*spawn_counter)++;
    if (*spawn_counter < delay) return 0;

    /* Reset counter and spawn — MAIN.ASM:2979  mov counter_monster,Off */
    *spawn_counter = 0;

    /* Find first inactive slot — MAIN.ASM:2982-2990 */
    for (i = 0; i < NBS_MONSTER; i++) {
        if (monsters[i].active || monsters[i].exploding) continue;

        /* MAIN.ASM:2996-3035  initialise monster */
        monsters[i].active = 1;
        monsters[i].exploding = 0;
        monsters[i].vx = 1;   /* MAIN.ASM:3002  mov [edx.sprite_sens_x],1 */
        monsters[i].vy = 1;   /* MAIN.ASM:3003  mov [edx.sprite_sens_y],1 */
        monsters[i].y  = PLAY_Y1;  /* MAIN.ASM:3001  mov [edx.sprite_pos_y],bord_y */

        /* Random X in play area: MAIN.ASM:3021-3025
         *   eax = max_x - bord_x = (528-32) - 112 = 384
         *   call get_random → eax = random(384)
         *   add eax, bord_x → eax = random + 112 */
        monsters[i].x = PLAY_X1 + GetRandomValue(0, PLAY_X2 - MONSTER_W - PLAY_X1);

        /* Animation: 16 frames, speed 5 */
        monsters[i].anim_frame = 0;
        monsters[i].anim_timer = MONSTER_SPEED;
        monsters[i].top_bounce_ctr = 0;
        return 1;
    }
    return 0;  /* all slots full */
}

/* --------------------------------------------------------------------------
 * monster_update
 *
 * MAIN.ASM:3089-3125  Refresh_Monster:
 *   For each active monster:
 *     call detect_colision_wall  (handled externally)
 *     call move_ball             (pos += sens)
 *     Top-wall bounce: if pos_y <= 0 for 32 frames, reverse sens_y
 * -------------------------------------------------------------------------- */
void monster_update(Monster *monsters)
{
    int i;
    for (i = 0; i < NBS_MONSTER; i++) {
        if (monsters[i].exploding) {
            /* Tick explosion animation.
             * MAIN.ASM:3064  sprite_to_delete = explo_nbs_anim + 2 = 15 */
            monsters[i].explo_timer--;
            if (monsters[i].explo_timer <= 0) {
                monsters[i].exploding = 0;  /* done */
            } else {
                /* Advance explosion frame every EXPLO_SPEED frames */
                monsters[i].explo_frame++;
                if (monsters[i].explo_frame >= EXPLO_NBS_ANIM)
                    monsters[i].explo_frame = EXPLO_NBS_ANIM - 1; /* hold last */
            }
            continue;
        }

        if (!monsters[i].active) continue;

        /* Move — MAIN.ASM:3107  call move_ball (pos += sens) */
        monsters[i].x += monsters[i].vx;
        monsters[i].y += monsters[i].vy;

        /* Wall bounce — MAIN.ASM:3097  call detect_colision_wall
         * Boundaries: X [112, 496], Y [0, 448]
         * MAIN.ASM:3014-3020 */
        if (monsters[i].x <= PLAY_X1) {
            monsters[i].x = PLAY_X1;
            monsters[i].vx = -monsters[i].vx;
        } else if (monsters[i].x >= PLAY_X2 - MONSTER_W) {
            monsters[i].x = PLAY_X2 - MONSTER_W;
            monsters[i].vx = -monsters[i].vx;
        }
        if (monsters[i].y >= SCREEN_H - MONSTER_H) {
            monsters[i].y = SCREEN_H - MONSTER_H;
            monsters[i].vy = -monsters[i].vy;
        }

        /* Top-wall bounce.
         *
         * ASM MAIN.ASM:3497-3530 detect_colision_wall runs BEFORE move_ball
         * and flips sens_y when (pos_y + sens_y) <= min_y (`jbe`). So in
         * normal play the monster bounces off the ceiling automatically
         * via wall collision — y>0 on the next check, counter_3 never
         * accumulates.
         *
         * MAIN.ASM:3109-3115 is a 32-frame edge-case quirk: if the
         * monster stays at y<=0 for 32 consecutive frames (only reachable
         * if detect_colision_wall's bounce fails for some reason), force
         * sens_y=-1. Writing -1 (up) when already at y<=0 is almost
         * certainly an ASM typo for `neg sens_y` (flip) — the original
         * dev's intent was to unstick the sprite. Taking -1 literally
         * keeps it stuck (user-reported bug "monsters stuck at ceiling").
         *
         * We match the ASM's EFFECTIVE behaviour (bounce off ceiling) via
         * the conditional flip that mirrors detect_colision_wall's
         * inverse_sens_y path. */
        if (monsters[i].y <= 0) {
            monsters[i].y = 0;
            if (monsters[i].vy < 0) monsters[i].vy = -monsters[i].vy;
        }

        /* Animation tick — MAIN.ASM:3030  sprite_shape_speed = monster_speed (5) */
        monsters[i].anim_timer--;
        if (monsters[i].anim_timer <= 0) {
            monsters[i].anim_timer = MONSTER_SPEED;
            monsters[i].anim_frame = (monsters[i].anim_frame + 1) % MONSTER_NBS_ANIM;
        }
    }
}

/* --------------------------------------------------------------------------
 * monster_kill
 *
 * MAIN.ASM:3049-3085  Del_Monster:
 *   status = kill; sens = 0; switch to explosion sprite
 *   pos -= 16 (centre 70px explosion over 32px monster)
 *   to_delete = explo_nbs_anim + 2 = 15
 * -------------------------------------------------------------------------- */
void monster_kill(Monster *m)
{
    if (!m->active) return;
    m->active = 0;
    m->exploding = 1;
    m->vx = 0;
    m->vy = 0;
    /* Centre explosion: MAIN.ASM:3057-3058  sub pos, 16 */
    m->x -= (EXPLO_W - MONSTER_W) / 2;  /* 70-32=38, /2=19 ≈ 16 */
    m->y -= (EXPLO_H - MONSTER_H) / 2;
    m->explo_frame = 0;
    m->explo_timer = EXPLO_NBS_ANIM + 2;  /* MAIN.ASM:3064  15 frames total */
}

/* --------------------------------------------------------------------------
 * monster_destroy_all
 *
 * MAIN.ASM:3151-3168  destruction_monster:
 *   For each active monster, call del_monster + play sound.
 * Iter 2 fix #7: SFX (iff_del_monster → MONSTOFF.wav → SFX_POWERUP_LOST) is
 * played per-monster inside the loop, mirroring the ASM. Pass NULL to suppress.
 * -------------------------------------------------------------------------- */
void monster_destroy_all(Monster *monsters, AudioState *audio)
{
    int i;
    for (i = 0; i < NBS_MONSTER; i++) {
        if (monsters[i].active) {
            monster_kill(&monsters[i]);
            if (audio) audio_play(audio, SFX_POWERUP_LOST);
        }
    }
}
