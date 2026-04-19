#include "screen_manager.h"

// MAIN.ASM:216, 643 - initial state
void screen_state_init(ScreenState *state) {
    if (!state) return;

    state->game_mode = STATE_INTRO;
    state->current_menu = 1;
    state->demo_flag = 0;
    state->demo_counter = 0;
    state->world = 0;
    state->nbs_player = 1;
    state->dual_flag = 0;
    state->control_p2 = 1;  /* default keyboard for P2 */
    state->difficulte = 2;  // Normal
    state->hiscore_mode = 0;
    state->quit_requested = 0;
    state->dev_test = 0;
    state->music_enabled = 1;
    state->sfx_enabled = 1;
    state->drag_enabled = 1;   /* finger drag ON by default */
    state->tilt_enabled = 0;   /* tilt OFF by default */
    state->button_speed = 3;   /* pad buttons: Medium */
    state->tilt_speed   = 3;   /* tilt: Medium */
}

// MAIN.ASM:48-58, 142 - music switching based on state
void screen_update_music(ScreenState *state, MusicManager *music) {
    if (!state || !music) return;

    MusicTrack desired = MUSIC_BLASTER;  // Default

    switch (state->game_mode) {
        case STATE_INTRO:
        case STATE_TITLE:
        case STATE_INTRO_ORIGINAL:
        case STATE_MENU:
            desired = MUSIC_BLASTER;
            break;
        case STATE_PLAYING:
        case STATE_READY_TO_PLAY:
        case STATE_READY_TO_PLAY_AGAIN:
            desired = (state->world == 0) ? MUSIC_THELAST : MUSIC_LODE;
            break;
        case STATE_HISCORE:
        case STATE_FINAL:
            desired = MUSIC_RAIN;
            break;
        case STATE_CREDITS:
            desired = MUSIC_CREDIT;
            break;
        default:
            return;  // No change
    }

    music_manager_switch(music, desired);
}
