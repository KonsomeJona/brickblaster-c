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

/* -DGIF_RECORDER=OFF drops gif_recorder.c from the target (CMakeLists.txt:80)
 * but main.c calls these four unconditionally, so without stubs the build
 * simply failed to link — the documented "vanilla" flag was unusable on every
 * platform. Inline no-ops keep the call sites free of #ifdef noise. */
#if defined(GIF_RECORDER)

void gif_recorder_toggle(void);
void gif_recorder_capture(RenderTexture2D canvas);
int  gif_recorder_is_recording(void);
void gif_recorder_shutdown(void);

#else

static inline void gif_recorder_toggle(void) {}
static inline void gif_recorder_capture(RenderTexture2D canvas) { (void)canvas; }
static inline int  gif_recorder_is_recording(void) { return 0; }
static inline void gif_recorder_shutdown(void) {}

#endif
