# BrickBlaster — keep JNI-called methods in the Activity
-keep class com.takohi.brickblaster.BrickBlasterActivity {
    public static float jniConsumeRotaryDelta();
    public static boolean jniConsumeCrownPress();
}
