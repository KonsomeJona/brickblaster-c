#pragma once
/* screen_menu.h — 7-screen hierarchical menu (1:1 with MAIN.ASM / Blaster.inc).
 *
 * Each menu shows one 259x260 icon at (189, 141) containing a 2x2 grid of
 * buttons a/b/c/d. The hit-rects match the original table at
 * MAIN.ASM:6913-6942 (struc_menus entries).
 *
 *   current_menu = 1  main menu       play     / demo      / misc      / quit
 *   current_menu = 2  players         1 player / coop      / dual      / cancel
 *   current_menu = 3  ctrl player 2   computer / keyboard  / joystick  / cancel
 *   current_menu = 4  select world    space    / arcade    / -         / cancel
 *   current_menu = 5  skill level     easy     / medium    / hard      / cancel
 *   current_menu = 6  miscellaneous   hiscore  / hiscore co/ credits   / cancel
 *   current_menu = 7  joystick        analogic / numeric   / -         / cancel
 */

#include "screen_manager.h"
#include "constants.h"
#include "audio.h"
#include "input_frame.h"
#include "assets.h"
#include "font.h"
#include <raylib.h>

typedef struct {
    Assets    *assets;   /* shared — owned by main() (menu image, font, title) */
    Texture2D  menu_bg;  /* Blaster.png — 640x480 menu background */
    int        menu_bg_loaded;
    BitmapFont font;     /* initialised once in menu_assets_load */
    int        font_ready;

    int hover_button;    /* 0..3, or -1 if none (a=0, b=1, c=2, d=3) */
    int cursor_x, cursor_y; /* cursor position in canvas coords (for mouse display) */
    int idle_frames;     /* frames since last interaction (attract-mode demo) */
} MenuAssets;

void menu_assets_load(MenuAssets *assets, Assets *game_assets);
void menu_assets_unload(MenuAssets *assets);
void menu_handle_input(ScreenState *state, MenuAssets *assets, AudioState *audio,
                       const FrameInput *input);
void menu_draw(ScreenState *state, MenuAssets *assets);
