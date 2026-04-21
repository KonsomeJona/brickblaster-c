# BrickBlaster — Android target

Status: **experimental / work in progress**. The Gradle project scaffold
is present (`build.gradle`, `settings.gradle`, `gradlew*`) but the app
module (`AndroidManifest.xml`, native build wiring, icons, touch-input
glue) is not yet committed in a self-contained way. Builds currently
happen from a private dev environment; getting a clean fresh-clone
build is on the polish list.

## What's here

| File | Purpose |
|---|---|
| `build.gradle` | Root project config (Android Gradle Plugin 8.5.2) |
| `settings.gradle` | Includes the `:app` module |
| `gradle.properties` | AndroidX enabled, JVM 2 GB for Gradle |
| `gradlew` / `gradlew.bat` | Gradle wrapper |
| `app/proguard-rules.pro` | Release shrinker rules |
| `brickblaster-release.keystore` | Release signing keystore |
| `keystore.properties` | Keystore credentials (not committed to public builds) |

## What's still missing to make this reproducible

- `app/build.gradle` with `externalNativeBuild { cmake { path ... } }`
  pointing at the root `CMakeLists.txt`.
- `app/src/main/AndroidManifest.xml` declaring the NativeActivity entry.
- Root `CMakeLists.txt` Android branch (the current file targets
  Desktop only — raylib's Android path is not wired up yet).
- Touch input + virtual keys mapping to `FrameInput` (partially done
  in `src/input_frame.c` behind `BRICKBLASTER_MOBILE`, needs JNI glue).

## How builds happen today

The author builds APKs against raylib's `PLATFORM_ANDROID` using a
local NDK (`r26d`) + Gradle wrapper, with source overlays not yet
committed. Until that scaffolding is published, a clean `./gradlew
assembleRelease` from a fresh clone **will not produce a working APK**.

If you want to help port the Android target properly, open an issue on
the main repo: https://github.com/KonsomeJona/brickblaster-c/issues

## Desktop first

If you're just trying to play or build BrickBlaster, use the desktop
targets documented in the root [README.md](../README.md) (CMake +
raylib, builds on Windows / macOS / Linux).
