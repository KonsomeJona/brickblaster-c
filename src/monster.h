#pragma once
/* monster.h — BrickBlaster Monster system
 *
 * Ground truth: MAIN.ASM init_Monster (2927), Create_Monster (2950),
 *               Add_Monster (2978), Refresh_Monster (3089),
 *               Del_Monster (3049), detect_colision_monster (3129),
 *               _detect_colision_monster (3174)
 *
 * Monsters are 32x32 sprites that bounce around the play area.
 * They collide with walls, bricks, and paddles (same physics as balls).
 * Ball-monster collision destroys the monster (explosion) and awards 3 pts.
 *
 * Spawn delay by difficulty (FILE.ASM:1130-1132):
 *   Easy:   600 frames (~10 sec)
 *   Medium: 500 frames (~8.3 sec)
 *   Hard:   300 frames (~5 sec)
 *
 * Max 4 simultaneous monsters (Blaster.inc:118  nbs_monster=4).
 * 4 monster sprite variants, selected by (level-1) & 3.
 */

#include "constants.h"
#include "audio.h"   /* AudioState — monster_destroy_all plays per-monster SFX */

/* Spawn delay constants (FILE.ASM:1130-1132) */
#define MONSTER_DELAI_EASY   600
#define MONSTER_DELAI_MEDIUM 500
#define MONSTER_DELAI_HARD   300

/* Monster score value (MAIN.ASM:3214  mov ecx,3; call inc_score)
 * inc_score loops ecx times, adding 10 per iteration → 3×10 = 30 pts */
#define MONSTER_SCORE        30

typedef struct {
    int x, y;           /* pixel position top-left */
    int vx, vy;         /* velocity: initially +1, +1 (MAIN.ASM:3002-3003) */
    int active;         /* 1 = alive and moving */
    int exploding;      /* 1 = explosion animation playing */
    int anim_frame;     /* current animation frame (0..15) */
    int anim_timer;     /* ticks until next frame (counts down from MONSTER_SPEED) */
    int explo_frame;    /* explosion animation frame (0..12) */
    int explo_timer;    /* ticks until explosion finishes */
    int variant;        /* 0-3: which sprite row (set by init_monster) */
    int top_bounce_ctr; /* sprite_counter_3: top-wall bounce delay (MAIN.ASM:3113-3122) */
} Monster;

/* Initialise monster system for a new level.
 * Selects sprite variant based on (level_num - 1) & 3.
 * MAIN.ASM:2927  init_Monster */
void monster_init_level(Monster *monsters, int level_num);

/* Per-frame spawn check. Increments counter; spawns when delay reached.
 * MAIN.ASM:2950  Create_Monster
 * Returns 1 if a monster was spawned.
 *
 * F3 P1-ASM-36: `delai_override[3]` supplies per-difficulty overrides from
 * Blaster.cfg Freq_Create_Monster (FILE.ASM:985-989). Pass NULL to use the
 * compiled-in MONSTER_DELAI_* defaults. Indices 0/1/2 = easy/medium/hard. */
int monster_create(Monster *monsters, int *spawn_counter, Difficulty diff,
                   const int delai_override[3]);

/* Per-frame update for all monsters: move, animate.
 * Wall bouncing and brick collision are handled externally in game_update.
 * MAIN.ASM:3089  Refresh_Monster */
void monster_update(Monster *monsters);

/* Transition a monster from alive to exploding.
 * MAIN.ASM:3049  Del_Monster */
void monster_kill(Monster *m);

/* Destroy ALL active monsters (powerup "Wrath of God").
 * MAIN.ASM:3151-3168  destruction_monster:
 *   loop over all active monsters, call del_monster AND play_sound iff_del_monster
 *   (→ MONSTOFF.wav) per monster.  Iter 2 fix #7: pass AudioState so the SFX
 *   plays once per kill inside the loop. Pass NULL to suppress SFX. */
void monster_destroy_all(Monster *monsters, AudioState *audio);
