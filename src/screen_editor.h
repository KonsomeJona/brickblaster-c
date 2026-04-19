#pragma once
/* screen_editor.h — Level editor (EDITOR.ASM port).
 *
 * 13×30 brick grid on the 640×480 canvas. Mouse places/erases, keys cycle
 * brick parameters, S saves to data/custom_<world>.lv<diff>.
 */

#include "screen_manager.h"
#include "input_frame.h"
#include "level.h"
#include <raylib.h>

typedef struct {
    Level level;              /* current 13×30 grid + metadata */
    int   world;              /* 0 or 1 */
    int   difficulty_idx;     /* 0/1/2 = easy/medium/hard → lv0/1/2 */
    int   level_num;          /* 1..80 */
    int   sel_color;          /* 0..3 */
    int   sel_type;           /* 1=normal, 2=indestr, 3=transparent */
    int   sel_hp;             /* 1..31 */
    int   hover_col, hover_row;
    int   message_timer;      /* frames to show last save/load message */
    char  message[64];
    int   loaded;
} EditorState;

void editor_init(EditorState *ed);
void editor_update(ScreenState *state, EditorState *ed, const FrameInput *input);
void editor_draw(EditorState *ed);
