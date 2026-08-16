#pragma once
#include "raylib.h"

/*
 * assets.h - BrickBlaster asset loading
 *
 * Sprite sheet layout documented in brickblaster-godot/docs/SPRITES.md
 * All offsets derived from Blaster.inc and MAIN.ASM sprite table.
 *
 * Sprite sheet (SPRITE0.png / SPRITE1.png per world palette, 640px wide;
 * same layout in both — only the palette differs):
 *   Ball blue:        Rect( 0,   0, 9, 9)
 *   Ball orange:      Rect( 0,   9, 9, 9)
 *   Ghost ball blue:  Rect(18,   0, 9, 9)
 *   Ghost ball orange:Rect(18,   9, 9, 9)
 *   Paddle P1 normal: Rect( 0,  42, 74, 25)
 *   Paddle P1 large:  Rect(296, 42, 105, 25)
 *   Paddle P1 small:  Rect(506, 42, 38, 25)
 *   Brick row:        Rect(66,  95, 32, 16)  (green, color rows at y+17 each)
 *
 * Backgrounds: assets/sprites/00_01.png .. 00_08.png (set 0)
 *              assets/sprites/01_01.png .. 01_08.png (set 1)
 *
 * Audio SFX: assets/audio/*.wav  (BOOM, BOUNCE, DEATH, etc.)
 * Music:     assets/music/*.wav  (Blaster, Credit, Lode, Rain, Thelast)
 * Font:      assets/sprites/FONTE.png
 * Menu:      assets/sprites/MENU.png
 * Monsters:  assets/sprites/MONSTER.png
 */

/*
 * ASSETS_BASE — prefix for all asset paths.
 * On Android the Asset Manager roots at the APK's assets/ folder,
 * so paths must NOT include the "assets/" prefix.
 * On all other platforms assets/ sits next to the executable.
 */
#if defined(PLATFORM_ANDROID)
#define ASSETS_BASE ""
#else
#define ASSETS_BASE "assets/"
#endif

/* Number of background images per level set (8 backgrounds, 2 sets) */
#define ASSETS_BG_COUNT     8
#define ASSETS_BG_SETS      2

/*
 * Number of per-world sprite sheets.
 * FILE.ASM:9 declares `File_Palette db 'sprite0.pal',0` and MAIN.ASM:483/491
 * patch the digit at [file_palette+6] to '0' (space) or '1' (eclipse);
 * Read_Palette (FILE.ASM:776-791) then swaps the 768-byte sprite palette on
 * every world load. World 2 (@@coin_coin, MAIN.ASM:495-501) is commented out
 * in the ASM and never loads a palette, so only two sheets exist.
 */
#define ASSETS_SPRITE_WORLDS 2

typedef struct {
    /*
     * Core sprite sheet, one per world palette:
     *   sprite_sheets[0] = SPRITE0.png (Sprite0.pal — default, FILE.ASM:9)
     *   sprite_sheets[1] = SPRITE1.png (Sprite1.pal)
     * sprite_sheet / sprite_sheet_loaded alias the ACTIVE world's sheet so
     * existing consumers (draw.c) keep reading a->sprite_sheet unchanged.
     * Select with assets_select_world(); never UnloadTexture() the alias.
     */
    Texture2D sprite_sheets[ASSETS_SPRITE_WORLDS];
    int       sprite_sheets_loaded[ASSETS_SPRITE_WORLDS];
    int       sprite_world;     /* currently selected world (0 or 1) */

    Texture2D sprite_sheet;
    int       sprite_sheet_loaded;

    /* Font sprite sheet: FONTE.png (bitmap font from FONTE.ASM) */
    Texture2D font_sheet;
    int       font_sheet_loaded;

    /* Menu full-screen image: MENU.png */
    Texture2D menu_image;
    int       menu_image_loaded;

    /* Monster sprite sheet: MONSTER.png */
    Texture2D monster_sheet;
    int       monster_sheet_loaded;

    /* Blaster title/intro image: Blaster.png */
    Texture2D title_image;
    int       title_image_loaded;

    /*
     * Background images: [set][index]
     *   set 0: 00_01.png .. 00_08.png
     *   set 1: 01_01.png .. 01_08.png
     */
    Texture2D backgrounds[ASSETS_BG_SETS][ASSETS_BG_COUNT];
    int       backgrounds_loaded[ASSETS_BG_SETS][ASSETS_BG_COUNT];
} Assets;

/*
 * assets_load - Load all game assets.
 * Call once after InitWindow() and InitAudioDevice().
 * Missing assets are logged as warnings; the function does not crash.
 * Returns the number of successfully loaded textures.
 */
int  assets_load(Assets *a);

/*
 * assets_unload - Unload all loaded textures.
 * Call before CloseWindow().
 */
void assets_unload(Assets *a);

/*
 * assets_select_world - Point sprite_sheet at the given world's sprite sheet.
 * Mirrors the ASM palette swap: MAIN.ASM:483 ('0'), :491 ('1') patch
 * file_palette+6 and Read_Palette (FILE.ASM:776-791) reloads the palette.
 * world: 0 or 1. World 2 is a no-op (its ASM branch @@coin_coin,
 * MAIN.ASM:495-501, is commented out and leaves the previous palette).
 * Call after assets_load(), whenever the game world is chosen
 * (e.g. right after `game.world = state.world`).
 */
void assets_select_world(Assets *a, int world);

/*
 * assets_get_background - Return pointer to a loaded background texture.
 * set:   0 or 1
 * index: 0..7  (level 1 = index 0, level 8 = index 7)
 * Returns NULL if not loaded.
 */
const Texture2D *assets_get_background(const Assets *a, int set, int index);

/*
 * assets_load_takohi_logo - Load Logo_Eng.png and recolour dark pixels to white.
 * Returns a Texture2D with id==0 on failure.
 */
Texture2D assets_load_takohi_logo(void);
