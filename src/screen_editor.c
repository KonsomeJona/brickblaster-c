/* screen_editor.c — Level editor port (EDITOR.ASM).
 *
 * Grid 13×30, brick encoding = [CC|TTT|HHHHH] (Blaster.inc:390-406).
 * Saves to data/custom_<world>.lv<diff> so original levels are preserved.
 *
 * Controls:
 *   Mouse Left    — place brick with current (color, type, hp)
 *   Mouse Right   — erase
 *   1..4          — cycle color (green/blue/violet/orange)
 *   N             — set type Normal (1 HP)               EDITOR.ASM:223-230 F1
 *   M             — set type Normal/Multi (4 HP, P1-ASM-25)  EDITOR.ASM:232-239 F2
 *   I             — set type Indestructible              EDITOR.ASM:241-248 F3
 *   T             — set type Transparent                 EDITOR.ASM:250-257 F4
 *   L             — set type Teleporter (P1-ASM-25)      EDITOR.ASM:259-266 F5
 *   + / -         — increase / decrease HP
 *   PgUp / PgDn   — next / prev level
 *   S             — save
 *   F10           — swap current level with clipboard    EDITOR.ASM:336-364 xchg_level
 *   ESC           — back to menu
 */

#include "screen_editor.h"
#include "font.h"
#include "assets.h"
#include "constants.h"
#include "input_gamepad.h"
#include "letterbox.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(_WIN32)
  #include <direct.h>
  #define PORTABLE_MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #define PORTABLE_MKDIR(p) mkdir((p), 0755)
#endif

#define GRID_X       BRICK_ORIGIN_X     /* 112 */
#define GRID_Y       BRICK_ORIGIN_Y     /* 0   */
#define CELL_W       BRICK_W            /* 32  */
#define CELL_H       BRICK_H            /* 16  */

static BitmapFont s_font;
static int s_font_ready = 0;

/* F10 xchg_level clipboard — EDITOR.ASM:336-368 swaps current level with
 * two scratch buffers. A single 390-byte buffer is enough for a swap if
 * we stage through a temporary. P1-ASM-26. */
static unsigned char s_clipboard[BRICK_COUNT];

/* Color palette for brick preview (approximate original colors) */
static const Color BRICK_COLORS[4] = {
    { 60, 200, 80, 255 },   /* 0 green  */
    { 60, 120, 220, 255 },  /* 1 blue   */
    { 180, 60, 200, 255 },  /* 2 violet */
    { 230, 150, 40, 255 },  /* 3 orange */
};

void editor_init(EditorState *ed) {
    if (!ed) return;
    memset(ed, 0, sizeof(*ed));
    ed->world          = 0;
    ed->difficulty_idx = 0;
    ed->level_num      = 1;
    ed->sel_color      = 0;
    ed->sel_type       = 1;
    ed->sel_hp         = 1;
    ed->hover_col      = -1;
    ed->hover_row      = -1;
    ed->message_timer  = 0;
    memset(ed->level.bricks, 0, BRICK_COUNT);
    ed->level.cols = BRICK_COLS;
    ed->level.rows = BRICK_ROWS;

    /* Get font ref from global assets. Font already loaded by assets_load. */
    extern Assets *g_editor_assets;
    if (g_editor_assets && g_editor_assets->font_sheet_loaded) {
        font_init(&s_font, &g_editor_assets->font_sheet);
        s_font_ready = 1;
    }
    ed->loaded = 1;
}

/* Quick access to global assets (set by editor_bind_assets in main). */
Assets *g_editor_assets = NULL;
void editor_bind_assets(Assets *a) { g_editor_assets = a; if (a && a->font_sheet_loaded) { font_init(&s_font, &a->font_sheet); s_font_ready = 1; } }

static void build_filename(const EditorState *ed, char *out, int n) {
    snprintf(out, n, "data/custom_%d.lv%d", ed->world, ed->difficulty_idx);
}

static void editor_save(EditorState *ed) {
    char path[128];
    build_filename(ed, path, sizeof(path));
    /* Ensure the data/ directory exists before opening for write — otherwise
     * fopen(..., "wb") silently fails and the user loses their edits.
     * raylib 5.0 doesn't ship MakeDirectory, so fall back to the platform
     * mkdir (_mkdir on Windows, mkdir(path, mode) elsewhere). */
    if (!DirectoryExists("data")) {
        PORTABLE_MKDIR("data");
    }
    /* Ensure file is exactly 31200 bytes. Load existing or create zeroed. */
    unsigned char *buf = (unsigned char *)calloc(1, 31200);
    if (!buf) return;
    FILE *f = fopen(path, "rb");
    if (f) { fread(buf, 1, 31200, f); fclose(f); }
    /* Splice our level in at offset (level_num-1)*390 */
    long off = (long)(ed->level_num - 1) * BRICK_COUNT;
    memcpy(buf + off, ed->level.bricks, BRICK_COUNT);
    f = fopen(path, "wb");
    if (f) {
        fwrite(buf, 1, 31200, f);
        fclose(f);
        snprintf(ed->message, sizeof(ed->message), "saved lvl %d to %s",
                 ed->level_num, path);
    } else {
        snprintf(ed->message, sizeof(ed->message), "save failed: %s", path);
    }
    free(buf);
    ed->message_timer = 120;  /* 2 s */
}

static void editor_load(EditorState *ed) {
    char path[128];
    build_filename(ed, path, sizeof(path));
    if (level_load(&ed->level, path, ed->level_num) != 0) {
        /* Fall back to blank */
        memset(ed->level.bricks, 0, BRICK_COUNT);
        snprintf(ed->message, sizeof(ed->message),
                 "new lvl %d (no file yet)", ed->level_num);
    } else {
        snprintf(ed->message, sizeof(ed->message),
                 "loaded lvl %d from %s", ed->level_num, path);
    }
    ed->message_timer = 120;
}

static unsigned char pack_brick(int color, int type, int hp) {
    /* bits 7-6 color, 5-3 type (Blaster.inc:399-403), 4-0 hp (Blaster.inc:391). */
    int type_bits = 0;
    int hp_override = hp;
    switch (type) {
        case 1: type_bits = 0x20; break;  /* NORMALE          */
        case 2: type_bits = 0x08; break;  /* INCASSABLE       */
        case 3: type_bits = 0x10; break;  /* TRANSPARENTE     */
        case 4: type_bits = 0x20; hp_override = 4; break; /* multi — EDITOR.ASM:234-235
                                                             (normale + hp=4), P1-ASM-25 */
        case 5: type_bits = 0x18; break;  /* TELEPORTEUSE     EDITOR.ASM:261-262 */
    }
    if (hp_override < 1) hp_override = 1;
    if (hp_override > 31) hp_override = 31;
    return (unsigned char)(((color & 3) << 6) | type_bits | (hp_override & 0x1F));
}

static Vector2 screen_to_canvas(float sx, float sy) {
    return letterbox_screen_to_canvas((Vector2){ sx, sy });
}

void editor_update(ScreenState *state, EditorState *ed, const FrameInput *input) {
    if (!state || !ed || !ed->loaded) return;
    (void)input;

    if (ed->message_timer > 0) ed->message_timer--;

    if (IsKeyPressed(KEY_ESCAPE) || gamepad_back()) {
        state->game_mode = STATE_MENU;
        state->current_menu = 6;
        return;
    }

    /* Color keys 1-4 */
    if (IsKeyPressed(KEY_ONE))   ed->sel_color = 0;
    if (IsKeyPressed(KEY_TWO))   ed->sel_color = 1;
    if (IsKeyPressed(KEY_THREE)) ed->sel_color = 2;
    if (IsKeyPressed(KEY_FOUR))  ed->sel_color = 3;

    /* Type keys — 5 brick types to match EDITOR.ASM F1-F5 (P1-ASM-25). */
    if (IsKeyPressed(KEY_N)) ed->sel_type = 1;   /* normale      F1 */
    if (IsKeyPressed(KEY_M)) ed->sel_type = 4;   /* multi (hp=4) F2 */
    if (IsKeyPressed(KEY_I)) ed->sel_type = 2;   /* incassable   F3 */
    if (IsKeyPressed(KEY_T)) ed->sel_type = 3;   /* transparente F4 */
    if (IsKeyPressed(KEY_L)) ed->sel_type = 5;   /* teleporteuse F5 */

    /* F10: xchg_level — swap current grid with the clipboard buffer.
     * Original algo (EDITOR.ASM:336-364): memcpy level→clip1, level→clip2,
     * clip1→level. With a single buffer we just xor-swap. P1-ASM-26. */
    if (IsKeyPressed(KEY_F10)) {
        unsigned char tmp[BRICK_COUNT];
        memcpy(tmp,              ed->level.bricks, BRICK_COUNT);
        memcpy(ed->level.bricks, s_clipboard,      BRICK_COUNT);
        memcpy(s_clipboard,      tmp,              BRICK_COUNT);
        snprintf(ed->message, sizeof(ed->message), "xchg clipboard (F10)");
        ed->message_timer = 120;
    }

    /* HP */
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))      if (ed->sel_hp < 31) ed->sel_hp++;
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) if (ed->sel_hp > 1)  ed->sel_hp--;

    /* Level nav */
    if (IsKeyPressed(KEY_PAGE_UP)   || IsKeyPressed(KEY_RIGHT_BRACKET)) {
        if (ed->level_num < 80) ed->level_num++;
        editor_load(ed);
    }
    if (IsKeyPressed(KEY_PAGE_DOWN) || IsKeyPressed(KEY_LEFT_BRACKET)) {
        if (ed->level_num > 1) ed->level_num--;
        editor_load(ed);
    }

    /* Save / Load — F11 reloads the current level from disk
     * (L is now the teleporter type key per EDITOR.ASM:259-266). */
    if (IsKeyPressed(KEY_S))   editor_save(ed);
    if (IsKeyPressed(KEY_F11)) editor_load(ed);

    /* Ctrl+Ins — ins_level (EDITOR.ASM:372-399).
     * ASM algorithm:
     *   cmp level_number,80 ; je @@end     ; bail at max
     *   inc level_number
     *   cmp B [eax],-1                     ; next slot sentinel
     *   jne Error_File
     *   rep stosb (zero 390 bytes)         ; zero-fill new level
     *   call create_wall
     * C adaptation: advance to the next level slot and clear it in place.
     * The C level format packs every slot as a valid 390-byte payload so
     * we skip the 0xFF sentinel check — every slot is always present. */
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
        && IsKeyPressed(KEY_INSERT)) {
        if (ed->level_num < 80) {
            ed->level_num++;
            memset(ed->level.bricks, 0, BRICK_COUNT);
            snprintf(ed->message, sizeof(ed->message),
                     "ins_level -- new empty lvl %d", ed->level_num);
        } else {
            snprintf(ed->message, sizeof(ed->message),
                     "ins_level -- at last lvl (%d)", ed->level_num);
        }
        ed->message_timer = 120;
    }

    /* Compute hover cell */
    Vector2 mp = GetMousePosition();
    Vector2 cp = screen_to_canvas(mp.x, mp.y);
    int col = (int)((cp.x - GRID_X) / CELL_W);
    int row = (int)((cp.y - GRID_Y) / CELL_H);
    if (col >= 0 && col < BRICK_COLS && row >= 0 && row < BRICK_ROWS) {
        ed->hover_col = col;
        ed->hover_row = row;
    } else {
        ed->hover_col = -1;
        ed->hover_row = -1;
    }

    /* Place / erase */
    if (ed->hover_col >= 0 && ed->hover_row >= 0) {
        int idx = ed->hover_row * BRICK_COLS + ed->hover_col;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            ed->level.bricks[idx] = pack_brick(ed->sel_color, ed->sel_type, ed->sel_hp);
        else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
            ed->level.bricks[idx] = 0;
    }
}

static void draw_brick_preview(int x, int y, unsigned char b) {
    if (b == 0) return;
    int color = (b >> 6) & 3;
    int type_bits = b & 0x38;
    int hp = b & 0x1F;
    Color base = BRICK_COLORS[color];
    if (type_bits == 0x08) base = (Color){150, 150, 150, 255};           /* INCASSABLE  gray */
    if (type_bits == 0x10) base = (Color){base.r, base.g, base.b, 128};  /* TRANSPARENTE */
    if (type_bits == 0x18) base = (Color){200, 140, 255, 255};           /* TELEPORTEUSE purple-tint */
    DrawRectangle(x, y, CELL_W - 1, CELL_H - 1, base);
    /* HP indicator — darker inner stripes */
    if (hp > 1) {
        int stripes = (hp > 5) ? 5 : hp - 1;
        for (int s = 0; s < stripes; s++)
            DrawRectangle(x + 2 + s*3, y + 2, 2, 2, BLACK);
    }
}

void editor_draw(EditorState *ed) {
    if (!ed || !ed->loaded) return;

    ClearBackground((Color){30, 30, 40, 255});

    /* Playfield outline */
    DrawRectangleLinesEx((Rectangle){
        GRID_X - 1, GRID_Y - 1,
        BRICK_COLS * CELL_W + 2, BRICK_ROWS * CELL_H + 2
    }, 1.0f, (Color){80, 80, 100, 255});

    /* Grid lines */
    for (int c = 0; c <= BRICK_COLS; c++) {
        int x = GRID_X + c * CELL_W;
        DrawLine(x, GRID_Y, x, GRID_Y + BRICK_ROWS * CELL_H, (Color){60, 60, 80, 255});
    }
    for (int r = 0; r <= BRICK_ROWS; r++) {
        int y = GRID_Y + r * CELL_H;
        DrawLine(GRID_X, y, GRID_X + BRICK_COLS * CELL_W, y, (Color){60, 60, 80, 255});
    }

    /* Bricks */
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            int idx = r * BRICK_COLS + c;
            unsigned char b = ed->level.bricks[idx];
            draw_brick_preview(GRID_X + c * CELL_W, GRID_Y + r * CELL_H, b);
        }
    }

    /* Hover highlight */
    if (ed->hover_col >= 0 && ed->hover_row >= 0) {
        int x = GRID_X + ed->hover_col * CELL_W;
        int y = GRID_Y + ed->hover_row * CELL_H;
        DrawRectangle(x, y, CELL_W - 1, CELL_H - 1, (Color){255, 255, 255, 60});
        DrawRectangleLines(x, y, CELL_W, CELL_H, WHITE);
    }

    /* Side panel: current brush preview + status */
    int px = 10, py = 10;
    DrawRectangle(px, py, 90, 60, (Color){20, 20, 30, 200});
    DrawRectangleLines(px, py, 90, 60, WHITE);
    if (s_font_ready) {
        char buf[64];
        snprintf(buf, sizeof(buf), "lvl %d", ed->level_num);
        font_draw_string(&s_font, buf, px + 4, py + 4, WHITE);
        const char *type_name = (ed->sel_type == 1) ? "normal"
                             : (ed->sel_type == 2) ? "indestr"
                             : (ed->sel_type == 3) ? "transp"
                             : (ed->sel_type == 4) ? "multi"
                             : (ed->sel_type == 5) ? "telepod"
                             : "?";
        font_draw_string(&s_font, type_name, px + 4, py + 26, WHITE);
        snprintf(buf, sizeof(buf), "hp %d", ed->sel_hp);
        font_draw_string(&s_font, buf, px + 4, py + 48, WHITE);
    }
    /* Brush preview swatch */
    DrawRectangle(px + 60, py + 20, 24, 16, BRICK_COLORS[ed->sel_color]);
    DrawRectangleLines(px + 60, py + 20, 24, 16, WHITE);

    /* Help text */
    if (s_font_ready) {
        font_draw_string(&s_font, "1-4 color  n/m/i/t/l type  +/- hp",
                         60, SCREEN_H - 48, WHITE);
        font_draw_string(&s_font, "s save  f10 xchg  f11 reload  pgup/pgdn lvl  esc",
                         20, SCREEN_H - 24, WHITE);
    }

    /* Message */
    if (ed->message_timer > 0 && s_font_ready) {
        font_draw_string(&s_font, ed->message, 10, SCREEN_H - 70,
                         (Color){255, 200, 80, 255});
    }
}
