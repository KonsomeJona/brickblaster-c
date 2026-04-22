/* screen_menu.c — Original BrickBlaster 7-screen menu.
 *
 * Reproduces the menu table from MAIN.ASM:6896-6942 and the layout from
 * Blaster.inc:23-105. All coordinates are in the 640x480 canvas.
 */

#include "screen_menu.h"
#include "font.h"
#include "i18n.h"
#include "letterbox.h"
#include "input_frame.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "input_gamepad.h"
#if defined(BRICKBLASTER_MOBILE)
#include "mobile_controls.h"
#endif

/* --- Layout constants from Blaster.inc --- */
#define ICON_W           259    /* icon_menu_size_x */
#define ICON_H           260    /* icon_menu_size_y */
#define ICON_DST_X       189    /* icon_menu_pos_x  */
#define ICON_DST_Y       141    /* icon_menu_pos_y  */

#define CURSOR_W          14    /* cursor_menu_size_x */
#define CURSOR_H          18    /* cursor_menu_size_y */
#define CURSOR_SRC_X     526    /* cursor_menu_o      */
#define CURSOR_SRC_Y       0

/* Attract-mode idle timer — frames before triggering demo (Blaster.inc:415).
 * Iter 2 fix #13: alias the canonical DELAI_DATTENTE constant from constants.h. */
#define IDLE_TO_DEMO    DELAI_DATTENTE

/* Source rects in MENU.png for the 7 menu icons (Blaster.inc:23-29). */
static const Rectangle ICON_SRC[7] = {
    /* icon_menu_1_o = 002+(640*49)  */ {  2,  49, ICON_W, ICON_H },
    /* icon_menu_2_o = 262+(640*49)  */ {262,  49, ICON_W, ICON_H },
    /* icon_menu_3_o = 002+(640*310) */ {  2, 310, ICON_W, ICON_H },
    /* icon_menu_4_o = 262+(640*310) */ {262, 310, ICON_W, ICON_H },
    /* icon_menu_5_o = 002+(640*571) */ {  2, 571, ICON_W, ICON_H },
    /* icon_menu_6_o = 262+(640*571) */ {262, 571, ICON_W, ICON_H },
    /* icon_menu_7_o = 002+(640*832) */ {  2, 832, ICON_W, ICON_H },
};

/* MAIN.ASM:518-545 display_menu_icon — current_menu (1..7) → icon index (0..6). */
static const int MENU_TO_ICON[8] = { 0, 0, 1, 2, 5, 3, 4, 6 };

/* The 4 button hit-rects in the icon (MAIN.ASM:6914-6917): pos_x, pos_y, end_x, end_y.
 * Indices: 0=a (TL), 1=b (TR), 2=c (BL), 3=d (BR). */
static const Rectangle BTN_RECT[4] = {
    { 192, 142, 317 - 192, 268 - 142 },   /* a */
    { 322, 142, 447 - 322, 268 - 142 },   /* b */
    { 192, 273, 317 - 192, 399 - 273 },   /* c */
    { 322, 273, 447 - 322, 399 - 273 },   /* d */
};

/* Per-menu, per-button localised label and title (current menu's context). */
static StringId menu_label(int menu, int btn) {
    /* menu 1 main: play/demo/misc/quit */
    static const StringId M1[4] = { STR_M_PLAY, STR_M_DEMO, STR_M_MISC, STR_M_QUIT };
    /* menu 2 players: solo/coop/dual/cancel */
    static const StringId M2[4] = { STR_M_ONE_PLAYER, STR_M_COOP, STR_M_DUAL, STR_M_CANCEL };
    /* menu 3 ctrl player 2: computer/keyboard/joystick/cancel */
    static const StringId M3[4] = { STR_M_COMPUTER, STR_M_KEYBOARD, STR_M_JOYSTICK, STR_M_CANCEL };
    /* menu 4 select world: space/arcade/-/cancel */
    static const StringId M4[4] = { STR_M_SPACE, STR_M_ARCADE, STR_M_BLANK, STR_M_CANCEL };
    /* menu 5 skill: easy/medium/hard/cancel */
    static const StringId M5[4] = { STR_M_EASY, STR_M_MEDIUM, STR_M_HARD, STR_M_CANCEL };
    /* menu 6 misc: hiscore/hiscore-coop/credits/cancel
     * constants.h:410-413 / MAIN.ASM:6913-6941 — slot b is Hiscore-Coop, not Editor.
     * Editor (EDITOR.ASM) is reachable via F2 keyboard shortcut — see main loop. */
    static const StringId M6[4] = { STR_M_HISCORES, STR_M_HISCORES_COOP, STR_M_CREDITS, STR_M_CANCEL };
    /* menu 7 joystick: analogic/numeric/-/cancel */
    static const StringId M7[4] = { STR_M_ANALOGIC, STR_M_NUMERIC, STR_M_BLANK, STR_M_CANCEL };
    if (btn < 0 || btn > 3) return STR_M_BLANK;
    switch (menu) {
        case 1: return M1[btn]; case 2: return M2[btn]; case 3: return M3[btn];
        case 4: return M4[btn]; case 5: return M5[btn]; case 6: return M6[btn];
        case 7: return M7[btn]; default: return STR_M_BLANK;
    }
}
static StringId menu_title(int menu) {
    switch (menu) {
        case 1: return STR_M_TITLE_1; case 2: return STR_M_TITLE_2;
        case 3: return STR_M_TITLE_3; case 4: return STR_M_TITLE_4;
        case 5: return STR_M_TITLE_5; case 6: return STR_M_TITLE_6;
        case 7: return STR_M_TITLE_7; default: return STR_M_BLANK;
    }
}

/* --- Asset management --- */

void menu_assets_load(MenuAssets *m, Assets *game_assets) {
    if (!m) return;
    m->assets = game_assets;
    m->menu_bg = LoadTexture(ASSETS_BASE "sprites/Blaster.png");
    m->menu_bg_loaded = (m->menu_bg.id != 0);
    m->hover_button = -1;
    m->cursor_x = SCREEN_W / 2;
    m->cursor_y = SCREEN_H / 2;
    m->idle_frames = 0;
    m->font_ready = 0;
    if (game_assets && game_assets->font_sheet_loaded) {
        font_init(&m->font, &game_assets->font_sheet);
        m->font_ready = 1;
    }
}

void menu_assets_unload(MenuAssets *m) {
    if (!m) return;
    if (m->menu_bg_loaded) { UnloadTexture(m->menu_bg); m->menu_bg_loaded = 0; }
}

/* Coord helpers: use shared letterbox.h. */
static Vector2 screen_to_canvas(int sx, int sy) {
    return letterbox_screen_to_canvas((Vector2){ (float)sx, (float)sy });
}

static int hit_test_button(int cx, int cy) {
    Vector2 p = { (float)cx, (float)cy };
    for (int i = 0; i < 4; i++) {
        if (CheckCollisionPointRec(p, BTN_RECT[i])) return i;
    }
    return -1;
}

/* --- Action handling --- */

/* Apply the action for (current_menu, button). Mirrors MAIN.ASM:328-440. */
static void menu_apply_action(ScreenState *state, int menu, int btn) {
    switch (menu) {
        case 1: /* main */
            switch (btn) {
                case 0: /* play — full flow through players/ctrl/world/skill */
                    state->demo_flag = 0;
                    state->current_menu = 2;  /* players screen */
                    break;
                case 1: /* demo — launch directly (MAIN.ASM:85-115).
                         * P1-ASM-15: ASM:87-90 randomises nbs_player to
                         * {1,2} via get_random(1)+inc, so the attract
                         * demo alternates solo/coop layouts. */
                    state->demo_flag = 1;
                    state->nbs_player = GetRandomValue(1, 2);
                    state->dual_flag  = 0;
                    state->difficulte = 2;
                    state->world = GetRandomValue(0, 1); /* MAIN.ASM:113 */
                    state->game_mode = STATE_READY_TO_PLAY;
                    break;
                case 2: state->current_menu = 6; break;   /* misc */
                case 3: state->quit_requested = 1; break;  /* quit */
            }
            break;
        case 2: /* players */
            switch (btn) {
                case 0: state->nbs_player = 1; state->dual_flag = 0;
                        state->current_menu = 4; break;                         /* solo */
                case 1: state->nbs_player = 2; state->dual_flag = 0;
                        state->current_menu = 3; break;                         /* coop */
                case 2: state->nbs_player = 2; state->dual_flag = 1;
                        state->current_menu = 3; break;                         /* dual */
                case 3: state->current_menu = 1; break;                         /* cancel */
            }
            break;
        case 3: /* ctrl player 2 — computer/keyboard/joystick */
            switch (btn) {
                case 0: state->control_p2 = 0; state->current_menu = 4; break; /* computer */
                case 1: state->control_p2 = 1; state->current_menu = 4; break; /* keyboard */
                case 2: state->control_p2 = 2; state->current_menu = 7; break; /* joystick */
                case 3: state->current_menu = 2; break;                        /* cancel */
            }
            break;
        case 4: /* select world */
            switch (btn) {
                case 0: state->world = 0; state->current_menu = 5; break;       /* space */
                case 1: state->world = 1; state->current_menu = 5; break;       /* arcade */
                case 2: break;                                                  /* blank */
                case 3: state->current_menu = (state->nbs_player > 1) ? 3 : 2; break; /* cancel */
            }
            break;
        case 5: /* skill */
            switch (btn) {
                case 0: state->difficulte = 1; state->game_mode = STATE_READY_TO_PLAY; break;
                case 1: state->difficulte = 2; state->game_mode = STATE_READY_TO_PLAY; break;
                case 2: state->difficulte = 4; state->game_mode = STATE_READY_TO_PLAY; break;
                case 3: state->current_menu = 4; break;
            }
            break;
        case 6: /* miscellaneous */
            switch (btn) {
                case 0: state->hiscore_mode = 0; state->game_mode = STATE_HISCORE; break;
                /* constants.h:411 M_SCORE_2: slot b opens the coop hiscore table */
                case 1: state->hiscore_mode = 1; state->game_mode = STATE_HISCORE; break;
                case 2: state->game_mode = STATE_CREDITS; break;
                case 3: state->current_menu = 1; break;
            }
            /* TODO: wire Editor (STATE_EDIT) to F2 key or a separate menu slot —
             * Editor was not in MAIN.ASM menu 6 originally (EDITOR.ASM reached
             * via a separate entry). */
            break;
        case 7: /* joystick — analogic/numeric both confirm and return to world */
            switch (btn) {
                case 0: case 1: state->current_menu = 4; break;
                case 2: break;
                case 3: state->current_menu = 3; break;
            }
            break;
    }
}

/* --- Input --- */

void menu_handle_input(ScreenState *state, MenuAssets *m, AudioState *audio,
                       const FrameInput *input) {
    if (!state || !m) return;

    int menu = state->current_menu;
    if (menu < 1 || menu > 7) { state->current_menu = 1; menu = 1; }

    /* ESC / gamepad B → cancel-equivalent (button d when present). */
    if (IsKeyPressed(KEY_ESCAPE) || gamepad_back()) {
        m->idle_frames = 0;
        if (menu == 1) state->quit_requested = 1;
        else menu_apply_action(state, menu, 3);
        return;
    }

    /* Mouse / touch position → hover button. */
    Vector2 mp = (GetTouchPointCount() > 0) ? GetTouchPosition(0)
                                            : GetMousePosition();
    Vector2 cp = screen_to_canvas((int)mp.x, (int)mp.y);
    int has_pointer = (cp.x >= 0 && cp.x < SCREEN_W && cp.y >= 0 && cp.y < SCREEN_H);
    if (has_pointer) {
        m->cursor_x = (int)cp.x;
        m->cursor_y = (int)cp.y;
    }

    int hover = has_pointer ? hit_test_button(m->cursor_x, m->cursor_y) : -1;

    /* Keyboard arrows / D-pad cycle the 2x2 grid. */
    int kb_changed = 0;
    int sel = (m->hover_button < 0) ? 0 : m->hover_button;
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A) || gamepad_left_pressed())  { sel ^= 1; kb_changed = 1; }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || gamepad_right_pressed()) { sel ^= 1; kb_changed = 1; }
    if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W) || gamepad_up_pressed())    { sel ^= 2; kb_changed = 1; }
    if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S) || gamepad_down_pressed())  { sel ^= 2; kb_changed = 1; }
    if (kb_changed) {
        m->hover_button = sel;
        m->idle_frames = 0;
        /* Move cursor visualisation to the centre of the selected quadrant. */
        Rectangle r = BTN_RECT[sel];
        m->cursor_x = (int)(r.x + r.width / 2);
        m->cursor_y = (int)(r.y + r.height / 2);
    } else if (hover >= 0) {
        m->hover_button = hover;
    }

    /* Idle counter — increments unless something changed. */
    if (input->click_pressed || kb_changed || hover >= 0 ||
        gamepad_confirm() || gamepad_start()) {
        m->idle_frames = 0;
    } else {
        m->idle_frames++;
    }

    /* P1-ASM-16: Auto-demo on idle. MAIN.ASM:568-583 detect_reset_ecx
     * plus DELAI_DATTENTE=600 (Blaster.inc:415) — attract-mode demo
     * launches after ~10 s of no input on the main menu. Same entry
     * conditions as the manual demo button above (P1-ASM-15). */
    if (menu == 1 && m->idle_frames >= IDLE_TO_DEMO) {
        state->demo_flag  = 1;
        state->nbs_player = GetRandomValue(1, 2);
        state->dual_flag  = 0;
        state->difficulte = 2;
        state->world      = GetRandomValue(0, 1);
        state->game_mode  = STATE_READY_TO_PLAY;
        m->idle_frames    = 0;
        return;
    }

    /* Confirm: click on a quadrant, ENTER/SPACE on selected, gamepad A. */
    int confirm_keyboard =
        (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || gamepad_confirm());

    if (input->click_pressed) {
        int hit = hit_test_button(m->cursor_x, m->cursor_y);
        if (hit >= 0) {
            audio_play(audio, SFX_POWERUP_COLLECT);
            m->hover_button = hit;
            m->idle_frames = 0;
            menu_apply_action(state, menu, hit);
            /* MOUSE.ASM:420-433 / MAIN.ASM:266-270 — debounce: wait until
             * the click that selected this menu item is released before
             * the next screen reads read_click. Without this a held mouse
             * button bleeds into the next screen (e.g. demo exits frame 1). */
            input_wait_click_release();
            return;
        }
    } else if (confirm_keyboard && m->hover_button >= 0) {
        audio_play(audio, SFX_POWERUP_COLLECT);
        m->idle_frames = 0;
        menu_apply_action(state, menu, m->hover_button);
        input_wait_click_release();
        return;
    }
}

/* --- Drawing --- */

static void blit(Texture2D tex, Rectangle src, float dst_x, float dst_y, Color tint) {
    Rectangle dst = { dst_x, dst_y, src.width, src.height };
    DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, tint);
}

static void draw_centered_string(BitmapFont *font, const char *s, int cx, int y, Color tint) {
    int w = font_string_width(font, s);
    font_draw_string(font, s, cx - w / 2, y, tint);
}

void menu_draw(ScreenState *state, MenuAssets *m) {
    if (!state || !m) return;

    int menu = state->current_menu;
    if (menu < 1 || menu > 7) menu = 1;

    /* Clear canvas to black (Blaster.png has alpha=0 at edges). */
    ClearBackground((Color){0, 0, 0, 255});

    /* 1. Background — Blaster.png covers the full 640x480 canvas. */
    if (m->menu_bg_loaded) {
        Rectangle src = { 0, 0, (float)m->menu_bg.width, (float)m->menu_bg.height };
        blit(m->menu_bg, src, 0, 0, WHITE);
    }

    /* 2. Menu icon (259x260) at (189, 141), source from MENU.png. */
    if (m->assets && m->assets->menu_image_loaded) {
        Rectangle isrc = ICON_SRC[MENU_TO_ICON[menu]];
        blit(m->assets->menu_image, isrc, ICON_DST_X, ICON_DST_Y, WHITE);
    }

    /* 3. Cursor (14x18) at the cursor position.
     * Hover is indicated by the cursor sprite + the label text at the bottom
     * — no darkening overlay (the palette-pulse approximation looked heavy
     * against the disc artwork and isn't present in the ASM render path). */
    if (m->assets && m->assets->menu_image_loaded) {
        Rectangle csrc = { CURSOR_SRC_X, CURSOR_SRC_Y, CURSOR_W, CURSOR_H };
        float cx = m->cursor_x - CURSOR_W / 2.0f;
        float cy = m->cursor_y - CURSOR_H / 2.0f;
        blit(m->assets->menu_image, csrc, cx, cy, WHITE);
    }

    /* 5. Text: menu title stays pinned in the bottom bar, hovered-button
     * label follows the cursor per MAIN.ASM:555-567 refresh_text:
     *   panel_info.y = cursor.y + cursor_menu_size_y
     *   panel_info.x = cursor.x - panel_menu_size_x/2 + cursor_menu_size_x/2
     * which centers the label strip around the cursor column. */
    if (m->font_ready) {
        BitmapFont *font = &m->font;

        const char *title = i18n(menu_title(menu));
        const char *label = (m->hover_button >= 0)
                          ? i18n(menu_label(menu, m->hover_button))
                          : "";

        /* Bottom strip holds the (stationary) menu title. */
        DrawRectangle(0, 458, SCREEN_W, 22, (Color){0, 0, 0, 255});
        draw_centered_string(font, title, SCREEN_W / 2, 464, WHITE);

        /* Label trails the cursor, drawn over a short black backplate so
         * it stays readable against the disc artwork.  Clamp X so the
         * plate never runs off-screen. */
        if (m->hover_button >= 0 && label && label[0]) {
            int lw    = font_string_width(font, label);
            int pad   = 6;
            int plate_w = lw + pad * 2;
            int plate_x = (int)m->cursor_x - plate_w / 2;
            int plate_y = (int)m->cursor_y + CURSOR_H;
            if (plate_x < 4) plate_x = 4;
            if (plate_x + plate_w > SCREEN_W - 4) plate_x = SCREEN_W - 4 - plate_w;
            DrawRectangle(plate_x, plate_y, plate_w, 18, (Color){0, 0, 0, 200});
            draw_centered_string(font, label, plate_x + plate_w / 2, plate_y + 2, WHITE);
        }
    }
}
