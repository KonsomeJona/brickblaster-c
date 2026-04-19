/* audio.c — BrickBlaster SFX system
 *
 * Maps MAIN.ASM IFF sound event names to raylib Sound.
 * 17 WAV SFX loaded from assets/audio/.
 * Music streaming is handled by music_manager.c.
 *
 * ASM play_sound event → SFX file mapping:
 *   iff_cursor      → BOUNCE.wav   MAIN.ASM:4217,4340
 *   iff_normale     → BOUNCE.wav   MAIN.ASM:4032
 *   iff_incassable  → WALL.wav     MAIN.ASM:4055
 *   iff_lost_ball   → PERTEBAL.wav MAIN.ASM:4581
 *   iff_restart     → RESTART.wav  MAIN.ASM:260,1207,4707
 *   iff_option      → OPTIONON.wav MAIN.ASM:347
 *   iff_speed_up    → SPEEDUP.wav  MAIN.ASM:3408
 *   iff_shoot       → SHOOT.wav    MAIN.ASM:1930,2139
 *   iff_multi       → BOOM.wav     MAIN.ASM:4108
 *   iff_next_level  → NEXT.wav     MAIN.ASM:4141
 *   iff_game_over   → DEATH.wav    MAIN.ASM:4796
 *   iff_new_life    → NEWLIFE.wav  MAIN.ASM:6429
 *   iff_telepod     → TELEPOD.wav  MAIN.ASM:4071
 *   iff_death       → DEATH.wav    MAIN.ASM:6407
 *   iff_explosion   → BOOM.wav     MAIN.ASM:4795
 *   iff_option_off  → ENDOPT.wav   MAIN.ASM:6322,6335
 *   iff_large       → LARGE.wav    MAIN.ASM:2550,2649
 *   iff_small       → SMALL.wav    MAIN.ASM:2596,2698
 *   iff_lost_option → MONSTOFF.wav MAIN.ASM:5592
 *   iff_night       → NIGHT.wav    MAIN.ASM:6648
 */

#include "audio.h"
#include "assets.h"
#include <stdio.h>   /* fprintf */
#include <string.h>  /* memset */

/* --------------------------------------------------------------------------
 * SFX filename table — parallel to SfxId enum order.
 * Each entry is the path relative to the executable working directory.
 *
 * Note: BOUNCE.wav is shared for two logical events (iff_cursor and iff_normale).
 *       BOOM.wav   is shared for iff_multi and iff_explosion.
 *       DEATH.wav  is shared for iff_game_over and iff_death.
 * -------------------------------------------------------------------------- */
static const char *sfx_paths[SFX_COUNT] = {
    ASSETS_BASE "audio/BOUNCE.wav",   /* SFX_BOUNCE        — iff_cursor    MAIN.ASM:4217 */
    ASSETS_BASE "audio/BOUNCE.wav",   /* SFX_BRICK_HIT     — iff_normale   MAIN.ASM:4032 */
    ASSETS_BASE "audio/WALL.wav",     /* SFX_WALL_HIT      — iff_incassable MAIN.ASM:4055 */
    ASSETS_BASE "audio/PERTEBAL.wav", /* SFX_BALL_LOST     — iff_lost_ball MAIN.ASM:4581 */
    ASSETS_BASE "audio/RESTART.wav",  /* SFX_RESTART       — iff_restart   MAIN.ASM:260  */
    ASSETS_BASE "audio/OPTIONON.wav", /* SFX_POWERUP_COLLECT — iff_option  MAIN.ASM:347  */
    ASSETS_BASE "audio/SPEEDUP.wav",  /* SFX_SPEEDUP       — iff_speed_up  MAIN.ASM:3408 */
    ASSETS_BASE "audio/SHOOT.wav",    /* SFX_SHOOT         — iff_shoot     MAIN.ASM:1930 */
    ASSETS_BASE "audio/BOOM.wav",     /* SFX_MULTI_BALL    — iff_multi     MAIN.ASM:4108 */
    ASSETS_BASE "audio/NEXT.wav",     /* SFX_LEVEL_COMPLETE — iff_next_level MAIN.ASM:4141 */
    ASSETS_BASE "audio/DEATH.wav",    /* SFX_GAME_OVER     — iff_game_over MAIN.ASM:4796 */
    ASSETS_BASE "audio/NEWLIFE.wav",  /* SFX_NEW_LIFE      — iff_new_life  MAIN.ASM:6429 */
    ASSETS_BASE "audio/TELEPOD.wav",  /* SFX_TELEPOD       — iff_telepod   MAIN.ASM:4071 */
    ASSETS_BASE "audio/DEATH.wav",    /* SFX_DEATH_POWERUP — iff_death     MAIN.ASM:6407 */
    ASSETS_BASE "audio/BOOM.wav",     /* SFX_EXPLOSION     — iff_explosion MAIN.ASM:4795 */
    ASSETS_BASE "audio/ENDOPT.wav",   /* SFX_POWERUP_OFF   — iff_option_off MAIN.ASM:6322 */
    ASSETS_BASE "audio/LARGE.wav",    /* SFX_LARGE_PADDLE  — iff_large     MAIN.ASM:2550 */
    ASSETS_BASE "audio/SMALL.wav",    /* SFX_SMALL_PADDLE  — iff_small     MAIN.ASM:2596 */
    ASSETS_BASE "audio/MONSTOFF.wav", /* SFX_POWERUP_LOST  — iff_lost_option MAIN.ASM:5592 */
    ASSETS_BASE "audio/NIGHT.wav",    /* SFX_NIGHT         — iff_night     MAIN.ASM:6648 */
};

/* --------------------------------------------------------------------------
 * audio_init
 * -------------------------------------------------------------------------- */
void audio_init(AudioState *a) {
    int i;
    memset(a, 0, sizeof(*a));

    /* Load each SFX.
     * LoadSound returns an empty Sound (frameCount==0) on failure. */
    for (i = 0; i < SFX_COUNT; i++) {
        a->sfx[i] = LoadSound(sfx_paths[i]);
        if (a->sfx[i].frameCount > 0) {
            a->sfx_loaded[i] = 1;
        } else {
            fprintf(stderr, "audio: failed to load SFX: %s\n", sfx_paths[i]);
            a->sfx_loaded[i] = 0;
        }
    }

    a->sfx_enabled  = 1;
}

/* --------------------------------------------------------------------------
 * audio_play
 *
 * Play a sound effect by ID.
 * MAIN.ASM "play_sound iff_*" macro → PlaySound().
 * -------------------------------------------------------------------------- */
void audio_play(AudioState *a, SfxId id) {
    if (id < 0 || id >= SFX_COUNT) return;
    if (!a->sfx_loaded[id]) return;
    if (!a->sfx_enabled) return;
    PlaySound(a->sfx[id]);
}

/* --------------------------------------------------------------------------
 * audio_shutdown
 * -------------------------------------------------------------------------- */
void audio_shutdown(AudioState *a) {
    int i;
    for (i = 0; i < SFX_COUNT; i++) {
        if (a->sfx_loaded[i]) {
            UnloadSound(a->sfx[i]);
            a->sfx_loaded[i] = 0;
        }
    }
}
