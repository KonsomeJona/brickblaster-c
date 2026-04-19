/* gif_recorder.c — F12 toggle screen recorder → animated GIF.
 * Captures at 30 FPS (1 frame out of 2 at 60 Hz), 640×480 canvas.
 * Uses Charlie Tangora's gif.h (public domain).
 */

#include "gif_recorder.h"
#include "constants.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define GIF_FLIP_VERT   /* raylib RenderTexture2D is bottom-up */
#include "../third_party/gif.h"

static GifWriter s_writer;
static int       s_recording = 0;
static int       s_frame_count = 0;
static int       s_toggle_tick = 0;  /* skip every other frame to get 30 FPS */
static char      s_filename[256];
/* Persistent RGBA8 buffer reused across frames to avoid per-frame malloc.
 * Sized for SCREEN_W × SCREEN_H × 4 = 640×480×4 = 1 228 800 bytes. */
static unsigned char s_frame_buf[SCREEN_W * SCREEN_H * 4];

static void make_filename(char *out, int n) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    snprintf(out, n, "bb_%04d%02d%02d_%02d%02d%02d.gif",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
}

int gif_recorder_is_recording(void) { return s_recording; }

void gif_recorder_toggle(void) {
    if (!s_recording) {
        make_filename(s_filename, sizeof(s_filename));
        /* GIF delay is in 1/100s. 30 FPS → delay ≈ 3 (actually 3.33).
         * bitDepth=8, dither=false. */
        if (GifBegin(&s_writer, s_filename, SCREEN_W, SCREEN_H, 3, 8, false)) {
            s_recording = 1;
            s_frame_count = 0;
            s_toggle_tick = 0;
            TraceLog(LOG_INFO, "GIF: recording → %s", s_filename);
        } else {
            TraceLog(LOG_WARNING, "GIF: failed to create %s", s_filename);
        }
    } else {
        GifEnd(&s_writer);
        s_recording = 0;
        TraceLog(LOG_INFO, "GIF: stopped (%d frames → %s)",
                 s_frame_count, s_filename);
    }
}

/* Call every frame during gameplay. Captures 1 out of 2 frames. */
void gif_recorder_capture(RenderTexture2D canvas) {
    if (!s_recording) return;

    s_toggle_tick ^= 1;
    if (s_toggle_tick == 0) return;   /* skip every 2nd frame (60 → 30 fps) */

    /* Read canvas pixels (RGBA8). LoadImageFromTexture allocates. */
    Image img = LoadImageFromTexture(canvas.texture);
    if (img.data == NULL) return;

    /* Ensure RGBA8 format for gif.h */
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    if ((uint32_t)img.width == (uint32_t)SCREEN_W &&
        (uint32_t)img.height == (uint32_t)SCREEN_H) {
        GifWriteFrame(&s_writer, (const uint8_t *)img.data,
                      SCREEN_W, SCREEN_H, 3, 8, false);
        s_frame_count++;
    }
    UnloadImage(img);

    if (s_frame_count >= GIF_MAX_FRAMES) {
        gif_recorder_toggle();   /* auto-stop */
    }
}

void gif_recorder_shutdown(void) {
    if (s_recording) {
        GifEnd(&s_writer);
        s_recording = 0;
    }
}
