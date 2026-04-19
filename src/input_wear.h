#ifndef INPUT_WEAR_H
#define INPUT_WEAR_H

/* Wear OS crown / rotary encoder input bridge.
 *
 * On PLATFORM_ANDROID only — the Java BrickBlasterActivity intercepts
 * MotionEvent.AXIS_SCROLL from SOURCE_ROTARY_ENCODER (Pixel Watch crown)
 * and accumulates the delta.  These functions are called from the game
 * loop each frame to consume the accumulated values.
 *
 * On other platforms these are no-ops.
 */

#if defined(PLATFORM_ANDROID)

/* Call once at startup to cache the JNI method references. */
void wear_input_init(void *android_app_ptr);

/* Returns accumulated rotary delta since last call (positive = CW = right).
 * Resets the accumulator to zero. */
float wear_consume_rotary_delta(void);

/* Returns 1 once if the crown push-button was pressed since last call. */
int wear_consume_crown_press(void);

#else  /* stub for non-Android platforms */
static inline void  wear_input_init(void *p)       { (void)p; }
static inline float wear_consume_rotary_delta(void){ return 0.0f; }
static inline int   wear_consume_crown_press(void) { return 0; }
#endif

#endif /* INPUT_WEAR_H */
