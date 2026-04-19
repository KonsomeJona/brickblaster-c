#pragma once
/* screen_intro_original.h — Original BrickBlaster intro sequence.
 *
 * Plays the 36-frame intro.flc animation followed by the 6 credit GIFs
 * (b, m, g, c, w, e), each with a fade-in/hold/fade-out. Exactly matches
 * MAIN.ASM:53-190 display_intro / FILE.ASM:54-114 load_intro_anim.
 */

#include "screen_manager.h"
#include "input_frame.h"
#include <raylib.h>

#define INTRO_ORIG_FRAME_COUNT      36
#define INTRO_ORIG_TICKS_PER_FRAME   5   /* FILE.ASM:103 : ~5 ticks / frame */
#define INTRO_ORIG_CREDIT_COUNT      6   /* credit_b, m, g, c, w, e */
#define INTRO_ORIG_CREDIT_FADE      20   /* fade-in/out frames */
#define INTRO_ORIG_CREDIT_HOLD     120   /* hold ~2 s at 60 Hz */

typedef struct {
    Texture2D frames[INTRO_ORIG_FRAME_COUNT];
    int       frame_count;

    Texture2D credits[INTRO_ORIG_CREDIT_COUNT];
    int       credit_count;

    Texture2D takohi_logo;     /* TakoHi logo shown as final slide before menu */

    int       phase;           /* 0=FLI anim, 1=credits, 2=TakoHi slide, 3=done */
    int       current_index;   /* frame index (phase 0) or credit index (phase 1) */
    int       timer;           /* per-sub-step tick counter */
    float     alpha;           /* fade-in/out alpha for the credit currently shown */

    int       loaded;
} IntroOriginalAssets;

void intro_original_assets_load(IntroOriginalAssets *assets);
void intro_original_assets_unload(IntroOriginalAssets *assets);
void intro_original_reset(IntroOriginalAssets *assets);
void intro_original_update(ScreenState *state, IntroOriginalAssets *assets,
                           const FrameInput *input);
void intro_original_draw(IntroOriginalAssets *assets);
