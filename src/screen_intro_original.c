/* screen_intro_original.c — 36-frame intro.flc animation + 6 credit GIFs
 * + TakoHi publisher slide with URL.
 *
 * MAIN.ASM:53-190 display_intro — the sequence that runs before the menu.
 * FILE.ASM:54-114 load_intro_anim — FLI player: ~5 ticks/frame, fade-in,
 * loop until fli_last_frame.
 */

#include "screen_intro_original.h"
#include "assets.h"
#include "constants.h"
#include "input_gamepad.h"
#include <raylib.h>
#include <stdio.h>

/* Credit GIF file names in the original display order
 * (MAIN.ASM:57, 153, 162, 171, 180, 189). */
static const char *s_credit_files[INTRO_ORIG_CREDIT_COUNT] = {
    ASSETS_BASE "credits/credit_b.png",
    ASSETS_BASE "credits/credit_m.png",
    ASSETS_BASE "credits/credit_g.png",
    ASSETS_BASE "credits/credit_c.png",
    ASSETS_BASE "credits/credit_w.png",
    ASSETS_BASE "credits/credit_e.png",
};

void intro_original_assets_load(IntroOriginalAssets *a) {
    if (!a || a->loaded) return;

    a->frame_count = 0;
    for (int i = 0; i < INTRO_ORIG_FRAME_COUNT; i++) {
        char path[128];
        snprintf(path, sizeof(path),
                 ASSETS_BASE "intro/frame_%04d.png", i + 1);
        Texture2D t = LoadTexture(path);
        if (t.id == 0) break;
        a->frames[a->frame_count++] = t;
    }

    a->credit_count = 0;
    for (int i = 0; i < INTRO_ORIG_CREDIT_COUNT; i++) {
        Texture2D t = LoadTexture(s_credit_files[i]);
        if (t.id == 0) continue;
        a->credits[a->credit_count++] = t;
    }

#if defined(TAKOHI_BRANDING)
    /* TakoHi publisher logo (shown as final slide before menu).
     * Disabled when -DTAKOHI_BRANDING=0 for vanilla builds. */
    a->takohi_logo = assets_load_takohi_logo();
#else
    a->takohi_logo = (Texture2D){0};
#endif

    intro_original_reset(a);
    a->loaded = 1;
}

void intro_original_assets_unload(IntroOriginalAssets *a) {
    if (!a || !a->loaded) return;
    for (int i = 0; i < a->frame_count;  i++) UnloadTexture(a->frames[i]);
    for (int i = 0; i < a->credit_count; i++) UnloadTexture(a->credits[i]);
    if (a->takohi_logo.id != 0) UnloadTexture(a->takohi_logo);
    a->frame_count  = 0;
    a->credit_count = 0;
    a->loaded       = 0;
}

void intro_original_reset(IntroOriginalAssets *a) {
    if (!a) return;
    a->phase         = 0;
    a->current_index = 0;
    a->timer         = 0;
    a->alpha         = 0.0f;
}

static void goto_menu(ScreenState *state) {
    state->current_menu = 1;
    state->game_mode    = STATE_MENU;
}

void intro_original_update(ScreenState *state, IntroOriginalAssets *a,
                           const FrameInput *input) {
    if (!state || !a || !a->loaded) { goto_menu(state); return; }

    a->timer++;

    /* Skip everything on input after a short grace period. */
    if (a->timer > 10) {
        int skip = (GetKeyPressed() != 0) || input->click_pressed ||
                   gamepad_confirm() || gamepad_start() || gamepad_back();
        if (skip) { intro_original_reset(a); goto_menu(state); return; }
    }

    /* Phase 0 — FLI animation: advance one frame every N ticks. */
    if (a->phase == 0) {
        if (a->frame_count == 0) { a->phase = 1; a->timer = 0; return; }

        int step = a->timer / INTRO_ORIG_TICKS_PER_FRAME;
        if (step >= a->frame_count) {
            a->phase         = 1;
            a->current_index = 0;
            a->timer         = 0;
            a->alpha         = 0.0f;
            return;
        }
        a->current_index = step;
        return;
    }

    /* Phase 1 — credit GIFs: fade-in -> hold -> fade-out, then next. */
    if (a->phase == 1) {
        if (a->credit_count == 0 || a->current_index >= a->credit_count) {
            a->phase = 2;
            a->timer = 0;
            a->alpha = 0.0f;
            return;
        }

        int fade  = INTRO_ORIG_CREDIT_FADE;
        int hold  = INTRO_ORIG_CREDIT_HOLD;
        int total = fade + hold + fade;

        if (a->timer < fade) {
            a->alpha = (float)a->timer / (float)fade;
        } else if (a->timer < fade + hold) {
            a->alpha = 1.0f;
        } else if (a->timer < total) {
            a->alpha = 1.0f - (float)(a->timer - fade - hold) / (float)fade;
        } else {
            a->current_index++;
            a->timer = 0;
            a->alpha = 0.0f;
            if (a->current_index >= a->credit_count) {
                a->phase = 2;
                a->timer = 0;
                a->alpha = 0.0f;
            }
        }
        return;
    }

    /* Phase 2 — TakoHi logo + URL: same fade timing as credit GIFs. */
    if (a->phase == 2) {
        if (a->takohi_logo.id == 0) {
            a->phase = 3;
            goto_menu(state);
            return;
        }

        int fade  = INTRO_ORIG_CREDIT_FADE;
        int hold  = INTRO_ORIG_CREDIT_HOLD;
        int total = fade + hold + fade;

        if (a->timer < fade) {
            a->alpha = (float)a->timer / (float)fade;
        } else if (a->timer < fade + hold) {
            a->alpha = 1.0f;
        } else if (a->timer < total) {
            a->alpha = 1.0f - (float)(a->timer - fade - hold) / (float)fade;
        } else {
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

    if (a->phase == 0) {
        if (a->current_index >= 0 && a->current_index < a->frame_count)
            draw_centered(a->frames[a->current_index], 255);
    } else if (a->phase == 1) {
        if (a->current_index >= 0 && a->current_index < a->credit_count) {
            unsigned char al = (unsigned char)(a->alpha * 255.0f);
            draw_centered(a->credits[a->current_index], al);
        }
    } else if (a->phase == 2) {
        /* TakoHi logo centred, scaled to 50% canvas width, + URL below */
        unsigned char al = (unsigned char)(a->alpha * 255.0f);
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
