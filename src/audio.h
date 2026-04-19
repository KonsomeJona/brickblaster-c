#pragma once
/* audio.h — BrickBlaster SFX system
 *
 * Maps the original MAIN.ASM IFF sound event names to raylib Sound.
 * Music streaming is handled separately by music_manager.c.
 *
 * ASM sound calls (MAIN.ASM):
 *   play_sound iff_cursor     MAIN.ASM:4217,4340  — ball hits paddle
 *   play_sound iff_normale    MAIN.ASM:4032       — ball hits normal brick
 *   play_sound iff_incassable MAIN.ASM:4055       — ball hits indestructible
 *   play_sound iff_lost_ball  MAIN.ASM:4581       — ball falls off screen
 *   play_sound iff_restart    MAIN.ASM:260,1207,4707 — new life / restart
 *   play_sound iff_option     MAIN.ASM:347        — powerup collected (generic)
 *   play_sound iff_speed_up   MAIN.ASM:3408       — speed increase triggered
 *   play_sound iff_shoot      MAIN.ASM:1930,2139  — laser fired
 *   play_sound iff_multi      MAIN.ASM:4108       — multi-ball spawn
 *   play_sound iff_next_level MAIN.ASM:4141       — level complete
 *   play_sound iff_game_over  MAIN.ASM:4796       — game over
 *   play_sound iff_new_life   MAIN.ASM:6429       — bonus life awarded
 *   play_sound iff_telepod    MAIN.ASM:4071       — teleporter activated
 *   play_sound iff_death      MAIN.ASM:6407       — DEATH powerup
 *   play_sound iff_explosion  MAIN.ASM:4795       — monster explosion
 *   play_sound iff_option_off MAIN.ASM:6322,6335  — timed powerup expired
 *   play_sound iff_large      MAIN.ASM:2550,2649  — large paddle powerup
 *   play_sound iff_small      MAIN.ASM:2596,2698  — small paddle powerup
 *   play_sound iff_lost_option MAIN.ASM:5592      — powerup fell off screen
 *
 * WAV filename mapping (validated against audio_manager.gd SOUND_PATHS):
 *   iff_cursor      → BOUNCE.wav  (ball ↔ paddle bounce)
 *   iff_normale     → BOUNCE.wav  (ball ↔ normal brick)
 *   iff_incassable  → WALL.wav    (ball ↔ indestructible)
 *   iff_lost_ball   → PERTEBAL.wav
 *   iff_restart     → RESTART.wav
 *   iff_option      → OPTIONON.wav
 *   iff_speed_up    → SPEEDUP.wav
 *   iff_shoot       → SHOOT.wav
 *   iff_multi       → BOOM.wav    (multi-ball spawn)
 *   iff_next_level  → NEXT.wav
 *   iff_game_over   → DEATH.wav
 *   iff_new_life    → NEWLIFE.wav
 *   iff_telepod     → TELEPOD.wav
 *   iff_death       → DEATH.wav   (DEATH powerup)
 *   iff_explosion   → BOOM.wav    (monster explosion)
 *   iff_option_off  → ENDOPT.wav
 *   iff_large       → LARGE.wav
 *   iff_small       → SMALL.wav
 *   iff_lost_option → MONSTOFF.wav (powerup lost — closest available)
 *   iff_night       → NIGHT.wav (NIGHT powerup — MAIN.ASM:6648)
 */

#include "raylib.h"

typedef enum {
    SFX_BOUNCE = 0,      /* ball bounces off paddle   — iff_cursor     → BOUNCE.wav  */
    SFX_BRICK_HIT,       /* ball hits normal brick    — iff_normale    → BOUNCE.wav  */
    SFX_WALL_HIT,        /* ball hits indestructible  — iff_incassable → WALL.wav    */
    SFX_BALL_LOST,       /* ball falls off screen     — iff_lost_ball  → PERTEBAL.wav */
    SFX_RESTART,         /* new life / restart        — iff_restart    → RESTART.wav */
    SFX_POWERUP_COLLECT, /* powerup collected (generic) — iff_option   → OPTIONON.wav */
    SFX_SPEEDUP,         /* speed increase            — iff_speed_up   → SPEEDUP.wav */
    SFX_SHOOT,           /* laser fired               — iff_shoot      → SHOOT.wav   */
    SFX_MULTI_BALL,      /* multi-ball spawn          — iff_multi      → BOOM.wav    */
    SFX_LEVEL_COMPLETE,  /* level done                — iff_next_level → NEXT.wav    */
    SFX_GAME_OVER,       /* game over                 — iff_game_over  → DEATH.wav   */
    SFX_NEW_LIFE,        /* extra life awarded        — iff_new_life   → NEWLIFE.wav */
    SFX_TELEPOD,         /* teleporter activated      — iff_telepod    → TELEPOD.wav */
    SFX_DEATH_POWERUP,   /* DEATH powerup collected   — iff_death      → DEATH.wav   */
    SFX_EXPLOSION,       /* monster/brick explosion   — iff_explosion  → BOOM.wav    */
    SFX_POWERUP_OFF,     /* timed powerup expired     — iff_option_off → ENDOPT.wav  */
    SFX_LARGE_PADDLE,    /* large paddle powerup      — iff_large      → LARGE.wav   */
    SFX_SMALL_PADDLE,    /* small paddle powerup      — iff_small      → SMALL.wav   */
    SFX_POWERUP_LOST,    /* powerup fell off screen   — iff_lost_option → MONSTOFF.wav */
    SFX_NIGHT,           /* NIGHT powerup             — iff_night      → NIGHT.wav   */
    SFX_COUNT
} SfxId;

typedef struct {
    Sound sfx[SFX_COUNT];
    int   sfx_loaded[SFX_COUNT];/* 1 for each SFX that was successfully loaded */
    int   sfx_enabled;          /* 1 = play SFX normally, 0 = all SFX muted */
} AudioState;

/* Initialise AudioState.  Calls LoadSound() for each WAV file.
 * Relies on InitAudioDevice() having been called in main.c. */
void audio_init(AudioState *a);

/* Play a sound effect by SFX ID.
 * Safe to call if the sound failed to load (no-op). */
void audio_play(AudioState *a, SfxId id);

/* Free all Sound resources. */
void audio_shutdown(AudioState *a);
