#ifndef INPUT_TILT_H
#define INPUT_TILT_H

/* Accelerometer tilt input bridge (mobile only).
 *
 * On BRICKBLASTER_MOBILE (Android phone/tablet) — the Java activity registers
 * a SensorEventListener for TYPE_ACCELEROMETER and stores the X-axis value.
 * These functions are called from input_frame.c each frame to read it.
 *
 * On other platforms these are no-ops.
 */

#if defined(BRICKBLASTER_MOBILE)

/* Call once at startup (after wear_input_init) to cache JNI method refs. */
void tilt_input_init(void *android_app_ptr);

/* Returns normalised tilt X (-1.0 = full left, +1.0 = full right).
 * The value is smoothed by the Java side. */
float tilt_consume_x(void);

#else  /* stub for non-mobile platforms */
static inline void  tilt_input_init(void *p)  { (void)p; }
static inline float tilt_consume_x(void)      { return 0.0f; }
#endif

#endif /* INPUT_TILT_H */
