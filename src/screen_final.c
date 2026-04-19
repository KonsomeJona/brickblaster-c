/* screen_final.c — Victory animation screen
 *
 * FILE.ASM:118-183 Blaster.flc victory animation
 * HISCORE.ASM:5-83 Display_score_from_final — wraps FLC with final_text /
 *                  final_dual modal screens.
 *
 * P1-ASM-21/22 fix: duel now plays the FLC too, and both solo/coop and duel
 * show a FONTE-rendered text panel (final_text / final_dual) after the
 * animation. Click or ~5 s auto-advance to STATE_HISCORE (solo/coop) or
 * STATE_MENU (duel — duel scores must not contaminate leaderboards).
 */

#include "screen_final.h"
#include "assets.h"
#include "constants.h"
#include "font.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* FILE.ASM:164 `mov ecx,417-33` → 384 frames rendered after the first
 * uncompressed keyframe, not 418. P1-ASM-27. */
#define FINAL_FRAME_COUNT 384
#define FINAL_FPS 12.0f  /* FILE.ASM:168-170 — 5 vsyncs per frame (60Hz/5 = 12 FPS) */

/* F4-D7: HISCORE.ASM:52 `mov ecx,-1; call wait_click` → wait INFINITELY
 * until click/key. No auto-advance — must mirror ASM semantics exactly. */

/* Modal phase inside FinalAssets. */
enum {
    FINAL_PHASE_ANIM  = 0,
    FINAL_PHASE_MODAL = 1,
};

/* Font shared with overlays — lazily initialised from assets. */
static BitmapFont s_final_font;
static int s_final_font_ready = 0;

static void ensure_font(void) {
    /* Accessors into the global assets live in main.c; the final screen
     * only needs font metrics, so we re-init from the same sheet on first
     * draw. The font struct is tiny so repeatedly calling font_init is
     * harmless — we gate with s_final_font_ready for clarity. */
    (void)s_final_font_ready;
}

/* Load victory animation */
void final_assets_load(FinalAssets *assets) {
    if (!assets) return;
    if (assets->loaded) return;

    /* Load victory animation — FLC capped at 384 frames per ASM. */
    assets->final_anim = animation_load(ASSETS_BASE "final", FINAL_FRAME_COUNT, FINAL_FPS);
    if (!assets->final_anim.frames) return;

    assets->phase       = FINAL_PHASE_ANIM;
    assets->modal_timer = 0;
    assets->loaded      = 1;
}

void final_assets_unload(FinalAssets *assets) {
    if (!assets) return;
    if (!assets->loaded) return;

    animation_unload(&assets->final_anim);
    assets->loaded = 0;
}

/* Bind the font sheet from global assets (optional — if the font is
 * missing we still advance the state machine, only drawing is affected). */
void final_bind_font(Assets *game_assets) {
    if (!game_assets || !game_assets->font_sheet_loaded) return;
    font_init(&s_final_font, &game_assets->font_sheet);
    s_final_font_ready = 1;
}

/* Transition to the post-anim modal phase. */
static void enter_modal(FinalAssets *assets) {
    assets->phase       = FINAL_PHASE_MODAL;
    assets->modal_timer = 0;
}

/* Done showing the modal — hand off to the appropriate state.
 *
 * HISCORE.ASM:28-29 keeps the duel path separate: duel plays the FLC and
 * the final_dual "winner is player N" text, but then returns to the menu
 * without a hiscore entry (duel scores must not contaminate leaderboards).
 * Solo/coop hits the hiscore entry screen (HISCORE.ASM:196-210). */
static void leave_modal(ScreenState *state, FinalAssets *assets) {
    if (assets->loaded) {
        animation_reset(&assets->final_anim);
    }
    assets->phase       = FINAL_PHASE_ANIM;
    assets->modal_timer = 0;
    state->game_mode = state->dual_flag ? STATE_MENU : STATE_HISCORE;
}

void final_update(ScreenState *state, FinalAssets *assets, const FrameInput *input) {
    if (!state || !assets) return;

    /* Modal phase: wait INFINITELY until click/key (HISCORE.ASM:52
     * `mov ecx,-1; call wait_click`). F4-D7: no auto-advance. */
    if (assets->phase == FINAL_PHASE_MODAL) {
        int skip = (GetKeyPressed() != 0) || input->click_pressed;
        assets->modal_timer++;
        if (skip) {
            leave_modal(state, assets);
        }
        return;
    }

    /* Anim phase — skip on any key/click. FILE.ASM:152-154. */
    if (GetKeyPressed() != 0 || input->click_pressed) {
        enter_modal(assets);
        return;
    }

    if (assets->loaded) {
        animation_update(&assets->final_anim);
        if (animation_is_finished(&assets->final_anim)) {
            enter_modal(assets);
        }
    } else {
        /* Asset missing — fall through to the menu so we don't wedge. */
        state->game_mode = STATE_MENU;
    }
}

/* Draw a centred FONTE line at the given y (canvas coords). */
static void draw_line(const char *s, int y) {
    if (!s_final_font_ready || !s) return;
    int w = font_string_width(&s_final_font, s);
    font_draw_string(&s_final_font, s, (SCREEN_W - w) / 2, y, WHITE);
}

/* Render the final_text block (solo/coop) — 13 lines byte-exact from
 * Blaster_en.cfg:112-124. F4-D1: blank padders preserved so vertical
 * layout matches the ASM print_final. */
static void draw_solo_modal(ScreenState *state) {
    (void)state;
    ClearBackground((Color){0, 0, 0, 255});
    if (!s_final_font_ready) return;
    /* Blaster_en.cfg:112-124 — all 13 entries including blank padders. */
    static const char *LINES[13] = {
        "                           ",
        "                           ",
        "   thank you for playing   ",
        "       brick blaster       ",
        "                           ",
        "you have reached the end of",
        "    this terrible game     ",
        "                           ",
        "dont't forget to contact us",
        "  at www.eclipse-game.com  ",
        "                           ",
        "                           ",
        "                           ",
    };
    int line_h = FONT_CHAR_H + 4;
    int count  = (int)(sizeof(LINES) / sizeof(LINES[0]));
    int y0     = (SCREEN_H - count * line_h) / 2;
    for (int i = 0; i < count; i++) {
        draw_line(LINES[i], y0 + i * line_h);
    }
}

/* Render the final_dual block — byte-exact from Blaster.cfg:127-139.
 * MAIN.ASM:133-138 patches the 4-char `winner` label:
 *   `mov eax,' eno'` (LE "one ") if P2 lives<0, else `' owt'` ("two ").
 * Final rendered line is "player one " or "player two ". No "wins",
 * no "draw" (F4-D2). ASM only checks P2 lives<0 — if both dead (should
 * not happen per ASM), default to "one". */
static void draw_dual_modal(Game *game) {
    ClearBackground((Color){0, 0, 0, 255});
    if (!s_final_font_ready) return;

    /* Pick "one" or "two" — mirror MAIN.ASM:133-136 exactly. */
    const char *who = "one ";
    if (game && game->lives_2 >= 0) {
        who = "two ";
    }

    /* Compose "         player one        " (27 chars to match ASM width). */
    char winner_line[32];
    snprintf(winner_line, sizeof(winner_line), "         player %s       ", who);

    /* Blaster_en.cfg:127-139 — 13 lines, byte-exact. */
    static const char *LINES[13] = {
        "                           ",
        "                           ",
        "                           ",
        "         ----------        ",
        "           winner          ",
        "                           ",
        NULL,  /* patched with "player one" / "player two" at index 6 */
        "         ----------        ",
        "                           ",
        "                           ",
        "                           ",
        "                           ",
        "                           ",
    };
    int line_h = FONT_CHAR_H + 4;
    int count  = 13;
    int y0     = (SCREEN_H - count * line_h) / 2;
    for (int i = 0; i < count; i++) {
        const char *s = (i == 6) ? winner_line : LINES[i];
        draw_line(s, y0 + i * line_h);
    }
}

void final_draw(FinalAssets *assets) {
    if (!assets) return;
    if (!assets->loaded) return;
    if (assets->phase == FINAL_PHASE_ANIM) {
        animation_draw(&assets->final_anim, 0, 0);
    }
}

void final_draw_modal(FinalAssets *assets, ScreenState *state, Game *game) {
    if (!assets || !state) return;
    if (assets->phase != FINAL_PHASE_MODAL) return;
    ensure_font();
    if (state->dual_flag) draw_dual_modal(game);
    else                  draw_solo_modal(state);
}
