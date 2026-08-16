package me.takohi.brickblaster;

import android.app.NativeActivity;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.Surface;
import android.view.View;
import android.view.WindowManager;

/**
 * NativeActivity host for the raylib game (libbrickblaster.so).
 *
 * The native side (src/input_tilt.c / src/input_wear.c) looks up STATIC
 * methods on this class via JNI:
 *   - jniConsumeTiltX()       float   — normalised tilt, -1 left .. +1 right
 *   - jniConsumeRotaryDelta() float   — Wear OS crown; stubbed to 0 on phone
 *   - jniConsumeCrownPress()  boolean — Wear OS crown; stubbed to false
 *
 * Do not rename these methods without updating the C sources.
 */
public class BrickBlasterActivity extends NativeActivity implements SensorEventListener {

    /** Latest normalised tilt (-1..+1), written on the sensor thread. */
    private static volatile float sTiltX = 0.0f;
    /** Display rotation, cached for accelerometer axis remapping. */
    private static volatile int sRotation = Surface.ROTATION_0;

    private SensorManager mSensorManager;
    private Sensor mAccelerometer;

    /** Full paddle speed at ~30 degrees of tilt (sin 30 = 0.5 g). */
    private static final float TILT_FULL_SCALE = SensorManager.GRAVITY_EARTH * 0.5f;

    // ---- JNI bridge (called from native code every frame) -------------------

    public static float jniConsumeTiltX() {
        return sTiltX;
    }

    public static float jniConsumeRotaryDelta() {
        return 0.0f; // no rotary crown on phones
    }

    public static boolean jniConsumeCrownPress() {
        return false; // no crown button on phones
    }

    /** One-shot haptic, called from platform_vibrate() in native code. */
    public static void jniVibrate(int ms) {
        Vibrator v = sVibrator;
        if (v == null || ms <= 0) return;
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                v.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE));
            } else {
                v.vibrate(ms);
            }
        } catch (Exception ignored) { }
    }

    /** Pattern haptic (Vibrator style: delay, on, off, ...), no repeat. */
    public static void jniVibratePattern(long[] pattern) {
        Vibrator v = sVibrator;
        if (v == null || pattern == null || pattern.length == 0) return;
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                v.vibrate(VibrationEffect.createWaveform(pattern, -1));
            } else {
                v.vibrate(pattern, -1);
            }
        } catch (Exception ignored) { }
    }

    private static volatile Vibrator sVibrator;

    // ---- Lifecycle ----------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mSensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        if (mSensorManager != null) {
            mAccelerometer = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        }
        sVibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
    }

    @Override
    protected void onResume() {
        super.onResume();
        sRotation = getWindowManager().getDefaultDisplay().getRotation();
        if (mSensorManager != null && mAccelerometer != null) {
            mSensorManager.registerListener(this, mAccelerometer, SensorManager.SENSOR_DELAY_GAME);
        }
        hideSystemUi();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (mSensorManager != null) {
            mSensorManager.unregisterListener(this);
        }
        sTiltX = 0.0f;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUi();
    }

    private void hideSystemUi() {
        View decor = getWindow().getDecorView();
        decor.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
    }

    // ---- Accelerometer -> screen-space tilt ---------------------------------

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() != Sensor.TYPE_ACCELEROMETER) return;
        // Remap device axes to screen axes. Reading along the screen-right
        // axis is negative when that edge dips down, so negate to get
        // "positive = right edge down = move right".
        float alongScreenRight;
        switch (sRotation) {
            case Surface.ROTATION_90:  alongScreenRight = -event.values[1]; break;
            case Surface.ROTATION_180: alongScreenRight = -event.values[0]; break;
            case Surface.ROTATION_270: alongScreenRight =  event.values[1]; break;
            case Surface.ROTATION_0:
            default:                   alongScreenRight =  event.values[0]; break;
        }
        float t = -alongScreenRight / TILT_FULL_SCALE;
        if (t >  1.0f) t =  1.0f;
        if (t < -1.0f) t = -1.0f;
        // Small dead zone so a phone lying flat doesn't drift the paddle.
        if (Math.abs(t) < 0.06f) t = 0.0f;
        sTiltX = t;
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) { }
}
