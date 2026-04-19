cmake_minimum_required(VERSION 3.16)
project(brickblaster C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

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
