#include "input_tilt.h"

#if defined(BRICKBLASTER_MOBILE)

#include <jni.h>
#include <android/log.h>

#define LOG_TAG "BrickBlaster"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* Cached JNI references — obtained once in tilt_input_init. */
static JavaVM    *s_jvm         = NULL;
static jclass     s_class       = NULL;
static jmethodID  s_consume_tilt = NULL;

/* -------------------------------------------------------------------------
 * tilt_input_init — called from main() after InitWindow().
 * Same pattern as wear_input_init: extract JNI env from android_app.
 * ------------------------------------------------------------------------- */
void tilt_input_init(void *android_app_ptr)
{
    struct {
        void *userData;
        void *onAppCmd;
        void *onInputEvent;
        void *activity;
    } *app = android_app_ptr;

    if (!app || !app->activity) {
        LOGE("tilt_input_init: null android_app or activity");
        return;
    }

    typedef struct {
        void   *callbacks;
        JavaVM *vm;
        JNIEnv *env;
        jobject clazz;
    } NativeActivityCompat;

    NativeActivityCompat *activity = (NativeActivityCompat *)app->activity;
    s_jvm = activity->vm;

    JNIEnv *env = NULL;
    (*s_jvm)->AttachCurrentThread(s_jvm, &env, NULL);
    if (!env) {
        LOGE("tilt_input_init: failed to attach JNI thread");
        return;
    }

    jclass cls = (*env)->GetObjectClass(env, activity->clazz);
    if (!cls) {
        LOGE("tilt_input_init: GetObjectClass returned null");
        return;
    }

    s_class = (*env)->NewGlobalRef(env, cls);

    s_consume_tilt = (*env)->GetStaticMethodID(env, s_class,
                         "jniConsumeTiltX", "()F");

    if (!s_consume_tilt) {
        LOGE("tilt_input_init: jniConsumeTiltX not found");
        return;
    }

    LOGI("tilt_input_init: OK");
}

/* -------------------------------------------------------------------------
 * tilt_consume_x — called each frame from input_frame.c.
 * Returns normalised tilt X: negative=left, positive=right.
 * ------------------------------------------------------------------------- */
float tilt_consume_x(void)
{
    if (!s_jvm || !s_class || !s_consume_tilt) return 0.0f;

    JNIEnv *env = NULL;
    (*s_jvm)->AttachCurrentThread(s_jvm, &env, NULL);
    if (!env) return 0.0f;

    return (*env)->CallStaticFloatMethod(env, s_class, s_consume_tilt);
}

#endif /* BRICKBLASTER_MOBILE */
