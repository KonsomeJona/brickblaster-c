#pragma once
/* screen_credits.h — Replays the 6 original credit GIFs (b, m, g, c, w, e),
 * matching MAIN.ASM:53-190 display_intro. Returns to menu 6 (miscellaneous).
 */

#include "screen_manager.h"
#include "input_frame.h"
#include <raylib.h>

#define CREDITS_SLIDE_COUNT   6
#define CREDITS_FADE_FRAMES  20
#define CREDITS_HOLD_FRAMES 120

typedef struct {
    Texture2D slides[CREDITS_SLIDE_COUNT];
    int       slide_count;
    int       current;
    int       timer;
    float     alpha;
    int       loaded;
} CreditsAssets;

void credits_assets_load(CreditsAssets *assets);
void credits_assets_unload(CreditsAssets *assets);
void credits_reset(CreditsAssets *assets);
void credits_update(ScreenState *state, CreditsAssets *assets, const FrameInput *input);
void credits_draw(CreditsAssets *assets);
