#pragma once
/* animation.h — FLC animation playback
 * Loads PNG sequences from pre-converted FLC files.
 * MAIN.ASM uses FLC animations for intro and victory screens.
 */

#include <raylib.h>

typedef struct {
    Texture2D *frames;         // Array of loaded frames
    int frame_count;
    int current_frame;
    float fps;                 // Target playback speed
    float frame_interval;      // Pre-computed 1.0f/fps
    float frame_timer;         // Frame accumulator
    int loop;                  // Loop playback
} Animation;

/* Load animation from directory of PNG files
 * dir: path like "assets/intro"
 * count: number of frames
 * fps: playback speed — use FLC_FPS for anything converted from a 1999 .flc
 */
Animation animation_load(const char *dir, int count, float fps);

/* Playback rate of every FLC in the original: the player waits 5 vsyncs per
 * frame (FILE.ASM:96-99 for the intro, FILE.ASM:164-170 for the ending),
 * so 60/5 = 12 FPS. There is no "standard FLC rate" — it is whatever the
 * wait loop counts. */
#define FLC_FPS  12.0f

/* Update animation timer and advance frame */
void animation_update(Animation *anim);

/* Draw current frame */
void animation_draw(Animation *anim, int x, int y);

/* Check if animation finished (non-looping) */
int animation_is_finished(Animation *anim);

/* Unload animation textures */
void animation_unload(Animation *anim);

/* Reset to frame 0 */
void animation_reset(Animation *anim);
