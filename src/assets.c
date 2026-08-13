#include "assets.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

/*
 * assets.c - BrickBlaster asset loading (raylib)
 *
 * Paths are relative to the executable working directory.
 * The CMake install/run setup places assets/ next to the binary.
 * All texture loads use LoadTexture() which reads PNG via stb_image.
 *
 * Sprite layout references: brickblaster-godot/docs/SPRITES.md
 * ASM references: Blaster.inc (sprite offsets), MAIN.ASM (sprite table)
 */

/* ASSETS_BASE is defined in assets.h (platform-conditional) */

/* Helper: load a texture and warn if missing */
static Texture2D load_texture_warn(const char *path, int *loaded_out)
{
    Texture2D tex = {0};
    *loaded_out = 0;

    /* On Android, FileExists does not go through the Asset Manager, so skip it.
     * On desktop, the pre-check gives a clearer warning message. */
#if !defined(PLATFORM_ANDROID)
    if (!FileExists(path)) {
        TraceLog(LOG_WARNING, "ASSETS: missing texture: %s", path);
        return tex;
    }
#endif

    tex = LoadTexture(path);
    if (tex.id == 0) {
        TraceLog(LOG_WARNING, "ASSETS: failed to load texture: %s", path);
        return tex;
    }

    TraceLog(LOG_INFO, "ASSETS: loaded %s (%dx%d)", path, tex.width, tex.height);
    *loaded_out = 1;
    return tex;
}

int assets_load(Assets *a)
{
    int count = 0;
    char path[256];

    memset(a, 0, sizeof(*a));

    /* --- Core sprite sheets (SPRITE0.png / SPRITE1.png, 640px wide) ---
     * Blaster.inc: all in-game sprites packed into one 640-wide buffer.
     * Ball at (0,0), paddle at (0,42), bricks at (66,95), etc.
     * See SPRITES.md section 4-7 for complete layout.
     *
     * One sheet per world palette: Read_Palette (FILE.ASM:776-791) replaces
     * the sprite palette with sprite0.pal or sprite1.pal on every world load
     * (digit patched at file_palette+6 — MAIN.ASM:483/491). The sheets are
     * SPRITE.GIF indices rendered through each palette (VGA 6-bit ×4).
     * World 1 reuses the legacy SPRITE.png: rendering SPRITE.GIF through
     * sprite1.pal reproduces it pixel for pixel (verified, 0 differing
     * pixels over 640x1900), so shipping a separate SPRITE1.png would be
     * a byte-identical duplicate to keep in sync for nothing.
     */
    static const char *const sheet_paths[ASSETS_SPRITE_WORLDS] = {
        ASSETS_BASE "sprites/SPRITE0.png",  /* world 0 — sprite0.pal */
        ASSETS_BASE "sprites/SPRITE.png",   /* world 1 — sprite1.pal */
    };
    for (int w = 0; w < ASSETS_SPRITE_WORLDS; w++) {
        a->sprite_sheets[w] = load_texture_warn(sheet_paths[w],
                                                &a->sprite_sheets_loaded[w]);
        count += a->sprite_sheets_loaded[w];
    }

    /* Default world is 0: FILE.ASM:9 `File_Palette db 'sprite0.pal',0`
     * and MAIN.ASM `world db '0'`. */
    a->sprite_world = 0;
    a->sprite_sheet        = a->sprite_sheets[0];
    a->sprite_sheet_loaded = a->sprite_sheets_loaded[0];

    /* --- Font sheet (FONTE.png) ---
     * FONTE.ASM: bitmap font for score, HUD, and text rendering.
     */
    a->font_sheet = load_texture_warn(ASSETS_BASE "sprites/FONTE.png",
                                       &a->font_sheet_loaded);
    count += a->font_sheet_loaded;

    /* --- Menu full-screen image (MENU.png) ---
     * Used for title/menu screen background.
     */
    a->menu_image = load_texture_warn(ASSETS_BASE "sprites/MENU.png",
                                       &a->menu_image_loaded);
    count += a->menu_image_loaded;

    /* --- Monster sprite sheet (MONSTER.png) ---
     * MAIN.ASM: 4 monster types, 16 animation frames each, 32x32px.
     * MONSTER_SCREEN_X = 930 (buffer width).
     */
    a->monster_sheet = load_texture_warn(ASSETS_BASE "sprites/MONSTER.png",
                                          &a->monster_sheet_loaded);
    count += a->monster_sheet_loaded;

    /* --- Title/intro image (Blaster.png) ---
     * Full-screen intro/splash image.
     */
    a->title_image = load_texture_warn(ASSETS_BASE "sprites/Blaster.png",
                                        &a->title_image_loaded);
    count += a->title_image_loaded;

    /* --- Background images ---
     * Two sets of 8 backgrounds (one per level group).
     * Naming: XX_YY.png where XX = set (00/01), YY = level (01..08).
     * Original game: backgrounds stored as raw palette bitmaps;
     * converted to PNG by the Godot extraction tools.
     */
    for (int set = 0; set < ASSETS_BG_SETS; set++) {
        for (int idx = 0; idx < ASSETS_BG_COUNT; idx++) {
            snprintf(path, sizeof(path),
                     ASSETS_BASE "sprites/%02d_%02d.png", set, idx + 1);
            a->backgrounds[set][idx] = load_texture_warn(
                path, &a->backgrounds_loaded[set][idx]);
            count += a->backgrounds_loaded[set][idx];
        }
    }

    TraceLog(LOG_INFO, "ASSETS: loaded %d textures total", count);
    return count;
}

void assets_select_world(Assets *a, int world)
{
    /* World 2 (@@coin_coin, MAIN.ASM:495-501) is commented out in the ASM:
     * selecting it never touches file_palette, so the previous palette
     * stays in effect. Reproduce that as a no-op. */
    if (world < 0 || world >= ASSETS_SPRITE_WORLDS) {
        TraceLog(LOG_WARNING, "ASSETS: world %d has no sprite palette, keeping world %d",
                 world, a->sprite_world);
        return;
    }
    if (!a->sprite_sheets_loaded[world]) {
        TraceLog(LOG_WARNING, "ASSETS: sprite sheet for world %d not loaded, keeping world %d",
                 world, a->sprite_world);
        return;
    }
    a->sprite_world        = world;
    a->sprite_sheet        = a->sprite_sheets[world];
    a->sprite_sheet_loaded = a->sprite_sheets_loaded[world];
}

void assets_unload(Assets *a)
{
    /* sprite_sheet is an alias of one of sprite_sheets[] — clear it without
     * UnloadTexture() so the GPU texture is not freed twice. */
    a->sprite_sheet = (Texture2D){0};
    a->sprite_sheet_loaded = 0;
    for (int w = 0; w < ASSETS_SPRITE_WORLDS; w++) {
        if (a->sprite_sheets_loaded[w]) {
            UnloadTexture(a->sprite_sheets[w]);
            a->sprite_sheets_loaded[w] = 0;
        }
    }
    if (a->font_sheet_loaded)    { UnloadTexture(a->font_sheet);      a->font_sheet_loaded    = 0; }
    if (a->menu_image_loaded)    { UnloadTexture(a->menu_image);      a->menu_image_loaded    = 0; }
    if (a->monster_sheet_loaded) { UnloadTexture(a->monster_sheet);   a->monster_sheet_loaded = 0; }
    if (a->title_image_loaded)   { UnloadTexture(a->title_image);     a->title_image_loaded   = 0; }

    for (int set = 0; set < ASSETS_BG_SETS; set++) {
        for (int idx = 0; idx < ASSETS_BG_COUNT; idx++) {
            if (a->backgrounds_loaded[set][idx]) {
                UnloadTexture(a->backgrounds[set][idx]);
                a->backgrounds_loaded[set][idx] = 0;
            }
        }
    }

    TraceLog(LOG_INFO, "ASSETS: all textures unloaded");
}

Texture2D assets_load_takohi_logo(void)
{
    Texture2D tex = {0};
    Image img = LoadImage(ASSETS_BASE "takohi/Logo_Eng.png");
    if (!img.data) return tex;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *px = (Color *)img.data;
    int n = img.width * img.height;
    for (int i = 0; i < n; i++) {
        if (px[i].a > 64 && px[i].r < 80 && px[i].g < 80 && px[i].b < 80)
            px[i] = (Color){255, 255, 255, px[i].a};
    }
    tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

const Texture2D *assets_get_background(const Assets *a, int set, int index)
{
    if (set < 0 || set >= ASSETS_BG_SETS)   return NULL;
    if (index < 0 || index >= ASSETS_BG_COUNT) return NULL;
    if (!a->backgrounds_loaded[set][index])    return NULL;
    return &a->backgrounds[set][index];
}
