#pragma once
/* screen_credits.h — Plays the intro FLC (36 pre-converted frames) then
 * the 6 original credit GIFs (b, m, g, c, w, e), matching
 * MAIN.ASM:53-190 display_intro. Returns to menu 6 (miscellaneous).
 */

#include "screen_manager.h"
#include "input_frame.h"
#include "animation.h"
#include <raylib.h>

#define CREDITS_SLIDE_COUNT   6
#define CREDITS_FADE_FRAMES  20
#define CREDITS_HOLD_FRAMES 120
#define CREDITS_INTRO_FRAMES 36   /* assets/intro/frame_0001..0036.png   */
#define CREDITS_INTRO_FPS    18.0f /* standard FLC playback rate         */

typedef enum {
    CREDITS_PHASE_INTRO  = 0,  /* playing intro.flc-equivalent PNG seq  */
    CREDITS_PHASE_SLIDES = 1,  /* cycling the 6 credit letters          */
} CreditsPhase;

typedef struct {
    Animation intro;
    Texture2D slides[CREDITS_SLIDE_COUNT];
    int       slide_count;
    int       current;
    int       timer;
    float     alpha;
    int       loaded;
    CreditsPhase phase;
} CreditsAssets;

void credits_assets_load(CreditsAssets *assets);
void credits_assets_unload(CreditsAssets *assets);
void credits_reset(CreditsAssets *assets);
void credits_update(ScreenState *state, CreditsAssets *assets, const FrameInput *input);
void credits_draw(CreditsAssets *assets);
