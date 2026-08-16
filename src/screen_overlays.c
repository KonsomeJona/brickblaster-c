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
#include "letterbox.h"
#include "i18n.h"
#include <stddef.h>
#include <stdio.h>

static BitmapFont s_font;
static int s_font_ready = 0;

/* Pause-overlay tap zones (canvas coords, 640x480). Drawn and hit-tested
 * only when input_touch_ui_active() — desktop keyboard/mouse players keep
 * the untouched 1999-style text overlay. Style matches mobile_controls.c
 * (rounded translucent rect + border, TakoHi orange for the primary action). */
static const Rectangle s_resume_rect = { 210, 196, 220, 56 };
static const Rectangle s_exit_rect   = { 210, 270, 220, 56 };

static void draw_tap_zone(Rectangle r, const char *label, Color border) {
    DrawRectangleRounded(r, 0.18f, 6, (Color){30, 30, 44, 190});
    DrawRectangleLinesEx(r, 2.0f, border);
    if (s_font_ready) {
        int tw = font_string_width(&s_font, label);
        font_draw_string(&s_font, label,
                         (int)(r.x + (r.width  - tw) / 2),
                         (int)(r.y + (r.height - FONT_CHAR_H) / 2), WHITE);
    }
}

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

/* MAIN.ASM:1261 — option_text_paused
 *
 * Nothing but the option_text_paused banner, as in MAIN.ASM:1150+. */
void draw_pause_screen(ScreenState *state) {
    draw_option_text(i18n(STR_OPT_PAUSED));
    if (!state || !s_font_ready) return;

    /* No audio panel here. `pause` (MAIN.ASM:1150+) prints option_text_paused
     * and nothing else; audio level is set by the menu VU meter
     * (MAIN.ASM:649-689 detect_button_music), which the port now draws. */

    /* Touch UI: explicit resume/exit tap targets — without them a touch
     * player is locked in the session until game over (no P/ESC key). The
     * audio lines above double as tap targets (see pause_handle_input);
     * left unboxed to keep the overlay readable. */
    if (input_touch_ui_active()) {
        draw_tap_zone(s_resume_rect, "resume", (Color){232, 115, 74, 255});
        draw_tap_zone(s_exit_rect,   "exit",   (Color){110, 110, 135, 220});
    }
}

/* MAIN.ASM:4681 — option_text_over */
void draw_game_over_screen(ScreenState *state, int *timer,
                           int game_mode, int winner) {
    (void)state; (void)timer;
    draw_option_text(i18n(STR_OPT_GAME_OVER));

    /* No winner banner here. MAIN.ASM:4678-4687 prints option_text_over and
     * nothing else; the duel winner is announced by Display_score, which
     * shows the final_dual panel with "one"/"two" patched into it
     * (HISCORE.ASM:109-141) — that is screen_final's job, not an overlay.
     * The "draw" case the port used to print cannot even occur: HISCORE.ASM:134
     * tests player_2's counter alone, so there is always a winner. */
    (void)game_mode;
    (void)winner;
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

/* Pause input: gamepad B exits to menu. Audio level lives in the menu VU
 * meter, not here (MAIN.ASM:649-689). */
int pause_handle_input(ScreenState *state, const FrameInput *input) {
    if (!state) return 0;

    /* Gamepad B = exit to menu */
    if (gamepad_back()) {
        state->game_mode    = STATE_MENU;
        state->current_menu = 1;
        return 0;
    }

    /* Touch UI tap zones (see draw_pause_screen). Canvas-space hit test of
     * the centralized click/tap. Inactive on desktop (touch UI off), where
     * the historical behaviour — any click resumes — is preserved by the
     * caller (main.c uses fi.click_pressed when we return 0). */
    if (input && input->click_pressed && input_touch_ui_active()) {
        Vector2 cp = letterbox_screen_to_canvas(
            (Vector2){ input->click_screen_x, input->click_screen_y });
        if (CheckCollisionPointRec(cp, s_resume_rect)) return 1;
        if (CheckCollisionPointRec(cp, s_exit_rect)) {
            state->game_mode    = STATE_MENU;
            state->current_menu = 1;
            return 0;
        }
    }
    return 0;
}

/* On-screen pause button — two bars in a rounded rect, top-right margin
 * (rect: PAUSE_BTN_* in input_frame.h). Same translucent style as the
 * mobile touch buttons. No font dependency, no-op when the touch UI is off
 * (desktop/Wear never see it; web only after a real touch is detected). */
void draw_touch_pause_button(void) {
    if (!input_touch_ui_active()) return;
    Rectangle r = { PAUSE_BTN_X, PAUSE_BTN_Y, PAUSE_BTN_W, PAUSE_BTN_H };
    DrawRectangleRounded(r, 0.25f, 6, (Color){30, 30, 44, 150});
    DrawRectangleLinesEx(r, 2.0f, (Color){110, 110, 135, 190});
    {
        int bw = 7, bh = 24;
        int cx = PAUSE_BTN_X + PAUSE_BTN_W / 2;
        int cy = PAUSE_BTN_Y + PAUSE_BTN_H / 2;
        Color bar = {200, 200, 215, 220};
        DrawRectangle(cx - bw - 3, cy - bh / 2, bw, bh, bar);
        DrawRectangle(cx + 3,      cy - bh / 2, bw, bh, bar);
    }
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
