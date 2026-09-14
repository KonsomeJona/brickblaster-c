/* screen_editor.c — In-game level editor.
 *
 * Screen: the EDITOR.ASM arrangement, as the standalone brickblaster-editor
 * reproduces it — the world's real background and brick sprites, the
 * original panel sprite (456,94,101,238) at (535,16) with its two selector
 * arrows (33,163,14,16), the cursor sprite (0,163,32,16), the level number
 * at (123,9) and option_text_editor at panel_info (Blaster.inc:355-388,
 * EDITOR.ASM:33-55). Brushes are the EDITOR.ASM codes 21/24/08/11/18 with
 * the colour in bits 7-6 (level_brush_code).
 *
 * Workflow added by the port, none of it in the ASM:
 *   - a column of buttons on the left, so mouse and touch need no shortcut;
 *   - every change is saved at once to the player's copy of the world
 *     (level.h), which the campaign then plays instead of the shipped file;
 *   - TEST / Tab plays the grid in the real game; clearing it, losing it,
 *     Tab or Esc come back here with the grid untouched (main.c);
 *   - undo, restore the shipped level, a new level after the last one,
 *     and dropping a .lv0/.lv1/.lv3 world or a .lvl level on the window.
 *
 * Keys: F1-F4 normal/multi/indestructible/transparent, F9 teleporter,
 * F5-F8 or 1-4 colours (EDITOR.ASM dispatch), F10 swap with the clipboard,
 * F11 restore the shipped level, PgUp/PgDn levels, arrows + Space/Delete
 * keyboard drawing, E eraser, W world, Ctrl+Z undo, Tab/Enter test, Esc menu.
 * F12 stays the global GIF recorder toggle.
 */

#include "screen_editor.h"
#include "assets.h"
#include "constants.h"
#include "font.h"
#include "i18n.h"
#include "input_gamepad.h"
#include "letterbox.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#define GRID_X          BRICK_ORIGIN_X      /* 112 */
#define CELL_W          BRICK_W             /* 32  */
#define CELL_H          BRICK_H             /* 16  */
/* EDITOR.ASM keeps the cursor on rows 2..25: rows 0-1 carry the level and
 * score panels, rows 26-29 lie below limite_y and are never collided. */
#define ROW_MIN         2
#define ROW_MAX         25
#define MESSAGE_FRAMES  150

static const Rectangle PANEL_SRC  = { 456,  94, 101, 238 };
static const Rectangle ARROW_SRC  = {  33, 163,  14,  16 };
static const Rectangle CURSOR_SRC = {   0, 163,  32,  16 };
#define PANEL_X      535
#define PANEL_Y       16
#define PANEL_ROW_Y   22     /* first icon row of the panel */
#define PANEL_ROW_H   18
#define ARROW_X      546

static const int WORLDS[] = { 0, 1, WORLD_ATOLL };
#define WORLD_COUNT ((int)(sizeof(WORLDS) / sizeof(WORLDS[0])))

enum { BTN_TEST, BTN_PREV, BTN_NEXT, BTN_WORLD, BTN_ERASER, BTN_UNDO,
       BTN_CLEAR, BTN_RESET, BTN_MENU, BTN_COUNT };
static const Rectangle BTN_RECT[BTN_COUNT] = {
    [BTN_TEST]   = {  6,  24, 58, 30 },
    [BTN_PREV]   = {  6,  62, 27, 30 },
    [BTN_NEXT]   = { 37,  62, 27, 30 },
    [BTN_WORLD]  = {  6, 100, 58, 30 },
    [BTN_ERASER] = {  6, 138, 58, 30 },
    [BTN_UNDO]   = {  6, 176, 58, 30 },
    [BTN_CLEAR]  = {  6, 214, 58, 30 },
    [BTN_RESET]  = {  6, 252, 58, 30 },
    [BTN_MENU]   = {  6, 440, 58, 30 },
};

static const Color ORANGE_UI = { 232, 115,  74, 255 };
static const Color MUTED_UI  = { 160, 168, 190, 255 };

/* F10 xchg_level clipboard — EDITOR.ASM:336-368, P1-ASM-26. */
static unsigned char s_clipboard[BRICK_COUNT];

/* ------------------------------------------------------------------ model */

static unsigned char *grid(EditorState *ed) {
    return ed->world_buf + (ed->level_num - 1) * BRICK_COUNT;
}

const unsigned char *editor_grid(const EditorState *ed) {
    return ed->world_buf + (ed->level_num - 1) * BRICK_COUNT;
}

static const unsigned char *ship_grid(const EditorState *ed) {
    return ed->ship_buf + (ed->level_num - 1) * BRICK_COUNT;
}

/* The slot right after the last level is still 0xFF-filled: shown empty,
 * it becomes a level with its first brick (search_level_number stops on a
 * block whose first byte is 0xFF, MAIN.ASM:5025-5041). */
static int slot_is_new(const EditorState *ed) {
    return editor_grid(ed)[0] == INVALIDE;
}

static int last_slot(const EditorState *ed) {
    int n = level_count_buffer(ed->world_buf);
    return (n < LEVELS_PER_FILE) ? n + 1 : LEVELS_PER_FILE;
}

int editor_world_valid(int world) {
    int i;
    for (i = 0; i < WORLD_COUNT; i++) if (WORLDS[i] == world) return 1;
    return 0;
}

static const char *world_name(int world) {
    if (world == 1)           return i18n(STR_M_ARCADE);
    if (world == WORLD_ATOLL) return i18n(STR_M_ATOLL);
    return i18n(STR_M_SPACE);
}

void editor_show_message(EditorState *ed, const char *msg) {
    ed->message = msg;
    ed->message_timer = MESSAGE_FRAMES;
}

static void persist_sync(void) {
#if defined(PLATFORM_WEB)
    /* Push MEMFS → IndexedDB; main.c mounted the user dir at start-up. */
    EM_ASM({ if (typeof FS !== 'undefined' && FS.syncfs) FS.syncfs(false, function (e) {}); });
#endif
}

static void save(EditorState *ed) {
    int rc;

    ed->dirty = 0;
    level_trim_world(ed->world_buf);
    if (ed->level_num > last_slot(ed)) ed->level_num = last_slot(ed);
    /* Back to the shipped bytes: drop the copy, so a later version of the
     * shipped world reaches this player again. */
    if (memcmp(ed->world_buf, ed->ship_buf, LEVEL_FILE_SIZE) == 0 &&
        level_remove_user_world(ed->world) == 0)
        rc = 0;
    else
        rc = level_write_user_world(ed->world, ed->world_buf);
    if (rc != 0) editor_show_message(ed, i18n(STR_OPT_ED_SAVE_FAILED));
    persist_sync();
}

static void load_world(EditorState *ed, int world) {
    ed->world = world;
    if (level_read_world(world, ed->ship_buf, 0) != 0)
        memset(ed->ship_buf, INVALIDE, LEVEL_FILE_SIZE);
    if (level_read_world(world, ed->world_buf, 1) != 0)
        memcpy(ed->world_buf, ed->ship_buf, LEVEL_FILE_SIZE);
    ed->undo_count = 0;
    ed->dirty = 0;
}

void editor_open(EditorState *ed, int world, int level_num) {
    if (!editor_world_valid(world)) world = 0;
    if (!ed->loaded || ed->world != world) {
        load_world(ed, world);
        ed->level_num = 1;
        if (!ed->loaded) {
            ed->brush = 0;
            ed->color = 0;
            ed->cursor_col = BRICK_COLS / 2;
            ed->cursor_row = ROW_MIN + 4;
        }
    }
    if (level_num > 0) ed->level_num = level_num;
    if (ed->level_num > last_slot(ed)) ed->level_num = last_slot(ed);
    if (ed->level_num < 1) ed->level_num = 1;
    ed->loaded         = 1;
    ed->need_release   = 1;
    ed->stroke_saved   = 0;
    ed->test_requested = 0;
}

static void push_undo(EditorState *ed) {
    if (ed->undo_count == EDITOR_UNDO_DEPTH) {
        memmove(&ed->undo[0], &ed->undo[1], sizeof(ed->undo[0]) * (EDITOR_UNDO_DEPTH - 1));
        ed->undo_count--;
    }
    ed->undo[ed->undo_count].level_num = ed->level_num;
    memcpy(ed->undo[ed->undo_count].bricks, grid(ed), BRICK_COUNT);
    ed->undo_count++;
}

static void undo(EditorState *ed) {
    EditorUndo *u;
    if (ed->undo_count == 0) return;
    u = &ed->undo[--ed->undo_count];
    ed->level_num = u->level_num;
    memcpy(grid(ed), u->bricks, BRICK_COUNT);
    ed->dirty = 1;
}

static void paint(EditorState *ed, int col, int row, unsigned char value) {
    unsigned char *g = grid(ed);
    int idx = row * BRICK_COLS + col;

    if (slot_is_new(ed)) {
        if (value == ABSENTE) return;               /* erasing nothing */
        if (!ed->stroke_saved) { push_undo(ed); ed->stroke_saved = 1; }
        memset(g, ABSENTE, BRICK_COUNT);            /* the slot becomes a level */
    } else {
        if (g[idx] == value) return;
        if (!ed->stroke_saved) { push_undo(ed); ed->stroke_saved = 1; }
    }
    g[idx] = value;
    ed->dirty = 1;
}

static void go_level(EditorState *ed, int n) {
    if (n < 1) n = 1;
    if (n > last_slot(ed)) n = last_slot(ed);
    ed->level_num = n;
}

static void next_world(EditorState *ed) {
    int i;
    if (ed->dirty) save(ed);
    for (i = 0; i < WORLD_COUNT; i++) if (WORLDS[i] == ed->world) break;
    load_world(ed, WORLDS[(i + 1) % WORLD_COUNT]);
    ed->level_num = 1;
}

static void clear_level(EditorState *ed) {
    if (slot_is_new(ed)) return;
    push_undo(ed);
    memset(grid(ed), ABSENTE, BRICK_COUNT);
    ed->dirty = 1;
}

static void restore_level(EditorState *ed) {
    if (memcmp(grid(ed), ship_grid(ed), BRICK_COUNT) == 0) return;
    push_undo(ed);
    memcpy(grid(ed), ship_grid(ed), BRICK_COUNT);
    ed->dirty = 1;
    editor_show_message(ed, i18n(STR_OPT_ED_RESTORED));
}

/* F10 xchg_level (EDITOR.ASM:336-364): swap the grid with the clipboard. */
static void swap_clipboard(EditorState *ed) {
    unsigned char tmp[BRICK_COUNT];
    int i;
    push_undo(ed);
    memcpy(tmp, grid(ed), BRICK_COUNT);
    memcpy(grid(ed), s_clipboard, BRICK_COUNT);
    for (i = 0; i < BRICK_COUNT; i++) if (tmp[i] == INVALIDE) tmp[i] = ABSENTE;
    memcpy(s_clipboard, tmp, BRICK_COUNT);
    ed->dirty = 1;
}

int editor_import_file(EditorState *ed, const char *path) {
    int size = 0, rc = -1, i;
    unsigned char *data = LoadFileData(path, &size);

    if (!ed->loaded) editor_open(ed, 0, 1);
    if (data && size == LEVEL_FILE_SIZE && level_count_buffer(data) > 0) {
        const char *ext = GetFileExtension(path);
        int world = ed->world;
        if (ext) {
            ext = TextToLower(ext);
            if (TextIsEqual(ext, ".lv0")) world = 0;
            else if (TextIsEqual(ext, ".lv1")) world = 1;
            else if (TextIsEqual(ext, ".lv3")) world = WORLD_ATOLL;
        }
        if (world != ed->world || !ed->loaded) load_world(ed, world);
        memcpy(ed->world_buf, data, LEVEL_FILE_SIZE);
        ed->undo_count = 0;
        ed->level_num  = 1;
        rc = 0;
    } else if (data && size == BRICK_COUNT && data[0] != INVALIDE) {
        /* A single .lvl (standalone editor, Ctrl E) replaces this level. */
        push_undo(ed);
        for (i = 0; i < BRICK_COUNT; i++)
            grid(ed)[i] = (data[i] == INVALIDE) ? ABSENTE : data[i];
        rc = 0;
    }
    if (data) UnloadFileData(data);
    if (rc == 0) {
        save(ed);
        editor_show_message(ed, i18n(STR_OPT_ED_IMPORTED));
    } else {
        editor_show_message(ed, i18n(STR_OPT_ED_REFUSED));
    }
    ed->need_release = 1;
    return rc;
}

/* ------------------------------------------------------------------ input */

/* Pointer in canvas space: first touch when there is one (web on a phone,
 * as in screen_menu.c), the mouse otherwise. */
static Vector2 pointer_canvas(void) {
    Vector2 p = (GetTouchPointCount() > 0) ? GetTouchPosition(0) : GetMousePosition();
    return letterbox_screen_to_canvas(p);
}

static void leave(ScreenState *state, EditorState *ed) {
    if (ed->dirty) save(ed);
    state->game_mode    = STATE_MENU;
    state->current_menu = 1;
}

static void panel_row_clicked(EditorState *ed, int row) {
    if (row < 4)       { ed->brush = row; ed->eraser = 0; }
    else if (row < 8)  { ed->color = row - 4; ed->eraser = 0; }
    else if (row == 8) { ed->brush = 4; ed->eraser = 0; }
    else if (row == 9) swap_clipboard(ed);
    else if (row == 10) restore_level(ed);           /* F11 icon */
    else if (row == 11) { save(ed); editor_show_message(ed, i18n(STR_OPT_ED_SAVED)); }
}

void editor_update(ScreenState *state, EditorState *ed, const FrameInput *input) {
    int ctrl, ldown, rdown, lpress, btn = -1, i;
    int key_draw, key_erase;
    Vector2 cp;

    (void)input;
    if (!ed->loaded) editor_open(ed, state->world, 0);
    if (ed->message_timer > 0) ed->message_timer--;

    ctrl   = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    ldown  = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    rdown  = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (!ldown && !rdown) ed->need_release = 0;
    lpress = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ed->need_release;
    cp     = pointer_canvas();

    if (lpress) {
        for (i = 0; i < BTN_COUNT; i++)
            if (CheckCollisionPointRec(cp, BTN_RECT[i])) btn = i;
    }

    if (IsKeyPressed(KEY_ESCAPE) || gamepad_back() || btn == BTN_MENU) {
        leave(state, ed);
        return;
    }
    if (btn == BTN_TEST || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ENTER)) {
        if (ed->dirty) save(ed);
        ed->test_requested = 1;
        return;
    }

    if (btn == BTN_UNDO || (ctrl && IsKeyPressed(KEY_Z))) undo(ed);
    if (btn == BTN_CLEAR) clear_level(ed);
    if (btn == BTN_RESET || IsKeyPressed(KEY_F11)) restore_level(ed);
    if (btn == BTN_WORLD || (!ctrl && IsKeyPressed(KEY_W))) next_world(ed);
    if (btn == BTN_ERASER || IsKeyPressed(KEY_E)) ed->eraser = !ed->eraser;
    if (btn == BTN_PREV || IsKeyPressed(KEY_PAGE_DOWN) || IsKeyPressed(KEY_LEFT_BRACKET))
        go_level(ed, ed->level_num - 1);
    if (btn == BTN_NEXT || IsKeyPressed(KEY_PAGE_UP) || IsKeyPressed(KEY_RIGHT_BRACKET))
        go_level(ed, ed->level_num + 1);

    /* EDITOR.ASM function-key dispatch: F1-F4 bricks, F5-F8 colours, F9
     * teleporter, F10 xchg_level. Number keys are the port's colour alias. */
    for (i = 0; i < 4; i++) {
        if (IsKeyPressed(KEY_F1 + i))  { ed->brush = i; ed->eraser = 0; }
        if (IsKeyPressed(KEY_F5 + i))  { ed->color = i; ed->eraser = 0; }
        if (IsKeyPressed(KEY_ONE + i)) { ed->color = i; ed->eraser = 0; }
    }
    if (IsKeyPressed(KEY_F9))  { ed->brush = 4; ed->eraser = 0; }
    if (IsKeyPressed(KEY_F10)) swap_clipboard(ed);

    if (lpress && CheckCollisionPointRec(cp, (Rectangle){ PANEL_X, PANEL_ROW_Y, PANEL_SRC.width, 12 * PANEL_ROW_H }))
        panel_row_clicked(ed, (int)((cp.y - PANEL_ROW_Y) / PANEL_ROW_H));

    /* Keyboard cursor, with the standalone editor's repeat rate. */
    {
        int dc = 0, dr = 0;
        if (IsKeyDown(KEY_LEFT))  dc = -1;
        if (IsKeyDown(KEY_RIGHT)) dc =  1;
        if (IsKeyDown(KEY_UP))    dr = -1;
        if (IsKeyDown(KEY_DOWN))  dr =  1;
        if (!dc && !dr) ed->next_cursor_move = 0.0;
        else if (GetTime() >= ed->next_cursor_move) {
            ed->cursor_col += dc;
            ed->cursor_row += dr;
            if (ed->cursor_col < 0) ed->cursor_col = 0;
            if (ed->cursor_col > BRICK_COLS - 1) ed->cursor_col = BRICK_COLS - 1;
            if (ed->cursor_row < ROW_MIN) ed->cursor_row = ROW_MIN;
            if (ed->cursor_row > ROW_MAX) ed->cursor_row = ROW_MAX;
            ed->next_cursor_move = GetTime() + (ed->next_cursor_move == 0.0 ? 0.25 : 0.09);
        }
    }

    /* Pointer on the grid moves the cursor and paints. */
    if (cp.x >= GRID_X && cp.x < GRID_X + BRICK_COLS * CELL_W &&
        cp.y >= ROW_MIN * CELL_H && cp.y < (ROW_MAX + 1) * CELL_H) {
        Vector2 d = GetMouseDelta();
        if (d.x != 0.0f || d.y != 0.0f || ldown || rdown) {
            ed->cursor_col = (int)((cp.x - GRID_X) / CELL_W);
            ed->cursor_row = (int)(cp.y / CELL_H);
        }
        if (!ed->need_release) {
            if (ldown)
                paint(ed, ed->cursor_col, ed->cursor_row,
                      ed->eraser ? ABSENTE : level_brush_code(ed->brush, ed->color));
            else if (rdown)
                paint(ed, ed->cursor_col, ed->cursor_row, ABSENTE);
        }
    }

    key_draw  = IsKeyDown(KEY_SPACE);
    key_erase = IsKeyDown(KEY_DELETE) || IsKeyDown(KEY_BACKSPACE);
    if (key_draw)
        paint(ed, ed->cursor_col, ed->cursor_row,
              ed->eraser ? ABSENTE : level_brush_code(ed->brush, ed->color));
    else if (key_erase)
        paint(ed, ed->cursor_col, ed->cursor_row, ABSENTE);

    /* One stroke = one undo step = one save, when everything is released. */
    if (!ldown && !rdown && !key_draw && !key_erase) {
        ed->stroke_saved = 0;
        if (ed->dirty) save(ed);
    }
}

/* ------------------------------------------------------------------- draw */

static void draw_button(Rectangle r, const char *label, const char *key,
                        int active, Vector2 pointer) {
    int hot = CheckCollisionPointRec(pointer, r);
    int w;
    DrawRectangleRounded(r, 0.25f, 4, hot ? (Color){ 60, 60, 92, 230 } : (Color){ 18, 18, 30, 210 });
    DrawRectangleRoundedLines(r, 0.25f, 4, 1.0f, active ? ORANGE_UI : (Color){ 110, 110, 135, 220 });
    w = MeasureText(label, 10);
    DrawText(label, (int)(r.x + (r.width - w) / 2), (int)r.y + (key ? 5 : 10), 10, WHITE);
    if (key) {
        w = MeasureText(key, 10);
        DrawText(key, (int)(r.x + (r.width - w) / 2), (int)r.y + 17, 10, MUTED_UI);
    }
}

void editor_draw(EditorState *ed, DrawContext *dc) {
    Assets *a = dc->assets;
    const unsigned char *g = editor_grid(ed);
    int fresh = slot_is_new(ed), i;
    Vector2 pointer = pointer_canvas();
    Color line = { 255, 255, 255, 26 };
    const char *status;
    Color status_color;

    /* Same palette as the game will use for this world (FILE.ASM:776-791). */
    if (a->sprite_world != level_world_palette(ed->world))
        assets_select_world(a, level_world_palette(ed->world));

    ClearBackground(BLACK);
    draw_world_background(dc, ed->world, ed->level_num);

    /* Faint grid on the editable rows only — an aid, not original art. */
    for (i = 0; i <= BRICK_COLS; i++)
        DrawLine(GRID_X + i * CELL_W, ROW_MIN * CELL_H,
                 GRID_X + i * CELL_W, (ROW_MAX + 1) * CELL_H, line);
    for (i = ROW_MIN; i <= ROW_MAX + 1; i++)
        DrawLine(GRID_X, i * CELL_H, GRID_X + BRICK_COLS * CELL_W, i * CELL_H, line);

    if (!fresh && a->sprite_sheet_loaded) {
        for (i = 0; i < BRICK_COUNT; i++) {
            unsigned char b = g[i];
            if (b == ABSENTE || b == INVALIDE) continue;
            DrawTextureRec(a->sprite_sheet, draw_brick_source(b),
                           (Vector2){ (float)(GRID_X + (i % BRICK_COLS) * CELL_W),
                                      (float)((i / BRICK_COLS) * CELL_H) }, WHITE);
        }
    }

    if (a->sprite_sheet_loaded) {
        DrawTextureRec(a->sprite_sheet, CURSOR_SRC,
                       (Vector2){ (float)(GRID_X + ed->cursor_col * CELL_W),
                                  (float)(ed->cursor_row * CELL_H) },
                       ed->eraser ? (Color){ 255, 110, 110, 255 } : WHITE);
        DrawTextureRec(a->sprite_sheet, PANEL_SRC, (Vector2){ PANEL_X, PANEL_Y }, WHITE);
        if (!ed->eraser)
            DrawTextureRec(a->sprite_sheet, ARROW_SRC,
                           (Vector2){ ARROW_X, (float)(PANEL_ROW_Y + PANEL_ROW_H * (ed->brush == 4 ? 8 : ed->brush)) },
                           WHITE);
        DrawTextureRec(a->sprite_sheet, ARROW_SRC,
                       (Vector2){ ARROW_X, (float)(PANEL_ROW_Y + PANEL_ROW_H * (ed->color + 4)) }, WHITE);
    }

    /* init_panel / init_score slots: level number, and the world's name
     * where the score would be. */
    font_draw_string(&dc->font, TextFormat("%02d", ed->level_num), 123, 9, WHITE);
    font_draw_string(&dc->font, world_name(ed->world), 426, 9, WHITE);

    draw_button(BTN_RECT[BTN_TEST],   i18n(STR_ED_TEST),   "tab",    1, pointer);
    draw_button(BTN_RECT[BTN_PREV],   "<",                 "pgdn",   0, pointer);
    draw_button(BTN_RECT[BTN_NEXT],   ">",                 "pgup",   0, pointer);
    draw_button(BTN_RECT[BTN_WORLD],  i18n(STR_ED_WORLD),  "w",      0, pointer);
    draw_button(BTN_RECT[BTN_ERASER], i18n(STR_ED_ERASER), "e",      ed->eraser, pointer);
    draw_button(BTN_RECT[BTN_UNDO],   i18n(STR_ED_UNDO),   "ctrl z", 0, pointer);
    draw_button(BTN_RECT[BTN_CLEAR],  i18n(STR_ED_CLEAR),  NULL,     0, pointer);
    draw_button(BTN_RECT[BTN_RESET],  i18n(STR_ED_RESET),  "f11",    0, pointer);
    draw_button(BTN_RECT[BTN_MENU],   i18n(STR_ED_MENU),   "esc",    0, pointer);

    /* Level status and a short reminder under the panel. */
    if (fresh) {
        status = i18n(STR_ED_NEW_SLOT);  status_color = (Color){ 250, 220, 90, 255 };
    } else if (memcmp(g, ship_grid(ed), BRICK_COUNT) != 0) {
        status = i18n(STR_ED_EDITED);    status_color = ORANGE_UI;
    } else {
        status = i18n(STR_ED_SHIPPED);   status_color = MUTED_UI;
    }
    /* The border art under this corner is busy: give the text a backing. */
    DrawRectangleRounded((Rectangle){ 532, 257, 106, 100 }, 0.12f, 4, (Color){ 0, 0, 0, 165 });
    DrawText(TextFormat("%s %02d/%02d", i18n(STR_ED_LEVEL), ed->level_num,
                        level_count_buffer(ed->world_buf)), 536, 262, 10, WHITE);
    DrawText(status, 536, 276, 10, status_color);
    DrawText(i18n(STR_ED_HELP_DRAW),  536, 300, 10, MUTED_UI);
    DrawText(i18n(STR_ED_HELP_ERASE), 536, 314, 10, MUTED_UI);
    DrawText(i18n(STR_ED_HELP_TEST),  536, 328, 10, MUTED_UI);
    DrawText(i18n(STR_ED_HELP_DROP),  536, 342, 10, MUTED_UI);

    /* EDITOR.ASM:54-55 prints option_text_editor at panel_info; port
     * messages borrow the slot for a moment. */
    font_draw_string(&dc->font,
                     (ed->message_timer > 0 && ed->message) ? ed->message : i18n(STR_OPT_EDITOR),
                     PANEL_INFO_POS_X, PANEL_INFO_POS_Y, WHITE);
}
