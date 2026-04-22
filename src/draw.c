/*
 * draw.c — BrickBlaster render system
 *
 * Implements the full-frame render pipeline using raylib.
 * All sprite coordinates are derived from DRAW.ASM and Blaster.inc constants,
 * cross-checked against brickblaster-godot/src/core/sprite_atlas.gd.
 *
 * Key ASM references:
 *   DRAW.ASM:368    Refresh_Sprites — animation tick
 *   DRAW.ASM:426    Draw_sprites    — sprite blit loop
 *   DRAW.ASM:831    Flip_Buffer     — copy video_buffer to screen
 *   DRAW.ASM:781    Draw_Shape      — single shape blit (pos/size driven)
 *
 * Sprite sheet layout (SPRITE.png, 640px wide):
 *
 *   BALL SPRITES (9x9)
 *   Blaster.inc:332  ball_blue_o  = 000+(screen_x*000)  → Rect(0,  0, 9, 9)
 *   Blaster.inc:333  ball_orange_o= 000+(screen_x*009)  → Rect(0,  9, 9, 9)
 *   Blaster.inc:337  ghost_blue   = 018+(screen_x*000)  → Rect(18, 0, 9, 9)
 *   Blaster.inc:338  ghost_orange = 018+(screen_x*009)  → Rect(18, 9, 9, 9)
 *
 *   BREAK ANIMATION (5 frames, 9x9 each, NEXT_BALL=11 stride)
 *   Blaster.inc:344  break_ball_blue_o = 046+(screen_x*000) → Rect(46, 0, 9, 9)
 *   Blaster.inc:345  break_ball_orange = 046+(screen_x*009) → Rect(46, 9, 9, 9)
 *
 *   PADDLE (P1 only in single-player; same Y row for each size)
 *   Blaster.inc:225  vaisseau_1_o = 000+(screen_x*042)  → Rect(0,   42, 74, 25)
 *   Blaster.inc:281  vaisseau_large_1_o = 296+(screen_x*042) → Rect(296, 42,105, 25)
 *   Blaster.inc:286  vaisseau_small_1_o = 506+(screen_x*042) → Rect(506, 42, 38, 25)
 *
 *   BRICKS (32x16)
 *   Blaster.inc:342  brique_classic_o = 066+(screen_x*095) → Rect(66, 95, 32, 16)
 *   Blaster.inc:343  brique_multi_o   = 033+(screen_x*095) → Rect(33, 95, 32, 16)
 *   Blaster.inc:346  next_color = screen_x*17 → 17 row stride between colors
 *   Blaster.inc:347  next_brique = 33          → 33 px horizontal per damage state
 *   Color rows: Green=95, Blue=112, Violet=129, Orange=146
 *   Blaster.inc:350  brique_beton_o = 585+(screen_x*029) → Rect(585, 29, 32, 16)
 *   Blaster.inc:352  brique_transp_o = 198+(screen_x*095) → Rect(198, 95, 32, 16)
 *   Blaster.inc:353  brique_teleport_o = 231+(screen_x*095) → Rect(231, 95, 32, 16)
 *
 *   POWERUPS (26x24 each, sprite_atlas.gd:166)
 *   Row 1 base: Rect(1, 752, 26, 24), stride 26 px horizontal (NEXT_OPTION=26)
 *   BONUS/MALUS sprite column swap: BONUS→col 21, MALUS→col 20 (c672fcb fix)
 *
 *   PROJECTILES (mini shoot 12x18, big shoot 13x19)
 *   Blaster.inc:238  vaisseau_tir_left_1  = 374+(screen_x*183) → Rect(374,183,12,18)
 *   Blaster.inc:238  vaisseau_tir_right_1 = 402+(screen_x*183) → Rect(402,183,12,18)
 *   Blaster.inc:248  vaisseau_tir_big_1   = 446+(screen_x*413) → Rect(446,413,13,19)
 *
 *   HUD PANELS
 *   Blaster.inc:204  panel_level_o  = 001+(screen_x*019) → Rect(1,  19, 30, 22)
 *   Blaster.inc:212  panel_score_1_o= 032+(screen_x*019) → Rect(32, 19, 90, 22)
 *   Blaster.inc:219  panel_nbs_ball = 631+(screen_x*000) → Rect(631, 0, 9, 9) blue
 *                                     631+(screen_x*009) → Rect(631, 9, 9, 9) orange
 */

#include "draw.h"
#include "constants.h"
#include "game.h"
#include "ball.h"
#include "paddle.h"
#include "monster.h"
#include "brick.h"
#include "powerup.h"
#include "font.h"
#include "assets.h"

#if defined(BRICKBLASTER_MOBILE)
#include "mobile_controls.h"
#endif

#include "raylib.h"
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Sprite sheet source rectangles (Rectangle = {x, y, w, h})
 * All derived from Blaster.inc and validated against sprite_atlas.gd
 * ============================================================================ */

/* --- Balls --- */
/* Blaster.inc:332  ball_blue_o  = 000+(screen_x*000) */
static const Rectangle SR_BALL_BLUE        = {  0,  0, 9, 9 };
/* Blaster.inc:333  ball_orange_o= 000+(screen_x*009) */
static const Rectangle SR_BALL_ORANGE      = {  0,  9, 9, 9 };
/* Blaster.inc:337  ghost_ball_blue_o = 018+(screen_x*000) */
static const Rectangle SR_GHOST_BLUE       = { 18,  0, 9, 9 };
/* Blaster.inc:338  ghost_ball_orange_o = 018+(screen_x*009) */
static const Rectangle SR_GHOST_ORANGE     = { 18,  9, 9, 9 };

/* --- Paddle (Player 1 in single-player mode) --- */
/* Blaster.inc:225  vaisseau_1_o = 000+(screen_x*042) */
static const Rectangle SR_PADDLE_NORMAL    = {   0, 42, 74, 25 };
/* Blaster.inc:281  vaisseau_large_size_x=105, y same row 42 */
static const Rectangle SR_PADDLE_LARGE     = { 296, 42,105, 25 };
/* Blaster.inc:286  vaisseau_small_size_x=38, y same row 42 */
static const Rectangle SR_PADDLE_SMALL     = { 506, 42, 38, 25 };

/* --- Paddle Player 2 (P1-ASM-28) ---
 * Blaster.inc:226  vaisseau_2_o = 000+(screen_x*068) → Rect(0, 68, 74, 25) */
static const Rectangle SR_PADDLE_P2_NORMAL = {   0, 68, 74, 25 };
/* Blaster.inc:282  vaisseau_large_2_o (row 68 on P2 side) */
static const Rectangle SR_PADDLE_P2_LARGE  = { 296, 68,105, 25 };
/* Blaster.inc:287  vaisseau_small_2_o */
static const Rectangle SR_PADDLE_P2_SMALL  = { 506, 68, 38, 25 };

/* --- Bricks --- */
/* Blaster.inc:342  brique_classic_o = 066+(screen_x*095) */
#define BRICK_BASE_X   66
#define BRICK_BASE_Y   95
/* Blaster.inc:346  next_color = screen_x*17 → 17 rows between colors */
#define BRICK_COLOR_ROW_STRIDE  17
/* Blaster.inc:347  next_brique = 33 (horizontal stride per damage state) */
#define BRICK_DAMAGE_STRIDE     33

/* Blaster.inc:350  brique_beton_o = 585+(screen_x*029) */
static const Rectangle SR_BRICK_INDESTR    = { 585, 29, 32, 16 };
/* Indestructible flash frame: sprite_atlas.gd:148  BRIQUE_REFLET = Rect2(585, 97, 32, 16) */
static const Rectangle SR_BRICK_REFLET     = { 585, 97, 32, 16 };

/* Blaster.inc:352  brique_transp_o  = 198+(screen_x*095) */
#define BRICK_TRANSP_BASE_X  198
/* Blaster.inc:353  brique_teleport_o= 231+(screen_x*095) */
#define BRICK_TELE_BASE_X    231
/* Multi-hit base: Blaster.inc:343  brique_multi_o = 033+(screen_x*095) */
#define BRICK_MULTI_BASE_X   33

/* --- Powerups (26x24) --- */
/* sprite_atlas.gd:166  OPTION_1 = Rect2(1, 752, 26, 24) */
#define POWERUP_BASE_X    1
#define POWERUP_BASE_Y  752
/* NEXT_OPTION = 26  (Blaster.inc:142) */
#define POWERUP_STRIDE   26

/* Y-offset per powerup: 0 = brown row (Y=752), 52 = red row (Y=804).
 * Ball powerups use the red row so they visually stand out. */
static const int POWERUP_SPRITE_Y_OFF[POWERUP_COUNT] = {
    52, 52, 52, 52,  /* BALL_3/6/9/20 → red row */
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,
};

/* Sprite column for each PowerupType (0-based).
 * BONUS/MALUS are swapped: BONUS enum=20 → sprite col 21, MALUS enum=21 → col 20.
 * Verified in commit c672fcb and sprite_atlas.gd:173-201 */
static const int POWERUP_SPRITE_COL[POWERUP_COUNT] = {
     0,  /* POWERUP_BALL_3     */
     1,  /* POWERUP_BALL_6     */
     2,  /* POWERUP_BALL_9     */
     3,  /* POWERUP_BALL_20    */
     4,  /* POWERUP_DEATH      */
     5,  /* POWERUP_NEW_LIFE   */
     6,  /* POWERUP_SHOOT      */
     7,  /* POWERUP_SLOW_BALL  */
     8,  /* POWERUP_FAST_BALL  */
     9,  /* POWERUP_IRON_BALL  */
    10,  /* POWERUP_TELEPOD    */
    11,  /* POWERUP_NIGHT      */
    12,  /* POWERUP_SMALL_SHIP */
    13,  /* POWERUP_LARGE_SHIP */
    14,  /* POWERUP_REVERSE    */
    15,  /* POWERUP_NEXT_LEVEL */
    16,  /* POWERUP_DEL_MONSTER*/
    17,  /* POWERUP_MAGNETIC   */
    18,  /* POWERUP_ADD_MONSTER*/
    19,  /* POWERUP_GHOST      */
    21,  /* POWERUP_BONUS (enum=20) → sprite col 21   c672fcb */
    20,  /* POWERUP_MALUS (enum=21) → sprite col 20   c672fcb */
    22,  /* POWERUP_MINI_SHOOT */
    23,  /* POWERUP_COLLISION  */
};

/* Y-flip flag per PowerupType: 1 = sprite stored upside-down in the PNG.
 * Indices 3 (BALL_20), 6 (SHOOT), 20 (BONUS) appear reversed in-game. */
/* Y-flip: sprite stored upside-down in sheet (negative src.height) */
static const int POWERUP_SPRITE_FLIP[POWERUP_COUNT] = {
    1, 1, 0, 0,  /* 0=BALL_3 flipped, 1=BALL_6 flipped, 3=BALL_20 correct */
    0, 0, 0, 0,  /* 6=SHOOT X-flip only (see below) */
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    1, 0, 0, 0,  /* 20=BONUS flipped */
};
/* X-flip: sprite stored mirrored in sheet (negative src.width) */
static const int POWERUP_SPRITE_X_FLIP[POWERUP_COUNT] = {
    0, 0, 0, 0,
    0, 0, 1, 0,  /* 6=SHOOT x-flipped */
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
};

/* --- Projectiles in flight ---
 * MAIN.ASM:1947-1990 Detect_Shoot — a fired projectile copies its sprite
 * data from `shoot_o` (big) or `mini_shoot_o` (mini), NOT from the
 * `vaisseau_tir*` family. The vaisseau_tir / vaisseau_tir_big rectangles
 * are the PADDLE CANNON RECOIL ANIMATION (6 and 4 frames respectively),
 * which the port does not replay — they belong to the paddle render,
 * not to each flying projectile.
 *
 * Blaster.inc:107-112  shoot_o = 444+(screen_x*339) → (444,339,8,42),
 *                      11-frame anim (shoot_nbs_anim, stride shoot_next=3).
 *                      We use frame 0 only.
 * Blaster.inc:114-116  mini_shoot_o = 85+(screen_x*164) → (85,164,3,13),
 *                      single frame. */
static const Rectangle SR_PROJ_MINI       = {  85, 164,  3, 13 };
static const Rectangle SR_PROJ_BIG        = { 444, 339,  8, 42 };

/* --- HUD panels --- */
/* Blaster.inc:204  panel_level_o = 001+(screen_x*019) */
static const Rectangle SR_PANEL_LEVEL     = {  1, 19, 30, 22 };
/* Blaster.inc:212  panel_score_1_o = 032+(screen_x*019) */
static const Rectangle SR_PANEL_SCORE     = { 32, 19, 90, 22 };
/* F5 P1-ASM-41b: P2 score panel background (dual mode only)
 * MAIN.ASM:5992-6012 init_panel draws Panel_Score_2 with sprite
 * panel_score_2_o=(444, 384) 90x22. */
static const Rectangle SR_PANEL_SCORE_P2  = { 444, 384, 90, 22 };
/* F5 P1-ASM-42: dedicated life indicator sprites.
 * Blaster.inc:217-218 panel_nbs_ball_1/2_o points to the 9px-wide ZERO-
 * life anchor slot at X=631. The ASM render (MAIN.ASM:6056-6072) starts
 * there empty and for each life grows sprite_size_x by +12 and shifts
 * sprite_current_adrs by -12, pulling ball graphics out of the atlas
 * LEFT of 631 (where actual balls are stored at 12px stride). So X=631
 * itself is transparent — we must source one cell to the left.
 *
 * Rightmost real ball is at (619, 0, 9, 9); further balls stack left
 * with stride 12. The port draws N independent 9x9 sprites at screen
 * positions PANEL_NBS_BALL_POS_X + i*NEXT_BALL, so a single-cell source
 * is enough. */
static const Rectangle SR_HUD_LIFE_P1     = { 619, 0, 9, 9 };
static const Rectangle SR_HUD_LIFE_P2     = { 619, 9, 9, 9 };
/* F5 P1-ASM-41c: option background + info banner — TODO.
 * MAIN.ASM:5859-5940 init_panel draws:
 *   panel_option_o = (  1, 827) 26x24  (Blaster.inc:140  option_off)
 *   panel_info_o   = (311,1816) 270x22 (Blaster.inc:196)
 * Verified empty in current assets/sprites/SPRITE.png (640x1900 — content at
 * Y=824 ends abruptly at Y=825 and Y=1816 region is all transparent 00000000).
 * The shipped atlas does not include these HUD panel backgrounds; leave the
 * overlay text un-backgrounded rather than invent sprite cells. Re-enable
 * when a future atlas regeneration ports these ASM-source tiles. */


/* ============================================================================
 * Helper: get brick source rectangle
 * DRAW.ASM:Draw_sprites uses sprite_current_adrs computed by:
 *   base + color_offset + damage_offset
 * where:
 *   color_offset = color_index * (screen_x * next_color_rows)
 *   damage_offset = damage_level * next_brique
 * Blaster.inc:346  next_color   = screen_x*17  (17-row stride)
 * Blaster.inc:347  next_brique  = 33            (33-px horizontal stride)
 * ============================================================================ */
static Rectangle get_brick_rect(const Brick *b) {
    int base_x, base_y;

    switch (b->type) {
    case BRICK_INDESTRUCTIBLE:
        /* Blaster.inc:350  brique_beton_o */
        return SR_BRICK_INDESTR;

    case BRICK_TRANSPARENT:
        /* Blaster.inc:352  brique_transp_o */
        base_x = BRICK_TRANSP_BASE_X;
        base_y = BRICK_BASE_Y;
        break;

    case BRICK_TELEPORTER:
        /* Blaster.inc:353  brique_teleport_o */
        base_x = BRICK_TELE_BASE_X;
        base_y = BRICK_BASE_Y;
        break;

    default:
        /* BRICK_NORMAL: classic (HP=1) vs multi-hit (HP>1) use different base sprites.
         * Blaster.inc:342  brique_classic_o = 066+(screen_x*095) — single-hit bricks
         * Blaster.inc:343  brique_multi_o   = 033+(screen_x*095) — multi-hit bricks */
        {
            int orig_hp = b->raw & RESISTANCE_DE_BRIQUE;
            base_x = (orig_hp > 1) ? BRICK_MULTI_BASE_X : BRICK_BASE_X;
        }
        base_y = BRICK_BASE_Y;
        break;
    }

    /* Apply color offset (vertical): each color is 17 rows apart.
     * Blaster.inc:345  next_color = screen_x*17
     * Color 0=Green (row 95), 1=Blue (+17=112), 2=Violet (+34=129), 3=Orange (+51=146) */
    int color = b->color & 3;   /* mask to 2 bits (bits 7-6 decoded to 0-3) */
    base_y += color * BRICK_COLOR_ROW_STRIDE;

    /* Apply damage offset (horizontal): each damage state is 33 px apart.
     * Blaster.inc:346  next_brique = 33
     * damage_state = clamped HP reduction (0=fresh, 4=almost dead) */
    int damage_state = 0;
    if (b->type == BRICK_NORMAL) {
        /* Original brick HP in raw byte (bits 4-0). Damage state = original_hp - current_hp.
         * We clamp to 0..4 (5 states maximum). */
        int orig_hp = b->raw & RESISTANCE_DE_BRIQUE;
        int cur_hp  = b->hp;
        damage_state = orig_hp - cur_hp;
        if (damage_state < 0) damage_state = 0;
        if (damage_state > 4) damage_state = 4;
    }
    base_x += damage_state * BRICK_DAMAGE_STRIDE;

    Rectangle r = { (float)base_x, (float)base_y, 32, 16 };
    return r;
}


/* ============================================================================
 * Helper: get powerup source rectangle
 * MAIN.ASM struc_options table row for each powerup defines sprite offset.
 * sprite_atlas.gd:166  OPTION_1 = Rect2(1, 752, 26, 24), NEXT_OPTION=26
 *
 * F5 P1-ASM-35: in dual mode, P2-owned options use the +25 Y row in the atlas
 * (option_2_decal). Blaster.inc:145  option_2_decal = screen_x*777 - option_1_o
 *                                                 = screen_x*(777-752) = +25 rows.
 * MAIN.ASM:5525-5533 adds option_2_decal to sprite_adrs for P2 options only.
 * ============================================================================ */
static Rectangle get_powerup_rect(PowerupType type, int p2_owned_dual) {
    int idx = (int)type;
    if (idx < 0 || idx >= POWERUP_COUNT) idx = 0;
    int col = POWERUP_SPRITE_COL[idx];
    int y_decal = p2_owned_dual ? 25 : 0;  /* Blaster.inc:145  option_2_decal */
    Rectangle r = {
        (float)(POWERUP_BASE_X + col * POWERUP_STRIDE),
        (float)(POWERUP_BASE_Y + POWERUP_SPRITE_Y_OFF[idx] + y_decal),
        (float)OPTION_W,
        (float)OPTION_H
    };
    return r;
}


/* ============================================================================
 * Helper: get paddle source rectangle
 * DRAW.ASM:Draw_sprites uses sprite_current_adrs set during init:
 *   vaisseau_1_o / vaisseau_large_1_o / vaisseau_small_1_o for P1
 *   vaisseau_2_o / vaisseau_large_2_o / vaisseau_small_2_o for P2 (row Y=68)
 * P1-ASM-28: use true P2 sprite row instead of tinted P1 sprite.
 * ============================================================================ */
static Rectangle get_paddle_rect(const Paddle *p, int is_p2) {
    if (is_p2) {
        switch (p->size) {
        case PADDLE_SIZE_SMALL:  return SR_PADDLE_P2_SMALL;
        case PADDLE_SIZE_LARGE:  return SR_PADDLE_P2_LARGE;
        default:                 return SR_PADDLE_P2_NORMAL;
        }
    }
    switch (p->size) {
    case PADDLE_SIZE_SMALL:  return SR_PADDLE_SMALL;
    case PADDLE_SIZE_LARGE:  return SR_PADDLE_LARGE;
    default:                 return SR_PADDLE_NORMAL;
    }
}


/* ============================================================================
 * draw_init
 * ============================================================================ */
void draw_init(DrawContext *dc, Assets *assets) {
    /* Create 640x480 render target — mirrors DRAW.ASM screen_x / screen_y.
     * DRAW.ASM:10  mov ax,Mode640x480x256 → screen resolution = 640x480 */
    dc->canvas = LoadRenderTexture(SCREEN_W, SCREEN_H);
    dc->blur_tex = LoadRenderTexture(SCREEN_W / 8, SCREEN_H / 8);  /* 80x60 for frosted glass */
    SetTextureFilter(dc->blur_tex.texture, TEXTURE_FILTER_BILINEAR);

    /* Smooth scaling for the 2x window upscale */
    SetTextureFilter(dc->canvas.texture, TEXTURE_FILTER_POINT);

    /* Bitmap font from FONTE.ASM */
    font_init(&dc->font, &assets->font_sheet);

    dc->assets = assets;
    dc->frame  = 0;
}


/* ============================================================================
 * draw_reinit_textures — recreate render textures after screen change.
 * Fixes GPU state after rotation / fold / unfold on Android.
 * ============================================================================ */
void draw_reinit_textures(DrawContext *dc) {
    UnloadRenderTexture(dc->canvas);
    UnloadRenderTexture(dc->blur_tex);
    dc->canvas   = LoadRenderTexture(SCREEN_W, SCREEN_H);
    dc->blur_tex = LoadRenderTexture(SCREEN_W / 8, SCREEN_H / 8);
    SetTextureFilter(dc->canvas.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(dc->blur_tex.texture, TEXTURE_FILTER_BILINEAR);
}

/* ============================================================================
 * draw_shutdown
 * ============================================================================ */
void draw_shutdown(DrawContext *dc) {
    UnloadRenderTexture(dc->canvas);
    UnloadRenderTexture(dc->blur_tex);
}


/* ============================================================================
 * draw_background
 * Select background texture by level number.
 * 8 backgrounds per set (set 0 = world 0, set 1 = world 1).
 * Background selection: (level_num - 1) mod 8  → index 0..7.
 * MAIN.ASM:1004  call load_decor → FILE.ASM background selection logic.
 * ============================================================================ */
static void draw_background(DrawContext *dc, const Game *g) {
    int set   = g->world & 1;           /* 0 or 1 */
    int index = (g->level_num - 1) % ASSETS_BG_COUNT;  /* 0..7 */
    const Texture2D *bg = assets_get_background(dc->assets, set, index);

    if (bg && dc->assets->backgrounds_loaded[set][index]) {
        /* Draw full-screen background (640x480).
         * DRAW.ASM:Draw_Buffer — copies true_background_buffer to video_buffer */
        DrawTexture(*bg, 0, 0, WHITE);
    } else {
        /* Fallback: solid black (matches night mode or missing asset) */
        ClearBackground(BLACK);
    }

    /* Mask side panels — Arcade world (set 1) backgrounds have bright
     * red side panels in the source art.  Space (set 0) panels are textured but
     * still distracting; both worlds mask to keep focus on the play field.
     * DRAW.ASM side-panel logic: panels outside PLAY_X1..PLAY_X2 are overdrawn.
     *
     * Instead of solid black, use a dark semi-transparent overlay with a gradient
     * fade at the inner edge for a polished look on mobile. */
    {
        Color panel_dark = (Color){8, 8, 16, 220};
        Color panel_edge = (Color){8, 8, 16, 120};
        int grad_w = 8;  /* width of gradient fade strip */

        /* Left panel: solid dark + gradient fade at right edge */
        DrawRectangle(0, 0, PLAY_X1 - grad_w, SCREEN_H, panel_dark);
        DrawRectangleGradientH(PLAY_X1 - grad_w, 0, grad_w, SCREEN_H,
                               panel_dark, panel_edge);

        /* Right panel: gradient fade at left edge + solid dark */
        DrawRectangleGradientH(PLAY_X2, 0, grad_w, SCREEN_H,
                               panel_edge, panel_dark);
        DrawRectangle(PLAY_X2 + grad_w, 0, SCREEN_W - PLAY_X2 - grad_w, SCREEN_H, panel_dark);
    }
}


/* ============================================================================
 * draw_bricks
 * DRAW.ASM:Draw_sprites — for each sprite with sprite_mode matching brique:
 *   position = (row*BRICK_H, col*BRICK_W) + origin
 *   DrawTextureRec(sprite_sheet, src_rect, pos, WHITE)
 * Indestructible bricks get a flash effect every NBS_REFLET frames.
 * Blaster.inc:350  nbs_reflet = 5  (Refresh_Sprites cycles the beton anim)
 * ============================================================================ */
static void draw_bricks(DrawContext *dc, const Game *g) {
    if (!dc->assets->sprite_sheet_loaded) return;

    for (int i = 0; i < BRICK_COUNT; i++) {
        const Brick *b = &g->bricks[i];
        if (!b->active || b->hp == 0) continue;

        Rectangle src = get_brick_rect(b);

        /* Indestructible brick reflet (hit-triggered ripple).
         * MAIN.ASM:4058-4061 stamps brique_reflet_o+next_reflet*nbs_reflet
         * on ball impact; MAIN.ASM:6220-6240 decrements the sprite offset
         * each frame back to brique_beton_o.  We collapse the 5 reflet
         * sub-frames into a single reflet_timer tracked per brick — when
         * > 0 the reflet sprite is shown, else the base beton.  Replaces
         * the earlier ambient whole-screen flicker. */
        if (b->type == BRICK_INDESTRUCTIBLE && b->reflet_timer > 0) {
            src = SR_BRICK_REFLET;
        }

        Vector2 pos = { (float)b->x, (float)b->y };
        DrawTextureRec(dc->assets->sprite_sheet, src, pos, WHITE);
    }
}


/* ============================================================================
 * draw_powerups
 * DRAW.ASM:Draw_sprites — sprite_mode = option → draw 26x24 sprite.
 * MAIN.ASM:Refresh_Options sets sprite_pos_x/y each frame as they fall.
 * ============================================================================ */
static void draw_powerups(DrawContext *dc, const Game *g) {
    if (!dc->assets->sprite_sheet_loaded) return;

    for (int i = 0; i < g->powerup_count; i++) {
        const Powerup *p = &g->powerups[i];
        if (!p->active) continue;

        /* F5 P1-ASM-35: P2 duel options draw from the +25-row atlas tile. */
        int p2_owned_dual = (g->game_mode == 2 && p->owner == 1);
        Rectangle src = get_powerup_rect(p->type, p2_owned_dual);
        /* Y-flip: some sprites are stored upside-down in the sheet. */
        {
            int pidx = (int)p->type;
            if (pidx >= 0 && pidx < POWERUP_COUNT) {
                if (POWERUP_SPRITE_FLIP[pidx]) {
                    src.y += src.height;
                    src.height = -src.height;
                }
                if (POWERUP_SPRITE_X_FLIP[pidx]) {
                    src.x += src.width;
                    src.width = -src.width;
                }
            }
        }
        /* Wear OS: draw 2x centred (small watch screen needs larger touch target).
         * PC / Web / mobile tablet: draw at native 1x size (top-left at p->x, p->y). */
#if defined(PLATFORM_ANDROID) && !defined(BRICKBLASTER_MOBILE)
        {
            float ox = (float)p->x - OPTION_W / 2.0f;
            float oy = (float)p->y - OPTION_H / 2.0f;
            Rectangle dst = { ox, oy, OPTION_W * 2.0f, OPTION_H * 2.0f };
            DrawTexturePro(dc->assets->sprite_sheet, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        }
#else
        DrawTexturePro(dc->assets->sprite_sheet, src,
                       (Rectangle){ (float)p->x, (float)p->y, (float)OPTION_W, (float)OPTION_H },
                       (Vector2){0, 0}, 0.0f, WHITE);
#endif
    }
}


/* ============================================================================
 * draw_balls
 * DRAW.ASM:Draw_sprites — sprite_mode = ball → draw 9x9 sprite.
 * Ball sprite selection:
 *   P1 orange:     Rect(0, 9, 9, 9)    — Blaster.inc:333  ball_orange_o
 *   P2 blue:       Rect(0, 0, 9, 9)    — Blaster.inc:332  ball_blue_o (dual mode only)
 *   Ghost orange:  Rect(18, 9, 9, 9)   — Blaster.inc:338  ghost_ball_orange_o
 *   Ghost blue:    Rect(18, 0, 9, 9)   — Blaster.inc:337  ghost_ball_blue_o
 *   Iron ball:     same sprite as normal (no separate sprite in original ASM)
 * In single-player and coop, all balls are orange.
 * Blue balls are used by player 2 in dual (vs) mode only.
 * DRAW.ASM:Draw_sprites pixel blit skips color-index 0 (transparent mask).
 * ============================================================================ */
static void draw_balls(DrawContext *dc, const Game *g) {
    if (!dc->assets->sprite_sheet_loaded) return;

    /* game_mode: 0=1P solo, 1=2P coop, 2=2P duel.
     * Ball index 0 = P1 (orange); ball index 1 = P2 (blue in duel mode only). */
    for (int i = 0; i < g->ball_count; i++) {
        const Ball *b = &g->balls[i];
        if (!b->active) continue;

        /* Select sprite row.  Default (no iron):
         *   P1             → ball_orange_o (Blaster.inc:333)
         *   P2 in dual     → ball_blue_o   (Blaster.inc:332)
         *
         * Iron ball (option_iron_ball active): MAIN.ASM:2844-2849 sets
         * sprite_current_adrs to `ball_blue_o` for every ball on every
         * frame, then at MAIN.ASM:2855-2862 adds `+9` to the address if
         * `dual_flag && player == player_2`.  Since ball_blue_o is at
         * offset 0 and ball_orange_o at offset 9, this effectively
         * INVERTS the dual-mode colour coding while iron is active:
         *   1P / 2P coop  → both balls blue
         *   2P dual       → P1 becomes blue, P2 becomes orange
         *
         * Earlier versions of this port missed the per-frame sprite
         * update (looked only at `option_iron_ball_p`, which is a `ret`
         * — the sprite swap is in the ball update loop, not the pickup
         * handler). Reported upstream by david4599 (2026-04-21):
         * "iron ball qui reste rouge". */
        int is_p2_dual = (g->game_mode == 2 && i >= 1);
        Rectangle src;
        if (b->is_ghost) {
            /* Ghost/bubble ball: use ghost sprite (bubble-like appearance).
             * Blaster.inc:337-338  ghost_ball_blue/orange_o
             * MAIN.ASM:3352  sprite_adrs += 18 (offset to ghost variant).
             * Iron overlay: MAIN.ASM:2851-2853 mirrors the regular-ball
             * swap — ghost_ball_blue_o, +9 for P2 in dual. */
            if (b->is_iron) {
                src = is_p2_dual ? SR_GHOST_ORANGE : SR_GHOST_BLUE;
            } else {
                src = is_p2_dual ? SR_GHOST_BLUE : SR_GHOST_ORANGE;
            }
        } else {
            if (b->is_iron) {
                src = is_p2_dual ? SR_BALL_ORANGE : SR_BALL_BLUE;
            } else {
                src = is_p2_dual ? SR_BALL_BLUE : SR_BALL_ORANGE;
            }
        }

        Vector2 pos = { (float)b->x, (float)b->y };
        DrawTextureRec(dc->assets->sprite_sheet, src, pos, WHITE);
    }
}


/* ============================================================================
 * draw_paddle
 * DRAW.ASM:Draw_sprites — sprite_mode = vaisseau → draw paddle sprite.
 * Paddle size selects from three sprite variants at same Y row (42).
 * Blaster.inc:225  vaisseau_1_o = 000+(screen_x*042) → Rect(0, 42, 74, 25)
 * ============================================================================ */
static void draw_paddle(DrawContext *dc, const Game *g) {
    if (!dc->assets->sprite_sheet_loaded) return;

    Rectangle src = get_paddle_rect(&g->paddle, 0);
    Vector2 pos   = { (float)g->paddle.x, (float)g->paddle.y };
    DrawTextureRec(dc->assets->sprite_sheet, src, pos, WHITE);

    /* P1-ASM-28: in MP, P2 uses the true P2 paddle sprite row (Y=68).
     * No tint — the sprite itself is colour-coded per ASM (Blaster.inc:226). */
    if (g->game_mode > 0) {
        Rectangle src2 = get_paddle_rect(&g->paddle_2, 1);
        Vector2 pos2   = { (float)g->paddle_2.x, (float)g->paddle_2.y };
        DrawTextureRec(dc->assets->sprite_sheet, src2, pos2, WHITE);
    }
}


/* ============================================================================
 * draw_projectiles
 * DRAW.ASM:Draw_sprites — sprite_mode = tir → draw projectile sprite.
 * Mini shoot: 3x13 at (85,164) — `mini_shoot_o` in Blaster.inc:114-116.
 * Big shoot:  8x42 at (444,339) — `shoot_o` in Blaster.inc:107-112.
 * Both come from MAIN.ASM:1947-1990 Detect_Shoot, which seeds the fired
 * sprite's sprite_adrs from shoot_o / mini_shoot_o (NOT vaisseau_tir*).
 * ============================================================================ */
static void draw_projectiles(DrawContext *dc, const Game *g) {
    if (!dc->assets->sprite_sheet_loaded) return;

    for (int i = 0; i < g->proj_count; i++) {
        const Projectile *p = &g->projectiles[i];
        if (!p->active) continue;

        /* Projectile sprites are shared across players — MAIN.ASM tags
         * sprite_player on the fired entity for scoring attribution, but
         * visually both players shoot the same `shoot` / `mini_shoot`
         * graphic.  Center the narrower sprite on the paddle-relative
         * spawn point (game.c places p->x with vaisseau_tir's 12px width
         * in mind; the real projectile is 3 or 8 wide). */
        Rectangle src = p->is_big ? SR_PROJ_BIG : SR_PROJ_MINI;
        float pad_w   = p->is_big ? 13.0f : 12.0f;   /* cannon sprite width */
        float offset  = (pad_w - src.width) * 0.5f;
        Vector2 pos   = { (float)p->x + offset, (float)p->y };
        DrawTextureRec(dc->assets->sprite_sheet, src, pos, WHITE);
    }
}


/* ============================================================================
 * draw_hud
 * Draws: level panel, score panel, life ball indicators, active powerup text.
 *
 * MAIN.ASM frame loop step 16: refresh_sprites draws panel sprites.
 * Panel sprite_mode = panel in DRAW.ASM:Draw_sprites.
 *
 * Level panel:  Blaster.inc:204  panel_level_size_x=30, panel_level_pos_x=123
 * Score panel:  Blaster.inc:212  panel_score_size_x=90, panel_score_pos_x=426
 * Ball lives:   Blaster.inc:219  panel_nbs_ball_size_x=9, panel_nbs_ball_pos_x=518
 *               Blaster.inc:221  panel_nbs_ball_pos_y_1=458 (P1 indicator)
 * Font (FONTE.ASM):
 *   Number digits drawn via font_draw_string after panel background sprite.
 *   Blaster.inc:206  panel_level_pos_x=123, panel_level_pos_y=9
 *   Blaster.inc:214  panel_score_pos_x=426, panel_score_pos_y=9
 * ============================================================================ */
static void draw_hud(DrawContext *dc, const Game *g) {
    /* F5 P1-ASM-41a: in dual mode, Panel_Level shifts by +150+32 = +182.
     * MAIN.ASM:6020-6021  add [edx.sprite_pos_x],150+32
     * So X=123 → X=305 in dual (game_mode == 2). */
    int is_duel      = (g->game_mode == 2);
    int lvl_panel_x  = is_duel ? (PANEL_LEVEL_POS_X + 150 + 32) : PANEL_LEVEL_POS_X;

    /* --- Level panel background sprite (if sheet loaded) --- */
    if (dc->assets->sprite_sheet_loaded) {
        Vector2 lvl_pos   = { (float)lvl_panel_x,  PANEL_LEVEL_POS_Y  };
        Vector2 score_pos = { PANEL_SCORE_POS_X,   PANEL_SCORE_POS_Y  };
        DrawTextureRec(dc->assets->sprite_sheet, SR_PANEL_LEVEL, lvl_pos,   WHITE);
        DrawTextureRec(dc->assets->sprite_sheet, SR_PANEL_SCORE, score_pos, WHITE);

        /* F5 P1-ASM-41b: P2 score panel drawn only in dual.
         * MAIN.ASM:5992-6012 Panel_Score_2 at (panel_level_pos_x=123,
         * panel_score_pos_y=9) with sprite panel_score_2_o=(444, 384) 90x22. */
        if (is_duel) {
            Vector2 p2_score_pos = { PANEL_LEVEL_POS_X, PANEL_SCORE_POS_Y };
            DrawTextureRec(dc->assets->sprite_sheet, SR_PANEL_SCORE_P2,
                           p2_score_pos, WHITE);
        }
    }

    /* --- Level number text on panel ---
     * FONTE.ASM: font renders at panel_level_pos_x + small offset to centre.
     * We centre the 2-digit level number within the 30px panel. */
    if (dc->assets->font_sheet_loaded) {
        char buf[8];
        /* Level text: Blaster.inc:206  panel_level_pos_x=123 + centre offset
         * F5 P1-ASM-41a: +182 in duel mode. */
        snprintf(buf, sizeof(buf), "%d", g->level_num);
        int lw = font_string_width(&dc->font, buf);
        int lx = lvl_panel_x + (PANEL_LEVEL_W - lw) / 2;
        int ly = PANEL_LEVEL_POS_Y + (PANEL_LEVEL_H - FONT_CHAR_H) / 2;
        font_draw_string(&dc->font, buf, lx, ly, WHITE);

        /* Score text: Blaster.inc:214  panel_score_pos_x=426 */
        snprintf(buf, sizeof(buf), "%d", g->score);
        int sw = font_string_width(&dc->font, buf);
        int sx = PANEL_SCORE_POS_X + (PANEL_SCORE_W - sw) / 2;
        int sy = PANEL_SCORE_POS_Y + (PANEL_SCORE_H - FONT_CHAR_H) / 2;
        font_draw_string(&dc->font, buf, sx, sy, WHITE);

        /* Multiplayer: P2 score.
         * F5 P1-ASM-41b: ASM MAIN.ASM:6011 mov [edx.sprite_pos_x],panel_level_pos_x
         * → P2 score panel sits at X=123 in dual. Text drawn with WHITE
         *   (no ASM tint — pink was a non-ASM invention). */
        if (g->game_mode > 0) {
            snprintf(buf, sizeof(buf), "%d", g->score_2);
            int sw2 = font_string_width(&dc->font, buf);
            int p2_base_x = (g->game_mode == 2) ? PANEL_LEVEL_POS_X : 4;
            int sx2 = p2_base_x + (PANEL_SCORE_W - sw2) / 2;
            font_draw_string(&dc->font, buf, sx2, sy, WHITE);
        }
    }

    /* --- Life ball indicators ---
     * Blaster.inc:219  panel_nbs_ball_pos_x=518, panel_nbs_ball_pos_y_1=458
     * Draw one ball sprite per remaining life, spaced 11px apart (NEXT_BALL=11).
     * DRAW.ASM:Draw_sprites draws panel_nbs_ball_1 as sprite_mode=panel. */
    if (dc->assets->sprite_sheet_loaded) {
        /* lives field = player_nbs_ball = starts at 2 (NBS_BALL_START).
         * Display lives+1 as "balls remaining" (the one in play is lives+1).
         * MAIN.ASM:4595  detect_game_over: dec player_nbs_ball → game over when < 0
         * So actual visible count = lives + 1 (current ball counts). */
        int display_lives = g->lives;
        if (display_lives < 0) display_lives = 0;
        if (display_lives > 9) display_lives = 9;   /* cap for display */

        /* F5 P1-ASM-42: dedicated life-indicator sprites at X=631 in atlas.
         * Blaster.inc:217-218 panel_nbs_ball_1_o/_2_o — NOT the in-play ball
         * sprites (which live at X=0). */
        for (int i = 0; i < display_lives; i++) {
            Vector2 pos = {
                (float)(PANEL_NBS_BALL_POS_X + i * NEXT_BALL),
                (float)PANEL_NBS_BALL_POS_Y1
            };
            DrawTextureRec(dc->assets->sprite_sheet, SR_HUD_LIFE_P1, pos, WHITE);
        }

        /* Multiplayer: P2 lives row below P1 (panel_nbs_ball_pos_y_2=469). */
        if (g->game_mode > 0) {
            int dl2 = g->lives_2;
            if (dl2 < 0) dl2 = 0;
            if (dl2 > 9) dl2 = 9;
            for (int i = 0; i < dl2; i++) {
                Vector2 pos = {
                    (float)(PANEL_NBS_BALL_POS_X + i * NEXT_BALL),
                    (float)PANEL_NBS_BALL_POS_Y2
                };
                DrawTextureRec(dc->assets->sprite_sheet, SR_HUD_LIFE_P2, pos, WHITE);
            }
        }
    }
}


/* ============================================================================
 * draw_frame — main render entry point
 *
 * Render order matches MAIN.ASM frame loop (MAIN.ASM:1061-1086) and
 * DRAW.ASM:Draw_sprites sprite priority list:
 *   1. Background (drawn to background_buffer then copied to video_buffer)
 *   2. Bricks     (static shapes, drawn each frame)
 *   3. Options    (falling powerups, sprite_mode=option)
 *   4. Balls      (sprite_mode=ball — drawn before paddle in ASM z-order)
 *   5. Paddle     (sprite_mode=vaisseau — on top of field)
 *   6. Projectiles(sprite_mode=tir)
 *   7. HUD panels (sprite_mode=panel — always on top)
 *   8. Scale canvas to window (DRAW.ASM:Flip_Buffer equivalent)
 * ============================================================================ */
void draw_frame_to_canvas(DrawContext *dc, const Game *g) {
    dc->frame = g->frame;

    /* --- Off-screen render pass (640x480 canvas) --- */
    BeginTextureMode(dc->canvas);

    /* Step 1: Background
     * DRAW.ASM:_Erase_Shape copies true_background_buffer → video_buffer each frame.
     * We redraw the background texture each frame. */
    draw_background(dc, g);

    /* Step 2: Bricks
     * DRAW.ASM:Draw_sprites iterates Begin_Sprites..End_Sprites.
     * Bricks are drawn from the background pass in the ASM; here we draw them
     * as sprites above the background for correctness. */
    draw_bricks(dc, g);

    /* Step 3: Falling powerups (options)
     * DRAW.ASM:Draw_sprites sprite_mode=option */
    draw_powerups(dc, g);

    /* Step 3b: Brick break animations
     * DRAW.ASM:Refresh_Sprites — break_ball sprites cycle after brick destruction.
     * Blaster.inc:344  break_ball_blue_o = 046+(screen_x*000) → Rect(46, 0, 9, 9)
     * 5 frames at NEXT_BALL=11 px stride: x = 46 + frame*11 */
    if (dc->assets->sprite_sheet_loaded) {
        for (int ba = 0; ba < MAX_BREAK_ANIMS; ba++) {
            if (!g->break_anims[ba].active) continue;
            int frame = g->break_anims[ba].frame;
            if (frame < 0 || frame >= BREAK_NBS_ANIM) continue;
            Rectangle src = { (float)(46 + frame * NEXT_BALL), 9.0f, 9.0f, 9.0f };
            Vector2 pos = { (float)g->break_anims[ba].x, (float)g->break_anims[ba].y };
            DrawTextureRec(dc->assets->sprite_sheet, src, pos, WHITE);
        }
    }

    /* Step 3c: Monsters
     * DRAW.ASM:Draw_sprites sprite_mode=monster
     * Blaster.inc:120  monster_o = 001+(930*001) → starts at pixel (1,1)
     * Blaster.inc:123  monster_next = 930*(3+32) = 930*35 → 35 rows per variant
     * 16 frames × 32px wide per row, 4 variants stacked vertically.
     * Explosion: Blaster.inc:127  explo_o = 001+(930*141) → starts at (1,141) */
    if (dc->assets->monster_sheet_loaded) {
        for (int mi = 0; mi < NBS_MONSTER; mi++) {
            const Monster *m = &g->monsters[mi];
            if (m->exploding) {
                /* Explosion sprite: 70x70, 13 frames at (1, 141) in monster sheet.
                 * Frames have 1px border: stride = EXPLO_W + 1 = 71px. */
                int ef = m->explo_frame;
                if (ef >= EXPLO_NBS_ANIM) ef = EXPLO_NBS_ANIM - 1;
                Rectangle src = { (float)(1 + ef * (EXPLO_W + 1)), 141.0f,
                                  (float)EXPLO_W, (float)EXPLO_H };
                Vector2 pos = { (float)m->x, (float)m->y };
                DrawTextureRec(dc->assets->monster_sheet, src, pos, WHITE);
            } else if (m->active) {
                /* Monster sprite: 32x32, 16 frames.
                 * MONSTER.png has 1px orange borders between frames.
                 * Stride = MONSTER_W + 1 = 33px per frame.
                 * Row = 1 + variant * 35, Col = 1 + frame * 33 */
                int row_y = 1 + m->variant * (3 + MONSTER_H);
                Rectangle src = { (float)(1 + m->anim_frame * (MONSTER_W + 1)), (float)row_y,
                                  (float)MONSTER_W, (float)MONSTER_H };
                Vector2 pos = { (float)m->x, (float)m->y };
                DrawTextureRec(dc->assets->monster_sheet, src, pos, WHITE);
            }
        }
    }

    /* Step 4: Balls
     * DRAW.ASM:Draw_sprites sprite_mode=ball */
    draw_balls(dc, g);

    /* Step 5: Paddle
     * DRAW.ASM:Draw_sprites sprite_mode=vaisseau */
    draw_paddle(dc, g);

    /* Step 6: Projectiles
     * DRAW.ASM:Draw_sprites sprite_mode=tir (shoot) */
    draw_projectiles(dc, g);

    /* Step 6b: Night mode overlay — detect_init_palette equivalent.
     * MAIN.ASM: detect_init_palette darkens the VGA palette during POWERUP_NIGHT.
     * Raylib equivalent: semi-transparent black rectangle over play area.
     * Drawn BEFORE HUD so score/lives remain readable. */
    if (g->night_active) {
        DrawRectangle(PLAY_X1, PLAY_Y1, PLAY_X2 - PLAY_X1, SCREEN_H,
                      (Color){0, 0, 0, 160});
    }

    /* Step 7: HUD — always drawn last (topmost layer)
     * DRAW.ASM:Draw_sprites sprite_mode=panel */
    draw_hud(dc, g);

    /* Dev test mode indicator */
    if (g->dev_test) {
        DrawText("DEV TEST [F9]", 2, 2, 10, YELLOW);
        /* Show next powerup type number */
        char buf[32];
        snprintf(buf, sizeof(buf), "Next: %d/23", g->dev_powerup_cycle);
        DrawText(buf, 2, 14, 10, YELLOW);
    }

    EndTextureMode();
}


/* ============================================================================
 * draw_canvas_to_screen — blit the 640x480 canvas to the window.
 *
 * Equivalent to DRAW.ASM:Flip_Buffer / _flip_screen:
 *   DRAW.ASM:831  Flip_Buffer: copy video_buffer rows to VESA screen via SBANK.
 * Here we use DrawTexturePro for letterboxed integer-scale upscaling.
 *
 * MUST be called inside BeginDrawing()/EndDrawing().
 * The canvas RenderTexture2D stores the image bottom-up (OpenGL convention),
 * so we flip the source rect height to -SCREEN_H to correct orientation.
 * ============================================================================ */
void draw_canvas_to_screen(DrawContext *dc, Game *g) {
    /* Source rect: full canvas, flipped vertically (OpenGL texture coord fix).
     * DRAW.ASM:Flip_Buffer  mov edi,0; mov ecx,screen_y; mov edx,screen_x */
    Rectangle src = { 0, 0, (float)SCREEN_W, -(float)SCREEN_H };

    int win_w = GetScreenWidth();
    int win_h = GetScreenHeight();

    /* Compute letterboxed destination to preserve pixel-perfect ratio.
     * The default window is 1280x960 (exactly 2x), but handle any window size. */
    float scale_x = (float)win_w / SCREEN_W;
    float scale_y = (float)win_h / SCREEN_H;
    float scale   = (scale_x < scale_y) ? scale_x : scale_y;

    int dst_w = (int)(SCREEN_W * scale);
    int dst_h = (int)(SCREEN_H * scale);
    int dst_x = (win_w - dst_w) / 2;
    int dst_y = (win_h - dst_h) / 2;

    /* P0-ASM-6: removed non-ASM screen shake and red-flash overlay.
     * ASM has no equivalent — DRAW.ASM:Flip_Buffer just copies the video
     * buffer to the screen with no post-processing. */
    Rectangle dst = { (float)dst_x, (float)dst_y,
                       (float)dst_w, (float)dst_h };

    DrawTexturePro(dc->canvas.texture, src, dst, (Vector2){0,0}, 0.0f, WHITE);
    (void)g;  /* unused after shake removal */

#if defined(BRICKBLASTER_MOBILE)
    /* Draw L/R touch button extensions in the letterbox margins (screen space).
     * The canvas buttons cover 0..PLAY_X1 and PLAY_X2..SCREEN_W in game coords.
     * Here we extend them into the letterbox area (0..dst_x and dst_x+dst_w..win_w)
     * so the full screen edge is tappable. */
    {
        /* Convert button Y from canvas to screen coords */
        int btn_y = dst_y + (int)(MC_LEFT_Y * scale);
        int btn_h = (int)(MC_LEFT_H * scale);
        /* Extend from screen edge to canvas edge */
        Color margin_bg = (Color){30, 30, 44, 100};
        Color margin_border = (Color){110, 110, 135, 120};
        if (dst_x > 0) {
            /* Left margin button: 0 to canvas left edge */
            DrawRectangle(0, btn_y, dst_x, btn_h, margin_bg);
            DrawRectangleLinesEx((Rectangle){0, (float)btn_y, (float)dst_x, (float)btn_h},
                                1.0f, margin_border);
        }
        if (dst_x + dst_w < win_w) {
            /* Right margin button: canvas right edge to screen edge */
            int rx = dst_x + dst_w;
            int rw = win_w - rx;
            DrawRectangle(rx, btn_y, rw, btn_h, margin_bg);
            DrawRectangleLinesEx((Rectangle){(float)rx, (float)btn_y, (float)rw, (float)btn_h},
                                1.0f, margin_border);
        }
    }
#endif
}


/* ============================================================================
 * draw_frame — convenience wrapper.
 * Renders game world to canvas then blits to window.
 * MUST be called between BeginDrawing() and EndDrawing().
 * ============================================================================ */
void draw_frame(DrawContext *dc, Game *g) {
    draw_frame_to_canvas(dc, g);
    draw_canvas_to_screen(dc, g);
}
