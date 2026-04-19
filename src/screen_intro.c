/* screen_intro.c — TakoHi splash (publisher logo) shown before the original
 * BrickBlaster intro. Fades in, holds briefly, then hands off to
 * STATE_INTRO_ORIGINAL which plays intro.flc + the 6 credit GIFs.
 */

#include "screen_intro.h"
#include "assets.h"
#include "constants.h"
#include "input_gamepad.h"
#include <raylib.h>

void intro_assets_load(IntroAssets *assets) {
    if (!assets || assets->loaded) return;

    assets->logo = assets_load_takohi_logo();
    if (assets->logo.id == 0) return;

    assets->timer  = 0;
    assets->alpha  = 0.0f;
    assets->loaded = 1;
}

void intro_assets_unload(IntroAssets *assets) {
    if (!assets || !assets->loaded) return;
    UnloadTexture(assets->logo);
    assets->loaded = 0;
}

static void goto_original_intro(ScreenState *state) {
    state->game_mode = STATE_INTRO_ORIGINAL;
}

void intro_update(ScreenState *state, IntroAssets *assets, const FrameInput *input) {
    if (!state || !assets || !assets->loaded) return;

    assets->timer++;

    if (assets->timer > 15) {
        int skip = (GetKeyPressed() != 0) || input->click_pressed ||
                   gamepad_confirm() || gamepad_start();
        if (skip) {
            goto_original_intro(state);
            return;
        }
    }

    if (assets->timer < TAKOHI_FADE_IN_FRAMES)
        assets->alpha = (float)assets->timer / (float)TAKOHI_FADE_IN_FRAMES;
    else
        assets->alpha = 1.0f;

    if (assets->timer >= TAKOHI_SPLASH_FRAMES) {
        goto_original_intro(state);
    }
}

void intro_draw(IntroAssets *assets) {
    if (!assets || !assets->loaded) return;

    ClearBackground((Color){8, 8, 16, 255});

    float max_w = SCREEN_W * 0.8f;
    float scale = 1.0f;
    if (assets->logo.width > max_w)
        scale = max_w / (float)assets->logo.width;

    float draw_w = assets->logo.width  * scale;
    float draw_h = assets->logo.height * scale;
    float x = (SCREEN_W - draw_w) / 2.0f;
    float y = (SCREEN_H - draw_h) / 2.0f;

    unsigned char a = (unsigned char)(assets->alpha * 255);
    Rectangle src = {0, 0, (float)assets->logo.width, (float)assets->logo.height};
    Rectangle dst = {x, y, draw_w, draw_h};
    DrawTexturePro(assets->logo, src, dst, (Vector2){0, 0}, 0.0f,
                   (Color){255, 255, 255, a});
}
