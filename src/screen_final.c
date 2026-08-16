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
#include "i18n.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* FILE.ASM:164 `mov ecx,417-33` → 384 frames rendered after the first
 * uncompressed keyframe, not 418. P1-ASM-27. */
#define FINAL_FRAME_COUNT 384
#define FINAL_FPS FLC_FPS  /* animation.h — 5 vsyncs/frame, FILE.ASM:164-170 */

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

/* Draw one FONTE line, LEFT-aligned at x = 120.
 * Print_final (FONTE.ASM:171-184) passes ebx = panel_hiscore_o =
 * bord_x + screen_x*(bord_y+22) + 8, i.e. x = 112 + 8 = 120, y = 22, and
 * @@next_ligne (FONTE.ASM:370) does `add esi,4` to skip each row's four-space
 * prefix. The 27 payload characters then span 120 + 27*15 = 525 < limite_x.
 * Nothing is centred: the .cfg rows carry their own padding. */
#define FINAL_TEXT_X       120
#define FINAL_LINE_PITCH    30   /* FONTE.ASM:416  Fonte_Next_Line = 30 */
#define FINAL_TEXT_Y0       52   /* 22 + one pitch: final_text itself is a
                                  * blank row printed before the 13 .cfg rows */
static void draw_line(const char *s, int y) {
    if (!s_final_font_ready || !s) return;
    font_draw_string(&s_final_font, s, FINAL_TEXT_X, y, WHITE);
}

/* Render the final_text block (solo/coop) — 13 lines byte-exact from
 * Blaster_en.cfg:112-124. F4-D1: blank padders preserved so vertical
 * layout matches the ASM print_final. */
/* Blaster.cfg (FR) / Blaster_en.cfg (EN) / Blaster_es.cfg (ES), lines 112-124.
 * read_text_final (FILE.ASM:1089-1110) overwrites the compiled "+++" frame
 * with 13 rows of 27 characters taken from the .cfg, so these ARE the shipped
 * strings. The original has three languages only; DE/IT/PT fall back to EN. */
static const char *const FINAL_TEXT[3][13] = {
 /* EN */ { "                           ","                           ",
            "   thank you for playing   ","       brick blaster       ",
            "                           ","you have reached the end of",
            "    this terrible game     ","                           ",
            "dont't forget to contact us","  at www.eclipse-game.com  ",
            "                           ","                           ",
            "                           " },
 /* FR */ { "                           ","                           ",
            "                           ","      felicitations !      ",
            "                           ","                           ",
            "     vous etes vraiment    ","           un as !!        ",
            "                           ","   a une prochaine fois    ",
            "    peut etre dans une     ","     autre dimension!      ",
            "                           " },
 /* ES */ { "                           ","                           ",
            "                           ","         felicidades       ",
            "                           ","                           ",
            "        Eres realmente     ","           el mejor        ",
            "                           ","      Nos vemos quizas     ",
            "       en otro mundo       ","                           ",
            "                           " },
};

/* Blaster*.cfg lines 127-139. Row 6 is patched at runtime by the winner label. */
static const char *const FINAL_DUAL[3][13] = {
 /* EN */ { "                           ","                           ",
            "                           ","         ----------        ",
            "           winner          ","                           ",
            "         player ???        ","         ----------        ",
            "                           ","                           ",
            "                           ","                           ",
            "                           " },
 /* FR */ { "                           ","                           ",
            "                           ","         ----------        ",
            "         vainqueur         ","                           ",
            "         joueur ???        ","         ----------        ",
            "                           ","                           ",
            "                           ","                           ",
            "                           " },
 /* ES */ { "                           ","                           ",
            "                           ","         ----------        ",
            "         vencedor          ","                           ",
            "         jugador ???       ","         ----------        ",
            "                           ","                           ",
            "                           ","                           ",
            "                           " },
};

/* EN = 0, FR = 1, ES = 2. The 1999 game shipped exactly these three .cfg. */
static int final_lang_slot(void) {
    switch (i18n_get_language()) {
        case LANG_FR: return 1;
        case LANG_ES: return 2;
        default:      return 0;
    }
}

static void draw_solo_modal(ScreenState *state) {
    (void)state;
    /* No ClearBackground — the FLC's last frame stays underneath. */
    if (!s_final_font_ready) return;
    const char *const *LINES = FINAL_TEXT[final_lang_slot()];
    for (int i = 0; i < 13; i++) {
        draw_line(LINES[i], FINAL_TEXT_Y0 + i * FINAL_LINE_PITCH);
    }
}

/* Render the final_dual block — byte-exact from Blaster.cfg:127-139.
 * HISCORE.ASM:133-138 patches the 4-char `winner` label:
 *   `mov eax,' eno'` (LE "one ") if P2 lives<0, else `' owt'` ("two ").
 * Final rendered line is "player one " or "player two ". No "wins",
 * no "draw" (F4-D2). ASM only checks P2 lives<0 — if both dead (should
 * not happen per ASM), default to "one". */
static void draw_dual_modal(Game *game) {
    /* No ClearBackground — see draw_solo_modal. On the duel game-over path
     * there is no FLC underneath, so main.c clears the canvas itself. */
    if (!s_final_font_ready) return;

    /* HISCORE.ASM:133-138 writes FOUR BYTES at a fixed offset:
     *     mov eax,' eno'                    ; little-endian "one "
     *     cmp player_2.player_nbs_ball,-1
     *     je  @@ok
     *     mov eax,' owt'                    ; "two "
     * @@ok: mov D winner,eax
     * `winner` (FILE.ASM:1171-1172) is final_dual_2 + 6*32 + 20, i.e. index 16
     * of row 6's 27-character payload. The label is ALWAYS English, whatever
     * the .cfg language. Blaster_es.cfg has its "???" at index 17-19, so the
     * Spanish line renders as "jugadorone" — a 1999 bug, reproduced here
     * rather than silently corrected. */
    const char *who = (game && game->lives_2 >= 0) ? "two " : "one ";
    char winner_line[28];
    memcpy(winner_line, FINAL_DUAL[final_lang_slot()][6], 27);
    winner_line[27] = '\0';
    memcpy(winner_line + 16, who, 4);

    const char *const *LINES = FINAL_DUAL[final_lang_slot()];
    for (int i = 0; i < 13; i++) {
        draw_line((i == 6) ? winner_line : LINES[i],
                  FINAL_TEXT_Y0 + i * FINAL_LINE_PITCH);
    }
}


void final_draw(FinalAssets *assets) {
    if (!assets) return;
    if (!assets->loaded) return;
    /* Also draw during the modal phase: HISCORE.ASM:36-44 restores the
     * palette and calls init_palette but never clears the frame buffer, and
     * Print_final runs with `transparence = On` (FONTE.ASM:177) — the text
     * lands on top of the FLC's last frame, not on black. */
    animation_draw(&assets->final_anim, 0, 0);
}

void final_draw_modal(FinalAssets *assets, ScreenState *state, Game *game) {
    if (!assets || !state) return;
    if (assets->phase != FINAL_PHASE_MODAL) return;
    ensure_font();
    /* HISCORE.ASM:28-29  Display_score_from_final (the VICTORY path, reached
     * from next_level at MAIN.ASM:928) opens with
     *     cmp dual_flag,On / je @@exit
     * so a duel that clears the last level shows the FLC and nothing after it.
     * final_dual belongs to the other entry point, Display_score
     * (HISCORE.ASM:106-141), which test_game_over calls at MAIN.ASM:4687 —
     * i.e. at duel GAME OVER. The port had the two swapped. */
    if (state->dual_flag) return;
    draw_solo_modal(state);
}

/* Duel game over panel — HISCORE.ASM:106-141 Display_score with dual_flag On:
 * no FLC, no hiscore table, just the final_dual block with the winner label
 * patched in. Called directly by main.c on the duel game-over path. */
void final_draw_dual_gameover(Game *game) {
    ensure_font();
    draw_dual_modal(game);
}
