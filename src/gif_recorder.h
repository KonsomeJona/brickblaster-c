#pragma once
/* gif_recorder.h — Animated GIF screen recorder.
 *
 * Captures the 640×480 canvas at ~30 FPS and writes an animated GIF.
 * Toggle with F12. Auto-stops after GIF_MAX_FRAMES frames.
 *
 * Uses Charlie Tangora's gif.h (public domain) — see third_party/gif.h.
 */

#include "draw.h"
#include <raylib.h>

#define GIF_FPS           30   /* 30 fps (1 frame out of 2 at 60 Hz) */
#define GIF_MAX_FRAMES   900   /* auto-stop after 30 s */

void gif_recorder_toggle(void);
void gif_recorder_capture(RenderTexture2D canvas);
int  gif_recorder_is_recording(void);
void gif_recorder_shutdown(void);
