#pragma once
/* screen_manager.h — Game state machine
 * MAIN.ASM:216, 643 - game_mode and current_menu
 */

#include "music_manager.h"

// MAIN.ASM:216, Blaster.inc:420-462
// Undefine old GameState enum values from constants.h to avoid conflicts
#ifdef STATE_READY_TO_PLAY
#undef STATE_READY_TO_PLAY
#undef STATE_PLAYING
#undef STATE_EDIT
#undef STATE_GAME_OVER
#undef STATE_READY_TO_PLAY_AGAIN
#undef STATE_NEW_PLAY
#undef STATE_MENU
#endif

typedef enum {
    STATE_INTRO = 0,
    STATE_TITLE = 1,
    STATE_READY_TO_PLAY = 2,
    STATE_PLAYING = 3,
    STATE_EDIT = 4,
    STATE_GAME_OVER = 5,
    STATE_READY_TO_PLAY_AGAIN = 6,
    STATE_NEW_PLAY = 7,
    STATE_MENU = 8,
    STATE_HISCORE = 9,
    STATE_CREDITS = 10,
    STATE_FINAL = 11,
    STATE_PAUSED = 12,
    STATE_INTRO_ORIGINAL = 13   /* FLI intro + 6 credit GIFs before menu */
} GameMode;

// MAIN.ASM:643
typedef struct {
    GameMode game_mode;
    int current_menu;          // 1-7
    int demo_flag;             // 0=off, 1=on
    int demo_counter;          // Counts to 800
    int world;                 // 0=space, 1=eclipse
    int nbs_player;            // 1 or 2
    int dual_flag;             // 0=coop, 1=vs
    int control_p2;            // 0=computer, 1=keyboard, 2=joystick (menu 3)
    int difficulte;            // 1=easy, 2=medium, 4=hard
    int hiscore_mode;          // 0=1P, 1=2P
    int quit_requested;        // 1 = exit the main loop
    int dev_test;              // 1 = start in dev test mode (F9)
    int music_enabled;         // 1 = music on, 0 = music off
    int sfx_enabled;           // 1 = SFX on,   0 = SFX off
    int drag_enabled;          // 1=finger drag moves paddle, 0=off (default 1)
    int tilt_enabled;          // 1=accelerometer tilt moves paddle, 0=off (default 0)
    int button_speed;          // Pad button speed: 1=VeryLow..5=VeryHigh (default 3=Med)
    int tilt_speed;            // Tilt sensitivity: 1=VeryLow..5=VeryHigh (default 3=Med)
} ScreenState;

/* Initialize screen state */
void screen_state_init(ScreenState *state);

/* Update music based on current state */
void screen_update_music(ScreenState *state, MusicManager *music);
