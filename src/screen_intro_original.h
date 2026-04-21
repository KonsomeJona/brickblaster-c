#pragma once
/* screen_intro_original.h — Startup intro sequence (MAIN.ASM:50-57).
 *
 * Plays two slides before dropping into the main menu:
 *   1. Media Pocket publisher splash — MAIN.ASM:54  File_Editor = Media.gif
 *      (shipped as assets/title/media.png).
 *   2. credit_b.png — MAIN.ASM:56  File_Credit_B (first credit letter).
 *
 * The animated `intro.flc` logo and the remaining credit letters
 * (m/g/c/w/e) are part of the Credits menu (MAIN.ASM:146-190 @@credit),
 * handled by screen_credits.c — they do NOT play at startup.
 */

#include "screen_manager.h"
#include "input_frame.h"
#include <raylib.h>

#define INTRO_ORIG_CREDIT_FADE      20   /* fade-in/out frames */
#define INTRO_ORIG_CREDIT_HOLD     120   /* hold ~2 s at 60 Hz */

typedef struct {
    Texture2D media_splash;    /* assets/title/media.png — Media Pocket publisher splash */
    Texture2D credit_b;        /* assets/credits/credit_b.png — first credit letter */
    Texture2D takohi_logo;     /* TakoHi logo shown as final slide (compile-time opt) */

    int       phase;           /* 0=media, 1=credit_b, 2=TakoHi, 3=done */
    int       timer;           /* per-sub-step tick counter */
    float     alpha;           /* fade-in/out alpha for current slide */

    int       loaded;
} IntroOriginalAssets;

void intro_original_assets_load(IntroOriginalAssets *assets);
void intro_original_assets_unload(IntroOriginalAssets *assets);
void intro_original_reset(IntroOriginalAssets *assets);
void intro_original_update(ScreenState *state, IntroOriginalAssets *assets,
                           const FrameInput *input);
void intro_original_draw(IntroOriginalAssets *assets);
