#pragma once
/*
 * draw.h — BrickBlaster render system
 *
 * Renders the 640x480 game world to a RenderTexture2D (off-screen canvas),
 * then scales that canvas to fill the window.
 *
 * ASM source reference: DRAW.ASM
 *   - Refresh_Sprites  (DRAW.ASM:368)  — per-sprite animation tick
 *   - Draw_sprites     (DRAW.ASM:426)  — blit each sprite to video buffer
 *   - Erase_sprites    (DRAW.ASM:524)  — restore background under old sprite pos
 *   - Flip_Buffer      (DRAW.ASM:831)  — copy entire video buffer to screen
 *   - Draw_Shape       (DRAW.ASM:781)  — draw a single shape by pos/size
 *
 * The original DOS game uses:
 *   1. Background_Buffer — pre-rendered background (background drawn once)
 *   2. Video_Buffer      — composited frame (background + sprites)
 *   3. Flip to VESA screen via SBANK calls
 *
 * In the raylib port we use:
 *   1. RenderTexture2D canvas (640x480) as our "Video_Buffer" equivalent
 *   2. Every frame we redraw: background → bricks → balls → paddle → powerups → HUD
 *   3. Scale canvas to window using DrawTexturePro with flipped V coord
 *
 * Sprite regions from Blaster.inc (verified against sprite_atlas.gd):
 *
 *   Ball blue:          Rect(  0,  0, 9, 9)   — Blaster.inc:332
 *   Ball orange:        Rect(  0,  9, 9, 9)   — Blaster.inc:333
 *   Ghost ball blue:    Rect( 18,  0, 9, 9)   — Blaster.inc:337
 *   Ghost ball orange:  Rect( 18,  9, 9, 9)   — Blaster.inc:338
 *   Paddle P1 normal:   Rect(  0, 42, 74, 25) — Blaster.inc:225 (y=0+screen_x*42)
 *   Paddle P1 large:    Rect(296, 42,105, 25) — Blaster.inc:281
 *   Paddle P1 small:    Rect(506, 42, 38, 25) — Blaster.inc:286
 *   Brick classic:      Rect( 66, 95, 32, 16) — Blaster.inc:342
 *   Brick color stride: 17 rows               — Blaster.inc:345 next_color=screen_x*17
 *   Brick damage stride:33 px horizontal      — Blaster.inc:346 next_brique=33
 *   Powerup row 1:      Rect(  1,752, 26, 24) — MAIN.ASM option table
 *   Level panel:        Rect(  1, 19, 30, 22) — Blaster.inc:204
 *   Score panel:        Rect( 32, 19, 90, 22) — Blaster.inc:212
 */

#include "raylib.h"
#include "game.h"
#include "font.h"
#include "assets.h"

/* Window size = 2x the 640x480 game canvas (integer pixel-perfect scale) */
#define WINDOW_W    1280
#define WINDOW_H     960

typedef struct {
    RenderTexture2D canvas;     /* 640x480 off-screen render target — matches ASM screen_x/screen_y */
    RenderTexture2D blur_tex;   /* Small texture for frosted glass letterbox (80x60) */
    BitmapFont      font;       /* Bitmap font for HUD text (from FONTE.ASM) */
    Assets         *assets;     /* Pointer to loaded assets (owned by main()) */

    /* Animation tick counter — mirrors DRAW.ASM:Refresh_Sprites frame counter */
    int             frame;
} DrawContext;

/*
 * draw_init — initialise the draw context.
 * Creates the 640x480 RenderTexture2D canvas.
 * Initialises the bitmap font from assets->font_sheet.
 * Call after InitWindow() and assets_load().
 */
void draw_init(DrawContext *dc, Assets *assets);

/*
 * draw_reinit_textures — recreate canvas and blur render textures.
 * Call after screen rotation or fold/unfold to reset GPU state.
 * The 640x480 canvas size is preserved; only the GL resources are refreshed.
 */
void draw_reinit_textures(DrawContext *dc);

/*
 * draw_shutdown — free the render texture.
 * Call before CloseWindow().
 */
void draw_shutdown(DrawContext *dc);

/*
 * draw_frame_to_canvas — render game world into dc->canvas (640x480).
 * Wraps BeginTextureMode(dc->canvas) ... EndTextureMode().
 * Does NOT write to the window — call draw_canvas_to_screen for that.
 * Overlays that must sit inside the canvas (ready, pause, game over, demo)
 * should be drawn in a separate BeginTextureMode(dc->canvas) block after
 * this call.
 */
void draw_frame_to_canvas(DrawContext *dc, const Game *g);

/*
 * draw_canvas_to_screen — blit dc->canvas to the window at 2x scale.
 * MUST be called inside BeginDrawing()/EndDrawing().
 * Equivalent to DRAW.ASM:Flip_Buffer / _flip_screen.
 */
void draw_canvas_to_screen(DrawContext *dc, Game *g);

/*
 * draw_frame — convenience wrapper: draw_frame_to_canvas + draw_canvas_to_screen.
 * MUST be called between BeginDrawing() and EndDrawing() in the caller.
 * Use the split functions instead when overlays need to be composited into
 * the canvas before the blit.
 */
void draw_frame(DrawContext *dc, Game *g);
