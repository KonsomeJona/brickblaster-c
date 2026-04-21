/* screen_intro_original.c — Startup intro sequence (MAIN.ASM:50-57).
 *
 *   display_intro_2 File_Editor      → Media.gif (publisher splash)
 *   display_intro   File_Credit_B    → credit_b.gif (first credit letter)
 *
 * The animated `intro.flc` logo and credit_m/g/c/w/e letters are part of
 * the Credits menu (MAIN.ASM:146-190 @@credit, implemented in
 * screen_credits.c) and do NOT play at startup.
 *
 * The optional TakoHi publisher slide is appended under -DTAKOHI_BRANDING.
 * It has no ASM counterpart — it's our own porter credit, gated so
 * vanilla builds match 1999 exactly.
 *
 * David feedback (2026-04-21): the previous implementation looped the
 * FLC frames + all six credits at startup; the FLC logo and the extra
 * credits now stay in the Credits menu where the ASM puts them.
 */

#include "screen_intro_original.h"
#include "assets.h"
#include "constants.h"
#include "input_gamepad.h"
#include <raylib.h>

void intro_original_assets_load(IntroOriginalAssets *a) {
    if (!a || a->loaded) return;

    a->media_splash = LoadTexture(ASSETS_BASE "title/media.png");
    a->credit_b     = LoadTexture(ASSETS_BASE "credits/credit_b.png");

#if defined(TAKOHI_BRANDING)
    /* TakoHi publisher logo (final slide before menu, porter credit).
     * Disabled when -DTAKOHI_BRANDING=0 for a strict 1999-parity build. */
    a->takohi_logo = assets_load_takohi_logo();
#else
    a->takohi_logo = (Texture2D){0};
#endif

    intro_original_reset(a);
    a->loaded = 1;
}

void intro_original_assets_unload(IntroOriginalAssets *a) {
    if (!a || !a->loaded) return;
    if (a->media_splash.id != 0) UnloadTexture(a->media_splash);
    if (a->credit_b.id     != 0) UnloadTexture(a->credit_b);
    if (a->takohi_logo.id  != 0) UnloadTexture(a->takohi_logo);
    a->loaded = 0;
}

void intro_original_reset(IntroOriginalAssets *a) {
    if (!a) return;
    a->phase = 0;
    a->timer = 0;
    a->alpha = 0.0f;
}

static void goto_menu(ScreenState *state) {
    state->current_menu = 1;
    state->game_mode    = STATE_MENU;
}

/* Advance the fade-in/hold/fade-out timer for the current slide.
 * Returns 1 when the slide is finished, 0 while it is playing (with
 * `*out_alpha` set for the draw pass). */
static int update_fade_slide(int timer, float *out_alpha) {
    int fade  = INTRO_ORIG_CREDIT_FADE;
    int hold  = INTRO_ORIG_CREDIT_HOLD;
    int total = fade + hold + fade;

    if (timer < fade) {
        *out_alpha = (float)timer / (float)fade;
        return 0;
    }
    if (timer < fade + hold) {
        *out_alpha = 1.0f;
        return 0;
    }
    if (timer < total) {
        *out_alpha = 1.0f - (float)(timer - fade - hold) / (float)fade;
        return 0;
    }
    *out_alpha = 0.0f;
    return 1;
}

void intro_original_update(ScreenState *state, IntroOriginalAssets *a,
                           const FrameInput *input) {
    if (!state || !a || !a->loaded) { goto_menu(state); return; }

    a->timer++;

    /* Skip the whole intro on any input after a short grace period. */
    if (a->timer > 10) {
        int skip = (GetKeyPressed() != 0) || input->click_pressed ||
                   gamepad_confirm() || gamepad_start() || gamepad_back();
        if (skip) { intro_original_reset(a); goto_menu(state); return; }
    }

    /* Phase 0 — Media Pocket publisher splash (MAIN.ASM:54 File_Editor). */
    if (a->phase == 0) {
        if (a->media_splash.id == 0) {
            /* No asset → skip straight to credit_b */
            a->phase = 1;
            a->timer = 0;
            return;
        }
        if (update_fade_slide(a->timer, &a->alpha)) {
            a->phase = 1;
            a->timer = 0;
            a->alpha = 0.0f;
        }
        return;
    }

    /* Phase 1 — first credit letter (MAIN.ASM:56 File_Credit_B). */
    if (a->phase == 1) {
        if (a->credit_b.id == 0) {
            a->phase = 2;
            a->timer = 0;
            return;
        }
        if (update_fade_slide(a->timer, &a->alpha)) {
            a->phase = 2;
            a->timer = 0;
            a->alpha = 0.0f;
        }
        return;
    }

    /* Phase 2 — TakoHi logo + URL (compile-time opt, porter credit). */
    if (a->phase == 2) {
        if (a->takohi_logo.id == 0) {
            a->phase = 3;
            goto_menu(state);
            return;
        }
        if (update_fade_slide(a->timer, &a->alpha)) {
            a->phase = 3;
            goto_menu(state);
        }
    }
}

static void draw_centered(Texture2D tex, unsigned char alpha) {
    if (tex.id == 0) return;

    float sx = (float)SCREEN_W / (float)tex.width;
    float sy = (float)SCREEN_H / (float)tex.height;
    float s  = (sx < sy) ? sx : sy;
    float w  = tex.width  * s;
    float h  = tex.height * s;
    float x  = (SCREEN_W - w) / 2.0f;
    float y  = (SCREEN_H - h) / 2.0f;
    Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
    Rectangle dst = {x, y, w, h};
    DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f,
                   (Color){255, 255, 255, alpha});
}

void intro_original_draw(IntroOriginalAssets *a) {
    if (!a || !a->loaded) return;

    ClearBackground((Color){0, 0, 0, 255});

    unsigned char al = (unsigned char)(a->alpha * 255.0f);

    if (a->phase == 0) {
        draw_centered(a->media_splash, al);
    } else if (a->phase == 1) {
        draw_centered(a->credit_b, al);
    } else if (a->phase == 2) {
        /* TakoHi logo centred, scaled to 50% canvas width, + URL below */
        if (a->takohi_logo.id != 0) {
            float max_w = SCREEN_W * 0.5f;
            float sc = (a->takohi_logo.width > max_w)
                     ? max_w / (float)a->takohi_logo.width : 1.0f;
            float dw = a->takohi_logo.width  * sc;
            float dh = a->takohi_logo.height * sc;
            float lx = (SCREEN_W - dw) / 2.0f;
            float ly = (SCREEN_H - dh) / 2.0f - 30;
            Rectangle src = {0, 0, (float)a->takohi_logo.width,
                                    (float)a->takohi_logo.height};
            Rectangle dst = {lx, ly, dw, dh};
            DrawTexturePro(a->takohi_logo, src, dst, (Vector2){0, 0}, 0.0f,
                           (Color){255, 255, 255, al});

            /* URL below logo */
            const char *url = "www.takohi.me";
            int fs = 16;
            int tw = MeasureText(url, fs);
            DrawText(url, (SCREEN_W - tw) / 2, (int)(ly + dh + 20), fs,
                     (Color){180, 180, 200, al});
        }
    }
}
