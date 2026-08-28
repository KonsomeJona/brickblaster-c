cmake_minimum_required(VERSION 3.16)
project(brickblaster VERSION 0.2.3 LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_definitions(BRICKBLASTER_VERSION="${PROJECT_VERSION}")

# TakoHi publisher branding (logo + URL at end of intro).
# Default ON for distributed builds; set to OFF for a 100% vanilla port.
option(TAKOHI_BRANDING "Show TakoHi publisher slide at end of intro" ON)

# F12 GIF recorder — non-original debug tool for README demo GIFs.
# Default ON; set to OFF for a 100% vanilla build.
option(GIF_RECORDER "F12 in-game GIF screen recorder" ON)

# -------------------------------------------------------------------------
# Raylib
# -------------------------------------------------------------------------
include(FetchContent)

# Web (Emscripten): raylib's own CMake selects its backend from the PLATFORM
# variable — "Web" builds against OpenGL ES 2.0 / WebGL through GLFW's
# emscripten shim. Must be set BEFORE FetchContent_MakeAvailable(raylib).
# Configure with:  emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
if(EMSCRIPTEN)
    set(PLATFORM "Web" CACHE STRING "" FORCE)
endif()
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# Custom frame control: EndDrawing() only flushes GPU — we manually call
# PollInputEvents(), SwapScreenBuffer(), and handle frame pacing.
target_compile_definitions(raylib PUBLIC SUPPORT_CUSTOM_FRAME_CONTROL)

if(TAKOHI_BRANDING)
    add_compile_definitions(TAKOHI_BRANDING=1)
endif()
if(GIF_RECORDER)
    add_compile_definitions(GIF_RECORDER=1)
endif()

# -------------------------------------------------------------------------
# Game executable
# -------------------------------------------------------------------------
add_executable(brickblaster
    src/main.c
    src/asm_random.c
    src/game.c
    src/draw.c
    src/ball.c
    src/paddle.c
    src/brick.c
    src/powerup.c
    src/font.c
    src/hiscore.c
    src/level.c
    src/audio.c
    src/demo.c
    src/assets.c
    src/collision.c
    src/animation.c
    src/music_manager.c
    src/screen_manager.c
    src/screen_menu.c
    src/screen_intro.c
    src/screen_intro_original.c
    src/screen_hiscore.c
    src/screen_credits.c
    src/screen_overlays.c
    src/screen_final.c
    src/monster.c
    src/input_frame.c
    src/i18n.c
    src/settings.c
    $<$<BOOL:${GIF_RECORDER}>:src/gif_recorder.c>
    src/screen_editor.c
    $<$<PLATFORM_ID:Windows>:src/brickblaster.rc>
)
target_link_libraries(brickblaster raylib)
if(MSVC)
    target_link_libraries(brickblaster Shell32)
endif()
target_include_directories(brickblaster PRIVATE src)

# -------------------------------------------------------------------------
# Platform-specific configuration
# -------------------------------------------------------------------------
if(EMSCRIPTEN)
    # Directory preloaded into the Emscripten virtual FS as /assets — the game
    # loads everything relative to the CWD ("/" on web) via ASSETS_BASE
    # "assets/", so the @assets mapping reproduces the native layout exactly.
    # CI points this at a staged copy where the 82 MB of WAV music has been
    # converted to OGG (see .github/workflows/web.yml); a plain local emcmake
    # build defaults to the repo assets and still works, just heavier.
    set(WEB_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets" CACHE PATH
        "Assets directory preloaded into the web bundle as /assets")

    # main.c already branches on PLATFORM_WEB (extracted UpdateDrawFrame +
    # emscripten_set_main_loop); raylib only defines it for its own target.
    target_compile_definitions(brickblaster PRIVATE PLATFORM_WEB)

    # Emit brickblaster.html (+ .js/.wasm/.data) through our shell page.
    set_target_properties(brickblaster PROPERTIES
        SUFFIX ".html"
        LINK_DEPENDS "${CMAKE_SOURCE_DIR}/web/shell.html")

    # No ASYNCIFY: the main loop is handed to emscripten_set_main_loop with
    # simulate_infinite_loop=1, so main() unwinds cleanly to the browser —
    # nothing blocks. Audio uses miniaudio's WebAudio backend (no threads,
    # no SharedArrayBuffer, so no COOP/COEP headers needed on the host).
    target_link_options(brickblaster PRIVATE
        "-sUSE_GLFW=3"                # raylib web backend is GLFW-on-emscripten
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=134217728"  # 128 MB: MEMFS bundle (~20 MB) + decoded
                                      # textures/audio + game state, no early growth
        "-sSTACK_SIZE=1048576"
        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
        "SHELL:--preload-file ${WEB_ASSETS_DIR}@assets"
        # data/ ships blaster.cfg/.scr/.usr defaults; hiscore/settings writes
        # land in MEMFS (lost on reload — no IDBFS persistence wired yet).
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/data@data"
    )
endif()

if(MSVC)
    set_target_properties(brickblaster PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(brickblaster PRIVATE /ENTRY:mainCRTStartup)
    target_compile_options(brickblaster PRIVATE /wd4819)
    # Copy assets next to exe after build
    add_custom_command(TARGET brickblaster POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:brickblaster>/assets
        COMMENT "Copying assets"
    )
endif()

# -------------------------------------------------------------------------
# Instrumental parity tests — headless, no window, no assets beyond levels.
#
# The rule layer (game/ball/brick/collision/powerup/monster/paddle/asm_random)
# makes no raylib calls, and audio is skipped when Game.audio is NULL, so the
# whole behaviour set can be driven from a plain main(). Off by default so the
# normal build is untouched:
#     cmake -B build -DBUILD_TESTS=ON && cmake --build build --target bb_tests
#     (cd <repo root> && ./build/bb_tests)
# -------------------------------------------------------------------------
option(BUILD_TESTS "Build the headless parity tests" OFF)

if(BUILD_TESTS)
    add_executable(bb_tests
        tests/test_parity.c
        src/game.c src/ball.c src/brick.c src/collision.c src/powerup.c
        src/monster.c src/paddle.c src/level.c src/asm_random.c src/demo.c
        src/audio.c src/hiscore.c src/settings.c src/font.c src/i18n.c
    )
    target_include_directories(bb_tests PRIVATE src)
    target_link_libraries(bb_tests raylib)
    enable_testing()
    add_test(NAME parity
             COMMAND bb_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
