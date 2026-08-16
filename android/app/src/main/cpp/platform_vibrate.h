#pragma once
/* platform_vibrate.h — haptics API used by src/game.c under
 * BRICKBLASTER_MOBILE.
 *
 * The repo's src/ tree calls these but does not declare or define them
 * (they were part of the author's private Android overlay). This header is
 * force-included into every game TU via `-include` from the Android
 * CMakeLists so src/ stays untouched; the implementation lives in
 * platform_vibrate_android.c next to this file.
 */

/* One-shot vibration, duration in milliseconds. */
void platform_vibrate(int ms);

/* Pattern vibration, Android Vibrator style: pattern[0] is an initial delay
 * in ms, then alternating vibrate/pause durations. count = array length. */
void platform_vibrate_pattern(const int *pattern, int count);
