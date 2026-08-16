#include "monster.h"
#include "asm_random.h"  /* 1999 generator — asm_get_random is 0..n INCLUSIVE */
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
    int variant = (level_num - 1) & 3;  /* MAIN.ASM:2937  and eax,011b */
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
    int delay, idx;

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

    /* Spawn gate — MAIN.ASM:2969-2971  inc counter_monster /
     * cmp counter_monster,eax / jb @@end */
    (*spawn_counter)++;
    if (*spawn_counter < delay) return 0;

    /* MAIN.ASM:2972  call add_monster */
    return monster_add_now(monsters, spawn_counter, diff, delai_override);
}

/* --------------------------------------------------------------------------
 * monster_add_now
 *
 * Immediate spawn, no periodic gate — mirrors Add_Monster (MAIN.ASM:2978).
 * option_add_monster_p (MAIN.ASM:6765-6769) calls add_monster directly:
 *   option_add_monster_p:
 *     call add_monster
 *     mov current_option,off
 * Add_Monster still resets the periodic counter as its first act:
 *   MAIN.ASM:2981  mov counter_monster,Off
 * Same signature as monster_create (contract with game.c); diff and
 * delai_override are unused here.
 * -------------------------------------------------------------------------- */
int monster_add_now(Monster *monsters, int *spawn_counter, Difficulty diff,
                    const int delai_override[3])
{
    int i;
    (void)diff;
    (void)delai_override;

    /* MAIN.ASM:2981  Add_Monster: mov counter_monster,Off */
    *spawn_counter = 0;

    /* Find first inactive slot — MAIN.ASM:2982-2990 */
    for (i = 0; i < NBS_MONSTER; i++) {
        if (monsters[i].active || monsters[i].exploding) continue;

        /* MAIN.ASM:2996-3035  initialise monster */
        monsters[i].active = 1;
        monsters[i].exploding = 0;
        monsters[i].vx = 1;   /* MAIN.ASM:2998  mov [edx.sprite_sens_x],1 */
        monsters[i].vy = 1;   /* MAIN.ASM:2999  mov [edx.sprite_sens_y],1 */
        monsters[i].y  = PLAY_Y1;  /* MAIN.ASM:3001  mov [edx.sprite_pos_y],bord_y */

        /* Random X in play area: MAIN.ASM:3026-3030
         *   mov eax,[edx.sprite_max_x] / sub eax,bord_x   → 384
         *   call get_random                               → 0..384 inclusive
         *   add eax,bord_x                                → 112..496 */
        monsters[i].x = PLAY_X1 + asm_get_random(PLAY_X2 - MONSTER_W - PLAY_X1);

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
            /* Explosion cadence — DRAW.ASM:388-392 (Refresh_Sprites):
             *   dec [edx.sprite_current_speed]
             *   cmp [edx.sprite_current_speed],0
             *   jns @@next                        → skip while >= 0
             *   mov eax,[edx.sprite_shape_speed]
             *   mov [edx.sprite_current_speed],eax → reload
             * With explo_speed = 1 (Blaster.inc:131, set by Del_Monster
             * MAIN.ASM:3081-3082) the animation advances one frame every
             * TWO screen frames. anim_timer plays sprite_current_speed. */
            monsters[i].anim_timer--;
            if (monsters[i].anim_timer >= 0) continue;
            monsters[i].anim_timer = EXPLO_SPEED;  /* reload — DRAW.ASM:391-392 */

            /* sprite_to_delete = explo_nbs_anim + 2 = 15 (MAIN.ASM:3077-3078),
             * decremented once per animation ADVANCE (DRAW.ASM:394-398).
             *   dec sprite_to_delete / cmp sprite_to_delete,On / jne @@ok
             * stops at 1 (On = 1, Blaster.inc:431) — 14 advances, not 15.
             * Then DRAW.ASM:403-416:
             *   cmp sprite_current_shape,1 / jbe @@reset
             *   @@reset: current_adrs = adrs ; current_shape = nbs_shape
             *   @@inc:   current_adrs += size_x+next_shape ; dec current_shape
             * current_shape starts at 1 (MAIN.ASM:3080), so the FIRST advance
             * resets to frame 0 rather than stepping off it. */
            monsters[i].explo_timer--;
            if (monsters[i].explo_timer <= 1) {
                monsters[i].exploding = 0;               /* DRAW.ASM:399-400 */
            } else if (monsters[i].explo_shape <= 1) {
                monsters[i].explo_frame = 0;             /* DRAW.ASM:412-413 */
                monsters[i].explo_shape = EXPLO_NBS_ANIM;/* DRAW.ASM:414-415 */
            } else {
                monsters[i].explo_frame++;               /* DRAW.ASM:406-409 */
                monsters[i].explo_shape--;
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
 *   pos -= 16 (MAIN.ASM:3059-3060)
 *   to_delete = explo_nbs_anim + 2 = 15
 * -------------------------------------------------------------------------- */
void monster_kill(Monster *m)
{
    if (!m->active) return;
    m->active = 0;
    m->exploding = 1;
    m->vx = 0;
    m->vy = 0;
    /* MAIN.ASM:3059-3060  sub [edx.sprite_pos_x],16 / sub [edx.sprite_pos_y],16
     * The ASM shifts by exactly 16 px (not the geometric (70-32)/2 = 19). */
    m->x -= 16;
    m->y -= 16;
    m->explo_frame = 0;
    m->explo_shape = 1;                 /* MAIN.ASM:3080  sprite_current_shape = 1 */
    /* MAIN.ASM:3077-3078  mov sprite_to_delete,explo_nbs_anim / add ..,2 = 15,
     * consumed one step per animation advance (every 2 frames — see
     * monster_update). */
    m->explo_timer = EXPLO_NBS_ANIM + 2;
    /* MAIN.ASM:3081-3082  sprite_shape_speed = sprite_current_speed =
     * explo_speed (1). anim_timer doubles as sprite_current_speed. */
    m->anim_timer = EXPLO_SPEED;
}

/* --------------------------------------------------------------------------
 * monster_destroy_all
 *
 * MAIN.ASM:3151-3168  destruction_monster:
 *   For each active monster, call del_monster + play sound.
 * Iter 2 fix #7: SFX (iff_del_monster → MONSTOFF.wav → SFX_DEL_MONSTER) is
 * played per-monster inside the loop, mirroring the ASM. Pass NULL to suppress.
 * -------------------------------------------------------------------------- */
void monster_destroy_all(Monster *monsters, AudioState *audio)
{
    int i;
    for (i = 0; i < NBS_MONSTER; i++) {
        if (monsters[i].active) {
            monster_kill(&monsters[i]);
            if (audio) audio_play(audio, SFX_DEL_MONSTER);
        }
    }
}
