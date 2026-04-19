#pragma once
/* screen_hiscore.h — High score display and name entry
 * HISCORE.ASM:178-212 _Display_score / print_score
 * HISCORE.ASM:278-344 Get_name
 * HISCORE.ASM:358-382 Print_Cursor (blinking cursor)
 */

#include "screen_manager.h"
#include "hiscore.h"
#include "input_frame.h"
#include "assets.h"
#include <raylib.h>

typedef struct {
    Texture2D backgrounds[3];  // 00_01.png, 00_02.png, 00_03.png (FILE.ASM:437-439)
    int bg_index;              // Random 0-2 (FILE.ASM:437 get_random(3))
    int name_entry_active;     // 1 if waiting for name entry
    int name_entry_pos;        // Current character position in name
    int cursor_blink;          // Cursor blink counter
    int entry_rank;            // Which rank is being entered (0-14)
    int loaded;                // 1 if all resources loaded
    int letter_values[16];     // Current letter index per position:
                                //   0..25 = A..Z, 26 = ' ', 27..36 = '0'..'9'
                                // (P1-ASM-19, HISCORE.ASM:306-320 accepts al>=0x20)
} HiscoreScreenState;

/* Load high score screen assets. game_assets is needed for FONTE. */
void hiscore_screen_load(HiscoreScreenState *hs, Assets *game_assets);

/* Unload high score screen assets */
void hiscore_screen_unload(HiscoreScreenState *hs);

/* Update high score screen.
 * scores: pointer to the live hiscore table (for saving name on Enter). */
void hiscore_screen_update(ScreenState *state, HiscoreScreenState *hs, Hiscores *scores,
                           const FrameInput *input);

/* Draw high score screen */
void hiscore_screen_draw(HiscoreScreenState *hs, Hiscores *scores, int mode);
