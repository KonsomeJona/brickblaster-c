#pragma once
/* screen_intro.h — Short TakoHi splash shown BEFORE the original intro.
 * The original Media Pocket intro + credit GIFs are played in
 * screen_intro_original.{c,h} afterwards.
 */

#include "screen_manager.h"
#include "input_frame.h"
#include <raylib.h>

#define TAKOHI_SPLASH_FRAMES   90   /* ~1.5 seconds */
#define TAKOHI_FADE_IN_FRAMES  30   /* ~0.5 s fade-in */

typedef struct {
    Texture2D logo;      /* TakoHi Logo_Eng.png */
    int       timer;     /* tick counter */
    float     alpha;     /* fade-in alpha 0..1 */
    int       loaded;
} IntroAssets;

void intro_assets_load(IntroAssets *assets);
void intro_assets_unload(IntroAssets *assets);
void intro_update(ScreenState *state, IntroAssets *assets, const FrameInput *input);
void intro_draw(IntroAssets *assets);
