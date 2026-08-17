# BrickBlaster — Android native build (NDK / Gradle externalNativeBuild).
#
# WHY THIS FILE EXISTS: the root CMakeLists.txt builds a desktop EXECUTABLE
# and has no Android branch. Android needs a SHARED library loaded by
# NativeActivity, so this wrapper builds the same sources (plus the mobile
# input files) as libbrickblaster.so against raylib's PLATFORM_ANDROID.
#
# KEEP IN SYNC with the root CMakeLists.txt source list. Once the root file
# gains an `if(ANDROID)` branch, this wrapper can be deleted and
# app/build.gradle pointed at the root CMakeLists.txt instead.

cmake_minimum_required(VERSION 3.22)
project(brickblaster-android LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Repo root: android/app/src/main/cpp -> ../../../../..
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

# Version: parse it from the root CMakeLists.txt so the two never drift.
file(STRINGS "${REPO_ROOT}/CMakeLists.txt" _bb_project_line
     REGEX "project\\(brickblaster VERSION")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" BB_VERSION "${_bb_project_line}")
if(NOT BB_VERSION)
    set(BB_VERSION "0.0.0")
endif()
add_compile_definitions(BRICKBLASTER_VERSION="${BB_VERSION}")

# TakoHi publisher branding (same default as desktop).
add_compile_definitions(TAKOHI_BRANDING=1)
# This file list always includes gif_recorder.c, so declare it the way the root
# CMakeLists does — gif_recorder.h serves inline no-op stubs when the macro is
# absent, and compiling the real implementation against those stubs is a
# redefinition error.
add_compile_definitions(GIF_RECORDER=1)
# Phone build: touch buttons + drag + accelerometer tilt (src/mobile_controls.c,
# src/input_tilt.c). PLATFORM_ANDROID alone (without this) is the Wear OS mode.
add_compile_definitions(BRICKBLASTER_MOBILE=1)

# -------------------------------------------------------------------------
# Raylib — same tag as the desktop build. raylib's own CMake handles
# PLATFORM=Android: it compiles the NDK's android_native_app_glue into the
# lib, defines PLATFORM_ANDROID, and links log/android/EGL/GLESv2/OpenSLES.
# rcore_android.c provides android_main() (which calls our main()) and
# GetAndroidApp() (used by src/input_wear.c / src/input_tilt.c / src/i18n.c).
# -------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
set(PLATFORM "Android" CACHE STRING "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# Custom frame control: main.c calls PollInputEvents()/SwapScreenBuffer()
# itself (same contract as the desktop build).
target_compile_definitions(raylib PUBLIC SUPPORT_CUSTOM_FRAME_CONTROL)

# raylib 5.0's bundled stb_image_resize2.h auto-enables NEON fp16 intrinsics
# that armv7 (no +fp16) doesn't have -> compile error on armeabi-v7a
# (raysan5/raylib#3670). Scalar path is plenty for a 640x480 game.
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    target_compile_definitions(raylib PRIVATE STBIR_NO_SIMD)
endif()

# -------------------------------------------------------------------------
# Game shared library — name must match android.app.lib_name in the manifest.
# -------------------------------------------------------------------------
set(SRC "${REPO_ROOT}/src")
add_library(brickblaster SHARED
    ${SRC}/main.c
    ${SRC}/asm_random.c
    ${SRC}/game.c
    ${SRC}/draw.c
    ${SRC}/ball.c
    ${SRC}/paddle.c
    ${SRC}/brick.c
    ${SRC}/powerup.c
    ${SRC}/font.c
    ${SRC}/hiscore.c
    ${SRC}/level.c
    ${SRC}/audio.c
    ${SRC}/demo.c
    ${SRC}/assets.c
    ${SRC}/collision.c
    ${SRC}/animation.c
    ${SRC}/music_manager.c
    ${SRC}/screen_manager.c
    ${SRC}/screen_menu.c
    ${SRC}/screen_intro.c
    ${SRC}/screen_intro_original.c
    ${SRC}/screen_hiscore.c
    ${SRC}/screen_credits.c
    ${SRC}/screen_overlays.c
    ${SRC}/screen_final.c
    ${SRC}/monster.c
    ${SRC}/input_frame.c
    ${SRC}/i18n.c
    ${SRC}/settings.c
    ${SRC}/gif_recorder.c
    ${SRC}/screen_editor.c
    # Android-only input glue (not in the root/desktop source list)
    ${SRC}/input_tilt.c
    ${SRC}/input_wear.c
    ${SRC}/mobile_controls.c
    # Haptics: game.c calls platform_vibrate()/platform_vibrate_pattern()
    # under BRICKBLASTER_MOBILE but src/ neither declares nor defines them
    # (they came from the author's private overlay). Implementation lives
    # here; the prototypes are force-included below so src/ stays untouched.
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_vibrate_android.c
)
target_include_directories(brickblaster PRIVATE ${SRC})
target_compile_options(brickblaster PRIVATE
    "-include" "${CMAKE_CURRENT_SOURCE_DIR}/platform_vibrate.h")
target_link_libraries(brickblaster raylib android log)

# NativeActivity loads the .so and looks up ANativeActivity_onCreate, which
# lives in raylib's copy of android_native_app_glue. Nothing references it at
# link time, so force the linker to keep/export it.
target_link_options(brickblaster PRIVATE "-u" "ANativeActivity_onCreate")
