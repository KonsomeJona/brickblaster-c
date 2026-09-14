#pragma once
/* screen_editor.h — In-game level editor (EDITOR.ASM screen, port workflow).
 *
 * Edits one world at a time and saves every change straight away to the
 * player's copy of that world (level.h), which the game then plays. TEST /
 * Tab hands the grid in memory to main.c, which runs it in the real game and
 * comes back here when that run ends.
 */

#include "screen_manager.h"
#include "input_frame.h"
#include "level.h"
#include "draw.h"

#define EDITOR_UNDO_DEPTH 32

typedef struct {
    int           level_num;
    unsigned char bricks[BRICK_COUNT];
} EditorUndo;

typedef struct {
    int   loaded;
    int   world;            /* 0 space, 1 arcade, WORLD_ATOLL */
    int   level_num;        /* 1..80 — may be the empty slot after the last level */
    int   brush;            /* 0 normal, 1 multi, 2 indestructible, 3 transparent, 4 teleporter */
    int   color;            /* 0..3 green / blue / violet / orange */
    int   eraser;           /* 1: left click erases (touch screens have no right click) */
    int   cursor_col, cursor_row;
    int   stroke_saved;     /* undo snapshot already taken for the current stroke */
    int   need_release;     /* ignore the press that opened the editor */
    int   dirty;            /* world_buf changed since the last save */
    int   test_requested;   /* set by TEST / Tab, consumed by main.c */
    int   message_timer;
    const char *message;    /* 18-char FONTE banner shown at panel_info */
    double next_cursor_move;
    unsigned char world_buf[LEVEL_FILE_SIZE];   /* what the game will play */
    unsigned char ship_buf[LEVEL_FILE_SIZE];    /* shipped file, for RESET */
    EditorUndo undo[EDITOR_UNDO_DEPTH];
    int   undo_count;
} EditorState;

/* Worlds the editor and the world menu offer: space, arcade, atoll. */
int  editor_world_valid(int world);

/* Show `world` at `level_num`. level_num 0 keeps the current level when the
 * editor already shows that world (coming back to the editor resumes). */
void editor_open(EditorState *ed, int world, int level_num);

void editor_update(ScreenState *state, EditorState *ed, const FrameInput *input);
void editor_draw(EditorState *ed, DrawContext *dc);

/* The 390 bytes being edited (what TEST plays). */
const unsigned char *editor_grid(const EditorState *ed);

void editor_show_message(EditorState *ed, const char *msg);

/* Import a dropped / command-line file: 31,200 bytes replaces a whole world
 * (.lv0/.lv1/.lv3 pick the world, anything else the current one), 390 bytes
 * (.lvl from the standalone editor) replaces the current level. 0 on success. */
int  editor_import_file(EditorState *ed, const char *path);
