/* screen_hiscore.c  High score display and name entry
 * HISCORE.ASM:178-212 _Display_score / print_score
 * HISCORE.ASM:278-344 Get_name
 * HISCORE.ASM:358-382 Print_Cursor (blinking cursor)
 *
 * All text rendered with FONTE bitmap font (original style).
 */

#include "screen_hiscore.h"
#include "assets.h"
#include "font.h"
#include "constants.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(PLATFORM_ANDROID)
    #include "input_wear.h"
#endif
#if defined(BRICKBLASTER_MOBILE)
    #include "mobile_controls.h"
#endif
#include "input_gamepad.h"

static BitmapFont s_font;
static int s_font_ready = 0;

/* Table columns — canvas coords (640×480). */
#define COL_RANK    140
#define COL_SCORE   185
#define COL_LEVEL   290
#define COL_NAME    330
#define LETTER_W     15
#define ROW_H        26
#define ROW_START_Y  80

/* Name input length matches the ASM field: name_size = 15 (HISCORE.ASM:477). */
#define INPUT_NAME_LEN  HISCORE_NAME_LEN

/* Last entered name — initialised to all spaces to match the ASM Get_name
 * behaviour, which fills the buffer with ' ' before input (HISCORE.ASM:291-294). */
static char s_last_name[INPUT_NAME_LEN + 1] =
    "               ";  /* 15 spaces */

void hiscore_screen_load(HiscoreScreenState *hs, Assets *game_assets) {
    if (!hs) return;
    if (hs->loaded) return;

    /* FILE.ASM:403-441 Load_Picture_HiScore: `mov eax,3; call get_random;
     * call load_file_fond` — picks one of 3 backgrounds from world-0 set
     * (00_01, 00_02, 00_03). All three are space-world (world 0). */
    hs->backgrounds[0] = LoadTexture(ASSETS_BASE "backgrounds/00_01.png");
    if (hs->backgrounds[0].id == 0) return;
    hs->backgrounds[1] = LoadTexture(ASSETS_BASE "backgrounds/00_02.png");
    if (hs->backgrounds[1].id == 0) {
        UnloadTexture(hs->backgrounds[0]);
        return;
    }
    hs->backgrounds[2] = LoadTexture(ASSETS_BASE "backgrounds/00_03.png");
    if (hs->backgrounds[2].id == 0) {
        UnloadTexture(hs->backgrounds[0]);
        UnloadTexture(hs->backgrounds[1]);
        return;
    }

    hs->bg_index = GetRandomValue(0, 2);
    hs->name_entry_active = 0;
    hs->name_entry_pos = 0;
    hs->cursor_blink = 0;
    hs->entry_rank = 0;
    for (int i = 0; i < INPUT_NAME_LEN; i++) {
        char c = s_last_name[i];
        if (c >= 'A' && c <= 'Z')       hs->letter_values[i] = c - 'A';
        else if (c >= 'a' && c <= 'z')  hs->letter_values[i] = c - 'a';
        else                            hs->letter_values[i] = 26;  /* space */
    }

    if (game_assets && game_assets->font_sheet_loaded) {
        font_init(&s_font, &game_assets->font_sheet);
        s_font_ready = 1;
    }

    hs->loaded = 1;
}

void hiscore_screen_unload(HiscoreScreenState *hs) {
    if (!hs || !hs->loaded) return;
    UnloadTexture(hs->backgrounds[0]);
    UnloadTexture(hs->backgrounds[1]);
    UnloadTexture(hs->backgrounds[2]);
    hs->loaded = 0;
}

void hiscore_screen_update(ScreenState *state, HiscoreScreenState *hs, Hiscores *scores,
                           const FrameInput *input) {
    if (!state || !hs) return;
    hs->cursor_blink++;

    if (!hs->name_entry_active) {
        if (GetKeyPressed() != 0 || input->click_pressed
            || gamepad_confirm() || gamepad_back()
#if defined(PLATFORM_ANDROID)
            || wear_consume_crown_press()
#endif
        ) {
            state->game_mode = STATE_MENU;
            state->current_menu = 6;
        }
        return;
    }

    /* Name entry mode — HISCORE.ASM:296-331.
     * Cycle through 37 values: 0..25=A..Z, 26=space, 27..36=0..9 (P1-ASM-19). */
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || gamepad_up_pressed())
        hs->letter_values[hs->name_entry_pos] = (hs->letter_values[hs->name_entry_pos] + 1) % 37;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || gamepad_down_pressed())
        hs->letter_values[hs->name_entry_pos] = (hs->letter_values[hs->name_entry_pos] + 36) % 37;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || gamepad_right_pressed()) {
        if (hs->name_entry_pos < INPUT_NAME_LEN - 1) hs->name_entry_pos++;
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || gamepad_left_pressed()) {
        if (hs->name_entry_pos > 0) hs->name_entry_pos--;
    }

    /* HISCORE.ASM:296+ Get_name: int 16h keystroke, `or al,0x20` lowercase,
     * stosb write+advance. A-Z/a-z → letter, space → letter 26 (space),
     * digits 0-9 → extended letter_values 27..36 (P1-ASM-19, ASM accepts
     * any `al >= 0x20`). */
    int key = GetCharPressed();
    if (key >= 32 && key <= 122 && hs->name_entry_pos < INPUT_NAME_LEN) {
        int wrote = 1;
        if (key >= 65 && key <= 90)       hs->letter_values[hs->name_entry_pos] = key - 65;
        else if (key >= 97 && key <= 122) hs->letter_values[hs->name_entry_pos] = key - 97;
        else if (key == 32)               hs->letter_values[hs->name_entry_pos] = 26; /* space */
        else if (key >= 48 && key <= 57)  hs->letter_values[hs->name_entry_pos] = 27 + (key - 48);
        else                              wrote = 0;
        if (wrote && hs->name_entry_pos < INPUT_NAME_LEN - 1) hs->name_entry_pos++;
    }

    /* P1-ASM-20 BACKSPACE: HISCORE.ASM:310-311, 325-331 — ah=14 overwrites
     * the current slot with a space and steps back one position. We mirror
     * that: space the current slot (letter value 26) and decrement the
     * cursor if not already at the start. */
    if (IsKeyPressed(KEY_BACKSPACE)) {
        hs->letter_values[hs->name_entry_pos] = 26;
        if (hs->name_entry_pos > 0) hs->name_entry_pos--;
    }

    /* HISCORE.ASM:Get_name confirms on al=13 (ENTER) or ah=1 (ESC). */
    int do_confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)
                  || IsKeyPressed(KEY_ESCAPE) || gamepad_confirm();

#if defined(PLATFORM_ANDROID)
#if defined(BRICKBLASTER_MOBILE)
    if (mobile_left_pressed())
        hs->letter_values[hs->name_entry_pos] = (hs->letter_values[hs->name_entry_pos] + 36) % 37;
    if (mobile_right_pressed())
        hs->letter_values[hs->name_entry_pos] = (hs->letter_values[hs->name_entry_pos] + 1) % 37;
    if (mobile_fire_pressed()) {
        if (hs->name_entry_pos < INPUT_NAME_LEN - 1) hs->name_entry_pos++;
        else do_confirm = 1;
    }
#else
    {
        float rotary = wear_consume_rotary_delta();
        if (rotary > 0.3f)
            hs->letter_values[hs->name_entry_pos] = (hs->letter_values[hs->name_entry_pos] + 1) % 37;
        else if (rotary < -0.3f)
            hs->letter_values[hs->name_entry_pos] = (hs->letter_values[hs->name_entry_pos] + 36) % 37;
    }
    if (input->click_pressed || wear_consume_crown_press()) {
        if (hs->name_entry_pos < INPUT_NAME_LEN - 1) hs->name_entry_pos++;
        else do_confirm = 1;
    }
#endif
#endif

    if (do_confirm) {
        if (scores && hs->entry_rank >= 0 && hs->entry_rank < HISCORE_ENTRIES) {
            HiscoreEntry *e = &scores->entries[state->hiscore_mode][hs->entry_rank];
            /* Copy 15 letters into the entry; letter value 26 = ' ',
             * 27..36 = '0'..'9' (P1-ASM-19).
             * HISCORE.ASM:278-344 Get_name fills exactly name_size = 15 bytes. */
            for (int ni = 0; ni < HISCORE_NAME_LEN; ni++) {
                int lv = hs->letter_values[ni];
                char c;
                if (lv >= 0 && lv < 26)       c = (char)('A' + lv);
                else if (lv >= 27 && lv < 37) c = (char)('0' + (lv - 27));
                else                          c = ' ';
                e->name[ni] = c;
                s_last_name[ni] = c;
            }
            s_last_name[INPUT_NAME_LEN] = '\0';
            hiscore_save(scores, "data/blaster.scr");
        }
        hs->name_entry_active = 0;
        state->game_mode = STATE_MENU;
        state->current_menu = 6;
    }
}

/* Helper: draw a lowercase string with FONTE. */
static void draw_str(int x, int y, const char *s, Color tint) {
    if (s_font_ready)
        font_draw_string(&s_font, s, x, y, tint);
}

/* HISCORE.ASM:195 - print_score — all rendered with FONTE. */
void hiscore_screen_draw(HiscoreScreenState *hs, Hiscores *scores, int mode) {
    if (!hs || !scores || !hs->loaded) return;

    /* 1. Background + dark overlay */
    Rectangle src = {0, 0, (float)hs->backgrounds[hs->bg_index].width,
                            (float)hs->backgrounds[hs->bg_index].height};
    Rectangle dst = {0, 0, SCREEN_W, SCREEN_H};
    DrawTexturePro(hs->backgrounds[hs->bg_index], src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0, 0, 0, 180});

    /* 2. Title — FONTE (lowercase) */
    draw_str(220, 30, "high scores", WHITE);

    /* Hint: during name entry, show the confirm/cancel bindings so the
     * player isn't stuck wondering how to submit their score. */
    if (hs->name_entry_active) {
        draw_str(140, 440, "type name  -  enter to confirm  -  esc to cancel",
                 (Color){200, 200, 200, 255});
    } else {
        draw_str(200, 440, "press any key to continue",
                 (Color){200, 200, 200, 255});
    }

    /* 3. Column headers */
    draw_str(COL_RANK,  ROW_START_Y - 26, "#", WHITE);
    draw_str(COL_SCORE, ROW_START_Y - 26, "score", WHITE);
    draw_str(COL_LEVEL, ROW_START_Y - 26, "lv", WHITE);
    draw_str(COL_NAME,  ROW_START_Y - 26, "name", WHITE);

    /* 4. Entries */
    for (int i = 0; i < HISCORE_ENTRIES; i++) {
        HiscoreEntry *e = &scores->entries[mode][i];
        int y = ROW_START_Y + i * ROW_H;
        int is_entry_row = hs->name_entry_active && i == hs->entry_rank;

        /* Highlight active name-entry row */
        if (is_entry_row) {
            DrawRectangle(COL_RANK - 4, y - 2,
                          COL_NAME + INPUT_NAME_LEN * LETTER_W - COL_RANK + 12,
                          ROW_H - 2, (Color){80, 50, 120, 200});
        }

        Color tc = WHITE;
        char buf[16];

        /* Rank */
        snprintf(buf, sizeof(buf), "%02d", i + 1);
        draw_str(COL_RANK, y, buf, tc);

        /* Score */
        snprintf(buf, sizeof(buf), "%06d", e->value);
        draw_str(COL_SCORE, y, buf, tc);

        /* Level */
        snprintf(buf, sizeof(buf), "%.2s", e->level_text);
        /* lowercase for FONTE */
        for (int k = 0; buf[k]; k++)
            if (buf[k] >= 'A' && buf[k] <= 'Z') buf[k] += 32;
        draw_str(COL_LEVEL, y, buf, tc);

        /* Name */
        if (is_entry_row) {
            for (int j = 0; j < INPUT_NAME_LEN; j++) {
                int lv = hs->letter_values[j];
                /* 0..25=a..z, 26=space, 27..36=0..9 (P1-ASM-19). */
                char ch;
                if (lv >= 0 && lv < 26)       ch = (char)('a' + lv);
                else if (lv >= 27 && lv < 37) ch = (char)('0' + (lv - 27));
                else                          ch = ' ';
                char letter[2] = { ch, '\0' };
                int lx = COL_NAME + j * LETTER_W;

                if (j == hs->name_entry_pos && (hs->cursor_blink / 10) % 2 == 0) {
                    DrawRectangle(lx - 1, y - 1, LETTER_W, FONT_CHAR_H, WHITE);
                    if (s_font_ready)
                        font_draw_char(&s_font, letter[0], lx, y, (Color){40, 0, 60, 255});
                } else {
                    draw_str(lx, y, letter, WHITE);
                }
            }
        } else {
            char name[INPUT_NAME_LEN + 1];
            memcpy(name, e->name, INPUT_NAME_LEN);
            name[INPUT_NAME_LEN] = '\0';
            for (int k = 0; name[k]; k++)
                if (name[k] >= 'A' && name[k] <= 'Z') name[k] += 32;
            draw_str(COL_NAME, y, name, WHITE);
        }
    }
}
