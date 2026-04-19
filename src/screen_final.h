#pragma once
/* screen_final.h — Victory animation (Blaster.flc) + post-anim text modal.
 *
 * FILE.ASM:118-183   — 384-frame FLC (P1-ASM-27 cap 417-33).
 * HISCORE.ASM:5-83   — Display_score_from_final: FLC → modal → hiscore.
 */

#include "screen_manager.h"
#include "animation.h"
#include "input_frame.h"
#include "assets.h"
#include "game.h"

typedef struct {
    Animation final_anim;   /* Blaster.flc — 384 frames. */
    int       phase;        /* 0 = anim playing, 1 = modal text panel */
    int       modal_timer;  /* frames since modal began (HISCORE.ASM:46). */
    int       loaded;
} FinalAssets;

/* Load victory animation (384 PNG frames). */
void final_assets_load(FinalAssets *assets);

/* Unload victory animation. */
void final_assets_unload(FinalAssets *assets);

/* Bind the shared font sheet so the modal text renders. */
void final_bind_font(Assets *game_assets);

/* Advance the FLC / modal state machine. */
void final_update(ScreenState *state, FinalAssets *assets, const FrameInput *input);

/* Draw the animation layer. */
void final_draw(FinalAssets *assets);

/* Draw the post-animation modal text panel.
 * Must be called AFTER final_draw and BEFORE EndDrawing — it needs
 * access to the solo/coop vs duel game context. */
void final_draw_modal(FinalAssets *assets, ScreenState *state, Game *game);
