#pragma once
/* music_manager.h — Music track switching
 * MAIN.ASM:48-58, 142 - music system
 */

#include <raylib.h>

#define MUSIC_DEFAULT_VOLUME 0.5f

// MAIN.ASM:48-58 - 5 MOD music tracks
typedef enum {
    MUSIC_BLASTER = 0,    // blaster.mod - intro/menu
    MUSIC_THELAST,        // thelast.mod - world 0 (space)
    MUSIC_LODE,           // lode.mod - world 1 (arcade)
    MUSIC_CREDIT,         // credit.mod - credits
    MUSIC_RAIN,           // rain.mod - high scores
    MUSIC_COUNT
} MusicTrack;

typedef struct {
    Music tracks[MUSIC_COUNT];
    int tracks_loaded[MUSIC_COUNT];
    MusicTrack current;
    int initialized;
    int music_enabled;   /* 1 = play normally, 0 = volume forced to zero   */
} MusicManager;

/* Initialize and load all music tracks */
void music_manager_init(MusicManager *mgr);

/* Switch to desired track (stops current, plays new) */
void music_manager_switch(MusicManager *mgr, MusicTrack track);

/* Update current track (must call every frame) */
void music_manager_update(MusicManager *mgr);

/* Enable or disable music playback (volume 0 = silent, volume restored = enabled).
 * Does not stop the stream — toggling back on resumes immediately. */
void music_manager_set_enabled(MusicManager *mgr, int enabled);

/* Cleanup */
void music_manager_cleanup(MusicManager *mgr);
