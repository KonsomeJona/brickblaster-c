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
 * fps: playback speed (18.0 for standard FLC)
 */
Animation animation_load(const char *dir, int count, float fps);

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
