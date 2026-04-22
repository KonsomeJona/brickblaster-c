/* screen_credits.c — Original 6 credit GIFs played in sequence with fade.
 * Triggered from menu 6 (miscellaneous → credits).
 * Mirrors MAIN.ASM:53-190 display_intro/display_intro_2.
 */

#include "screen_credits.h"
#include "assets.h"
#include "constants.h"
#include "input_gamepad.h"
#include <raylib.h>

static const char *s_credit_files[CREDITS_SLIDE_COUNT] = {
    ASSETS_BASE "credits/credit_b.png",
    ASSETS_BASE "credits/credit_m.png",
    ASSETS_BASE "credits/credit_g.png",
    ASSETS_BASE "credits/credit_c.png",
    ASSETS_BASE "credits/credit_w.png",
    ASSETS_BASE "credits/credit_e.png",
};

void credits_assets_load(CreditsAssets *a) {
    if (!a || a->loaded) return;
    a->slide_count = 0;
    for (int i = 0; i < CREDITS_SLIDE_COUNT; i++) {
        Texture2D t = LoadTexture(s_credit_files[i]);
        if (t.id == 0) continue;
        a->slides[a->slide_count++] = t;
    }
    /* intro.flc frames are loaded lazily on first entry — 36 PNG
     * decodes at startup would slow cold launch for a screen most
     * users never open. Zero-init the Animation struct so
     * animation_is_finished / frame_count == 0 short-circuits
     * correctly until credits_intro_ensure_loaded runs. */
    Animation zero = {0};
    a->intro = zero;
    credits_reset(a);
    a->loaded = 1;
}

/* Load the intro PNG sequence on first Credits entry.  Safe to call
 * repeatedly — exits immediately once frames are present. */
static void credits_intro_ensure_loaded(CreditsAssets *a) {
    if (a->intro.frame_count > 0) return;
    a->intro = animation_load(ASSETS_BASE "intro",
                              CREDITS_INTRO_FRAMES, CREDITS_INTRO_FPS);
}

void credits_assets_unload(CreditsAssets *a) {
    if (!a || !a->loaded) return;
    for (int i = 0; i < a->slide_count; i++) UnloadTexture(a->slides[i]);
    a->slide_count = 0;
    animation_unload(&a->intro);
    a->loaded      = 0;
}

void credits_reset(CreditsAssets *a) {
    if (!a) return;
    a->current = 0;
    a->timer   = 0;
    a->alpha   = 0.0f;
    a->phase   = CREDITS_PHASE_INTRO;
    animation_reset(&a->intro);
}

void credits_update(ScreenState *state, CreditsAssets *a, const FrameInput *input) {
    if (!state) return;
    if (!a || !a->loaded) {
        state->game_mode    = STATE_MENU;
        state->current_menu = 6;
        return;
    }

    a->timer++;

    int skip = (a->timer > 10) &&
               (GetKeyPressed() != 0 || input->click_pressed ||
                gamepad_confirm() || gamepad_back() || gamepad_start() ||
                IsKeyPressed(KEY_ESCAPE));
    if (skip) {
        credits_reset(a);
        state->game_mode    = STATE_MENU;
        state->current_menu = 6;
        return;
    }

    /* Phase 1: intro anim (MAIN.ASM:145-190 @@credit — intro.flc plays
     * first). Lazy-load on first visit, then advance; when it finishes,
     * switch to slides. */
    if (a->phase == CREDITS_PHASE_INTRO) {
        credits_intro_ensure_loaded(a);
        if (a->intro.frame_count > 0) {
            animation_update(&a->intro);
            if (animation_is_finished(&a->intro)) {
                a->phase = CREDITS_PHASE_SLIDES;
                a->timer = 0;
                a->current = 0;
                a->alpha = 0.0f;
            }
        } else {
            /* Intro assets missing on disk — skip to slides. */
            a->phase = CREDITS_PHASE_SLIDES;
            a->timer = 0;
        }
        return;
    }

    /* MAIN.ASM:145-197 `@@credit_again` loops the whole sequence until the
     * user hits ESC (mapped to escape/back/click above). If no slides
     * loaded at all we bail out safely. */
    if (a->slide_count == 0) {
        credits_reset(a);
        state->game_mode    = STATE_MENU;
        state->current_menu = 6;
        return;
    }
    if (a->current >= a->slide_count) {
        /* Guard against stale state — wrap and fall through. */
        a->current = 0;
        a->timer   = 0;
        a->alpha   = 0.0f;
    }

    int fade  = CREDITS_FADE_FRAMES;
    int hold  = CREDITS_HOLD_FRAMES;
    int total = fade + hold + fade;

    if (a->timer < fade) {
        a->alpha = (float)a->timer / (float)fade;
    } else if (a->timer < fade + hold) {
        a->alpha = 1.0f;
    } else if (a->timer < total) {
        a->alpha = 1.0f - (float)(a->timer - fade - hold) / (float)fade;
    } else {
        a->current++;
        a->timer = 0;
        a->alpha = 0.0f;
        /* MAIN.ASM:197 `jmp @@credit_again` — loop back to the first slide
         * instead of exiting. Only ESC/click/back (handled above) exits. */
        if (a->current >= a->slide_count) {
            a->current = 0;
        }
    }
}

void credits_draw(CreditsAssets *a) {
    if (!a || !a->loaded) return;

    ClearBackground((Color){0, 0, 0, 255});

    /* Phase 1: intro — center the current frame full-screen. */
    if (a->phase == CREDITS_PHASE_INTRO) {
        if (a->intro.frame_count > 0) {
            int fi = a->intro.current_frame;
            if (fi < 0) fi = 0;
            if (fi >= a->intro.frame_count) fi = a->intro.frame_count - 1;
            Texture2D t = a->intro.frames[fi];
            if (t.id != 0) {
                int x = (SCREEN_W - t.width)  / 2;
                int y = (SCREEN_H - t.height) / 2;
                DrawTexture(t, x, y, WHITE);
            }
        }
        return;
    }

    if (a->current < 0 || a->current >= a->slide_count) return;
    Texture2D t = a->slides[a->current];
    if (t.id == 0) return;

    float sx = (float)SCREEN_W / (float)t.width;
    float sy = (float)SCREEN_H / (float)t.height;
    float s  = (sx < sy) ? sx : sy;
    float w  = t.width  * s;
    float h  = t.height * s;
    float x  = (SCREEN_W - w) / 2.0f;
    float y  = (SCREEN_H - h) / 2.0f;
    Rectangle src = { 0, 0, (float)t.width, (float)t.height };
    Rectangle dst = { x, y, w, h };
    unsigned char al = (unsigned char)(a->alpha * 255.0f);
    DrawTexturePro(t, src, dst, (Vector2){0, 0}, 0.0f,
                   (Color){255, 255, 255, al});
}
