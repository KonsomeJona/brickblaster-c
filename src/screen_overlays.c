/* screen_overlays.c — Ready / Pause / Game Over / Demo overlays.
 *
 * All text rendered with FONTE bitmap font at the original panel_info
 * position (MAIN.ASM:965-966, 1261, 4681; FONTE.ASM:292 Print).
 *
 * Original option_text_* strings are 18 chars, pre-padded with spaces
 * for centring in the 270-px panel (18 chars × 15 px = 270 px).
 *
 * P1-ASM-32 fix: route all strings through i18n() so FR/ES users see
 * their localised overlay (matches Blaster.cfg / Blaster_es.cfg).
 */

#include "screen_overlays.h"
#include "input_gamepad.h"
#include "i18n.h"
#include <stddef.h>
#include <stdio.h>

static BitmapFont s_font;
static int s_font_ready = 0;

void overlays_init(Assets *assets) {
    if (!assets || !assets->font_sheet_loaded) return;
    font_init(&s_font, &assets->font_sheet);
    s_font_ready = 1;
}

/* Draw an 18-char option_text at the panel_info position (195, 446). */
static void draw_option_text(const char *text) {
    if (!s_font_ready) return;
    font_draw_string(&s_font, text, PANEL_INFO_POS_X, PANEL_INFO_POS_Y, WHITE);
}

/* MAIN.ASM:965, 2757 — option_text_ready */
void draw_ready_screen(ScreenState *state) {
    (void)state;
    draw_option_text(i18n(STR_OPT_READY));
}

/* MAIN.ASM:1261 — option_text_paused */
void draw_pause_screen(ScreenState *state) {
    (void)state;
    draw_option_text(i18n(STR_OPT_PAUSED));
}

/* MAIN.ASM:4681 — option_text_over */
void draw_game_over_screen(ScreenState *state, int *timer,
                           int game_mode, int winner) {
    (void)state; (void)timer;
    draw_option_text(i18n(STR_OPT_GAME_OVER));

    /* Dual mode: show winner on an extra FONTE line above panel_info.
     * Route through i18n so FR/ES/DE/IT/PT users see localised banners
     * (F2-OVERLAY-02). 18-char FONTE-padded strings per option_text
     * convention. */
    if (game_mode == 2 && s_font_ready) {
        const char *msg = (winner == 0) ? i18n(STR_OPT_P1_WINS)
                        : (winner == 1) ? i18n(STR_OPT_P2_WINS)
                                        : i18n(STR_OPT_DRAW);
        font_draw_string(&s_font, msg, PANEL_INFO_POS_X,
                         PANEL_INFO_POS_Y - 30, WHITE);
    }
}

/* MAIN.ASM:6997 — option_text_demo */
void draw_demo_overlay(void) {
    draw_option_text(i18n(STR_OPT_DEMO));
}

/* MAIN.ASM:4698 — option_text_again. Shown briefly after losing a life,
 * then replaced by the READY overlay (P1-ASM-17). */
void draw_play_again_screen(ScreenState *state) {
    (void)state;
    draw_option_text(i18n(STR_OPT_PLAY_AGAIN));
}

/* Pause input: gamepad B exits to menu. */
int pause_handle_input(ScreenState *state, const FrameInput *input) {
    if (!state) return 0;

    /* Gamepad B = exit to menu */
    if (gamepad_back()) {
        state->game_mode    = STATE_MENU;
        state->current_menu = 1;
        return 0;
    }
    (void)input;
    return 0;
}

#if defined(BRICKBLASTER_MOBILE)
int is_screen_too_small(void) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int shorter = (w < h) ? w : h;
    return shorter < 500;
}

void draw_unfold_screen(void) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    DrawRectangle(0, 0, w, h, (Color){0, 0, 0, 245});
    if (s_font_ready) {
        /* Use FONTE: "unfold to play" centred on screen. */
        const char *msg = "unfold to play";
        int tw = font_string_width(&s_font, msg);
        /* draw_unfold_screen is in screen-space so we need to scale.
         * Use a simple 2x scale guess; not pixel-perfect but visible. */
        float sx = (float)w / SCREEN_W;
        float sy = (float)h / SCREEN_H;
        float s  = (sx < sy) ? sx : sy;
        int fx = (int)((w - tw * s) / 2);
        int fy = h / 2 - (int)(FONT_CHAR_H * s / 2);
        /* Fallback: draw in canvas coords (won't letterbox perfectly but OK). */
        font_draw_string(&s_font, msg, fx, fy, WHITE);
    }
}
#endif
