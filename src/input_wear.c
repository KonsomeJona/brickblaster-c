#include "input_wear.h"

#if defined(PLATFORM_ANDROID)

#include <jni.h>
#include <android/log.h>

#define LOG_TAG "BrickBlaster"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* Cached JNI references — obtained once in wear_input_init. */
static JavaVM  *s_jvm        = NULL;
static jclass   s_activity_class  = NULL;
static jmethodID s_consume_rotary = NULL;
static jmethodID s_consume_press  = NULL;

/* -------------------------------------------------------------------------
 * wear_input_init — called from main() after InitWindow().
 *
 * android_app_ptr is the struct android_app* that raylib exposes via
 * GetAndroidApp().  We use it to get the JNIEnv and cache method IDs.
 * ------------------------------------------------------------------------- */
void wear_input_init(void *android_app_ptr)
{
    /* raylib exposes the android_app via GetAndroidApp() — we receive it
     * as a void* to avoid pulling in android/native_activity.h here. */
    struct {
        void *userData;
        void *onAppCmd;
        void *onInputEvent;
        void *activity;          /* ANativeActivity* — first useful field */
    } *app = android_app_ptr;

    if (!app || !app->activity) {
        LOGE("wear_input_init: null android_app or activity");
        return;
    }

    /* activity is ANativeActivity; its first field is clazz (jobject) and
     * its second field is env (JNIEnv*) — layout from <android/native_activity.h>:
     *   struct ANativeActivity {
     *       struct ANativeActivityCallbacks *callbacks;
     *       JavaVM *vm;
     *       JNIEnv *env;
     *       jobject clazz;
     *       ...
     *   };
     */
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
        LOGE("wear_input_init: failed to attach JNI thread");
        return;
    }

    /* FindClass("com/takohi/...") fails on background threads — use GetObjectClass
     * on the activity jobject instead; it needs no class loader. */
    jclass cls = (*env)->GetObjectClass(env, activity->clazz);
    if (!cls) {
        LOGE("wear_input_init: GetObjectClass returned null");
        return;
    }

    /* Keep a global ref so the class isn't GC'd */
    s_activity_class = (*env)->NewGlobalRef(env, cls);

    s_consume_rotary = (*env)->GetStaticMethodID(env, s_activity_class,
                           "jniConsumeRotaryDelta", "()F");
    s_consume_press  = (*env)->GetStaticMethodID(env, s_activity_class,
                           "jniConsumeCrownPress",  "()Z");

    if (!s_consume_rotary || !s_consume_press) {
        LOGE("wear_input_init: JNI method not found");
        return;
    }

    LOGI("wear_input_init: OK");
}

/* -------------------------------------------------------------------------
 * wear_consume_rotary_delta — called each frame from game loop.
 * Returns accumulated crown rotation since last call (CW positive = right).
 * ------------------------------------------------------------------------- */
float wear_consume_rotary_delta(void)
{
    if (!s_jvm || !s_activity_class || !s_consume_rotary) return 0.0f;

    JNIEnv *env = NULL;
    (*s_jvm)->AttachCurrentThread(s_jvm, &env, NULL);
    if (!env) return 0.0f;

    return (*env)->CallStaticFloatMethod(env, s_activity_class, s_consume_rotary);
}

/* -------------------------------------------------------------------------
 * wear_consume_crown_press — returns 1 once per crown button press.
 * ------------------------------------------------------------------------- */
int wear_consume_crown_press(void)
{
    if (!s_jvm || !s_activity_class || !s_consume_press) return 0;

    JNIEnv *env = NULL;
    (*s_jvm)->AttachCurrentThread(s_jvm, &env, NULL);
    if (!env) return 0;

    return (int)(*env)->CallStaticBooleanMethod(env, s_activity_class, s_consume_press);
}

#endif /* PLATFORM_ANDROID */
