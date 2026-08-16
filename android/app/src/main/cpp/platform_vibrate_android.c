/* platform_vibrate_android.c — haptics for the Android build.
 *
 * Bridges src/game.c's platform_vibrate()/platform_vibrate_pattern() to
 * static Java methods on BrickBlasterActivity (jniVibrate/jniVibratePattern)
 * via JNI, using the same lazy android_app introspection pattern as
 * src/input_tilt.c / src/input_wear.c.
 */

#include "platform_vibrate.h"

#include <jni.h>
#include <android/log.h>
#include <stddef.h>

#define LOG_TAG "BrickBlaster"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* Provided by raylib's rcore_android.c (no public header). */
struct android_app;
extern struct android_app *GetAndroidApp(void);

static JavaVM   *s_jvm             = NULL;
static jclass    s_class           = NULL;
static jmethodID s_vibrate         = NULL;
static jmethodID s_vibrate_pattern = NULL;
static int       s_init_failed     = 0;

static int ensure_init(JNIEnv **env_out)
{
    if (s_init_failed) return 0;

    if (!s_jvm) {
        /* Field layout mirrors the compat structs in src/input_wear.c. */
        struct app_compat {
            void *userData;
            void *onAppCmd;
            void *onInputEvent;
            void *activity;
        } *app = (struct app_compat *)GetAndroidApp();

        struct activity_compat {
            void   *callbacks;
            JavaVM *vm;
            JNIEnv *env;
            jobject clazz;
        } *activity;

        if (!app || !app->activity) return 0;
        activity = (struct activity_compat *)app->activity;
        s_jvm = activity->vm;

        JNIEnv *env = NULL;
        (*s_jvm)->AttachCurrentThread(s_jvm, &env, NULL);
        if (!env) { s_init_failed = 1; return 0; }

        jclass cls = (*env)->GetObjectClass(env, activity->clazz);
        if (!cls) { s_init_failed = 1; return 0; }
        s_class = (*env)->NewGlobalRef(env, cls);

        s_vibrate         = (*env)->GetStaticMethodID(env, s_class, "jniVibrate", "(I)V");
        s_vibrate_pattern = (*env)->GetStaticMethodID(env, s_class, "jniVibratePattern", "([J)V");
        if (!s_vibrate || !s_vibrate_pattern) {
            LOGE("platform_vibrate: JNI methods not found");
            s_init_failed = 1;
            return 0;
        }
    }

    JNIEnv *env = NULL;
    (*s_jvm)->AttachCurrentThread(s_jvm, &env, NULL);
    if (!env) return 0;
    *env_out = env;
    return 1;
}

void platform_vibrate(int ms)
{
    JNIEnv *env = NULL;
    if (!ensure_init(&env)) return;
    (*env)->CallStaticVoidMethod(env, s_class, s_vibrate, (jint)ms);
}

void platform_vibrate_pattern(const int *pattern, int count)
{
    JNIEnv *env = NULL;
    if (!pattern || count <= 0) return;
    if (!ensure_init(&env)) return;

    jlongArray arr = (*env)->NewLongArray(env, count);
    if (!arr) return;
    jlong tmp[16];
    if (count > 16) count = 16;  /* game patterns are 4-6 entries */
    for (int i = 0; i < count; i++) tmp[i] = (jlong)pattern[i];
    (*env)->SetLongArrayRegion(env, arr, 0, count, tmp);
    (*env)->CallStaticVoidMethod(env, s_class, s_vibrate_pattern, arr);
    (*env)->DeleteLocalRef(env, arr);
}
