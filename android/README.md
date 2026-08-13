# BrickBlaster — Android target

Status: **builds from a fresh clone** (debug APK). The Gradle project is
self-contained: `./gradlew assembleDebug` produces a debug-signed APK with
no secrets or keystores required. Runtime on real devices is still young —
see "Honest caveats" below.

## Layout

| Path | Purpose |
|---|---|
| `build.gradle` / `settings.gradle` | Root project (Android Gradle Plugin 8.5.2) |
| `app/build.gradle` | App module: native build wiring + asset staging |
| `app/src/main/cpp/CMakeLists.txt` | Android native build — raylib 5.0 `PLATFORM_ANDROID` + game sources as `libbrickblaster.so`. **Keep the source list in sync with the root `CMakeLists.txt`** (the root file has no Android branch yet; when it gets one, this wrapper can be deleted) |
| `app/src/main/AndroidManifest.xml` | `BrickBlasterActivity` (NativeActivity subclass), landscape, fullscreen |
| `app/src/main/java/.../BrickBlasterActivity.java` | JNI bridge expected by `src/input_tilt.c` / `src/input_wear.c`: `jniConsumeTiltX` (accelerometer), crown methods stubbed for phones |
| `brickblaster-release.keystore` / `keystore.properties` | **Local only, gitignored.** Release signing activates automatically when present; absent on clones and in CI |

## Build

```bash
cd android
./gradlew assembleDebug
# -> app/build/outputs/apk/debug/app-debug.apk
```

Requirements: JDK 17, Android SDK with NDK `26.1.10909125` and CMake
`3.22.1` (Gradle installs them via the SDK manager if licenses are
accepted). First build fetches raylib 5.0 from GitHub (FetchContent).

ABIs: `arm64-v8a`, `armeabi-v7a`, `x86_64` (emulator).

## Assets & music

Assets are **staged** at build time (`stageAssets` task): the repo's
`assets/` tree is copied into `app/build/stagedAssets/` and packaged from
there — the repo assets are never modified. If `ffmpeg` is on the PATH,
the 82 MB of `music/*.wav` are converted to OGG (`-qscale:a 5`) during
staging; without ffmpeg the WAVs are copied as-is and the APK is ~100 MB
instead of ~30 MB. CI installs ffmpeg, so published artifacts always ship
OGG.

Note: `music_manager.c` references `music/*.wav` paths; the `.wav`→`.ogg`
extension fallback lives in `music_manager.c`/`audio.c`. An OGG-only APK
built before that fallback boots fine but plays no music.

## CI

`.github/workflows/android.yml` builds the debug APK on every push/PR to
`main`, on `v*` tags, and on manual dispatch, and uploads it as the
`brickblaster-android-debug` artifact (7-day retention).

## Honest caveats (not yet verified on-device)

- The APK **compiles and packages**; it has not yet been soak-tested on a
  physical phone. Lifecycle (home/back/rotate), audio focus, and pause
  behaviour are untested.
- High scores and settings (`data/blaster.scr`, `data/blaster.cfg`,
  `data/blaster.usr`) are opened with plain `fopen` relative paths — on
  Android these files won't resolve, so scores/settings will not persist
  until the paths are routed to the app's internal storage.
- Touch controls (`src/mobile_controls.c`), drag, and accelerometer tilt
  are wired but untuned on real hardware.

## Desktop first

If you're just trying to play or build BrickBlaster, use the desktop
targets documented in the root [README.md](../README.md) (CMake + raylib,
builds on Windows / macOS / Linux).
