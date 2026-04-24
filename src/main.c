/* main.c — BrickBlaster raylib port entry point
 *
 * Wires together: window, assets, game state machine, all screens, music.
 *
 * State machine (mirrors MAIN.ASM:60-128  intro: / @@play / @@demo):
 *   STATE_INTRO              → intro animation (36 frames)
 *   STATE_TITLE              → title screen
 *   STATE_MENU               → 7-part menu system
 *   STATE_HISCORE            → high scores display and name entry
 *   STATE_CREDITS            → 5-slide credits
 *   STATE_READY_TO_PLAY      → ready overlay (ball on paddle, waiting for fire)
 *   STATE_READY_TO_PLAY_AGAIN→ ready overlay after losing a life
 *   STATE_PLAYING            → gameplay
 *   STATE_PAUSED             → pause overlay
 *   STATE_GAME_OVER          → game over overlay then → menu
 *   STATE_NEW_PLAY           → level advance (handled in main loop)
 *   STATE_FINAL              → victory animation
 *
 * Window: 640x480 (original game canvas).
 *
 * Web build (PLATFORM_WEB / Emscripten):
 *   The main loop body is extracted into UpdateDrawFrame() so that
 *   emscripten_set_main_loop can drive the frame callback.  All state
 *   is file-scope static so UpdateDrawFrame can access it without parameters.
 */
#include "raylib.h"
#include "rlgl.h"
#include "constants.h"
#include "screen_manager.h"
#include "screen_intro.h"
#include "screen_intro_original.h"
#include "screen_menu.h"
#include "screen_hiscore.h"
#include "screen_credits.h"
#include "screen_overlays.h"
#include "screen_final.h"
#include "music_manager.h"
#include "hiscore.h"
#include "game.h"
#include "draw.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "assets.h"
#include "audio.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif
#if defined(PLATFORM_ANDROID)
    #include "input_wear.h"
    /* GetAndroidApp() is defined in raylib's rcore_android.c but not in any header (raylib 5.5).
     * Forward-declare it here so we can call it after InitWindow(). */
    struct android_app;
    extern struct android_app *GetAndroidApp(void);
#endif
#if defined(BRICKBLASTER_MOBILE)
    #include "input_tilt.h"
#endif
#if defined(BRICKBLASTER_MOBILE)
    #include "mobile_controls.h"
#endif
#include "input_gamepad.h"
#include "letterbox.h"
#include "i18n.h"
#include "gif_recorder.h"
#include "screen_editor.h"
extern void editor_bind_assets(Assets *a);
#include "input_frame.h"
#include "demo.h"
#include "settings.h"
#include <stdio.h>

/* -----------------------------------------------------------------------
 * File-scope state — accessible from UpdateDrawFrame() for web builds.
 * On native builds these are still used the same way, just not as locals.
 * ----------------------------------------------------------------------- */
static ScreenState       state;
static Assets            assets;
static AudioState        audio;
static DrawContext       dc;
static Game              game;
static int               game_initialized = 0;

static IntroAssets          intro;
static IntroOriginalAssets  intro_original;
static MenuAssets           menu;
static HiscoreScreenState hiscore_screen;
static CreditsAssets      credits;
static FinalAssets        final_screen;
static EditorState        editor_state;

static MusicManager       music;
static Hiscores           hiscores;
static GameConfig         cfg;             /* P1-ASM-23: Blaster.cfg overrides. */

static int                game_over_timer = 0;
/* Frame counter for the "play again" overlay after losing a life.
 * Counts down from PLAY_AGAIN_FRAMES when STATE_READY_TO_PLAY_AGAIN is
 * first entered; while >0 we render option_text_again, then fall through
 * to the normal ready overlay (MAIN.ASM:4695-4705 shows "play again" then
 * "ready" back-to-back after restart). */
#define PLAY_AGAIN_FRAMES 60
static int                play_again_timer = 0;
static int                prev_ready_state = 0; /* edge-detect entry into _AGAIN */
static FrameInput         fi;  /* polled once per frame, passed to game_update */

/* On native desktop we use custom frame control: PollInputEvents() at the top
 * of the loop, SwapScreenBuffer() at the bottom.  EndDrawing() must NOT be
 * called because it polls+swaps a second time, causing double-click bugs.
 * On web/Android we keep EndDrawing() because the platform loop expects it. */
#if !defined(PLATFORM_WEB) && !defined(PLATFORM_ANDROID)
#define FINISH_DRAWING() rlDrawRenderBatchActive()
#else
#define FINISH_DRAWING() EndDrawing()
#endif


/* Pause cooldown prevents instant unpause from same input that paused. */
#define PAUSE_COOLDOWN_FRAMES 20
static int pause_cooldown = 0;

/* -----------------------------------------------------------------------
 * UpdateDrawFrame — one iteration of the main loop.
 * Called each frame by emscripten_set_main_loop (web) or the while loop (native).
 * ----------------------------------------------------------------------- */
static int prev_screen_w = 0, prev_screen_h = 0;

static void UpdateDrawFrame(void) {

#if defined(PLATFORM_WEB)
    /* Web: poll + swap inside the callback (browser handles frame pacing) */
    PollInputEvents();
#endif

#if defined(BRICKBLASTER_MOBILE)
    /* Foldable cover screen or unexpected portrait layout — pause everything
     * and show an "unfold" message. Resumes automatically when unfolded. */
    if (is_screen_too_small()) {
        BeginDrawing();
        draw_unfold_screen();
        FINISH_DRAWING();
        return;
    }

    /* Detect rotation / fold / unfold: screen dimensions changed.
     * Fully reinitialise canvas and blur render textures + GL viewport
     * so the game renders correctly at the new screen size. */
    {
        int cur_w = GetScreenWidth();
        int cur_h = GetScreenHeight();
        if (prev_screen_w != 0 && prev_screen_h != 0 &&
            (cur_w != prev_screen_w || cur_h != prev_screen_h)) {
            /* Screen size changed — pause if playing */
            if (state.game_mode == STATE_PLAYING) {
                game.state      = STATE_PAUSED;
                state.game_mode = STATE_PAUSED;
                pause_cooldown  = PAUSE_COOLDOWN_FRAMES;
            }
            /* Recreate all render textures (canvas + blur) from scratch.
             * This resets GPU state as if it were the first init. */
            draw_reinit_textures(&dc);
            /* Force GL viewport to match the new screen dimensions.
             * BeginDrawing() uses cached values that may be stale. */
            rlViewport(0, 0, cur_w, cur_h);
            SetWindowSize(cur_w, cur_h);
        }
        prev_screen_w = cur_w;
        prev_screen_h = cur_h;
    }

    /* Update touch button state before any input reading. */
    mobile_controls_update();
#endif

    /* Sync audio enable flags from screen state every frame */
    audio.sfx_enabled = state.sfx_enabled;
    static int prev_music_enabled = 1;
    if (state.music_enabled != prev_music_enabled) {
        music_manager_set_enabled(&music, state.music_enabled);
        prev_music_enabled = state.music_enabled;
    }

    /* Update music based on current state (BEFORE music_manager_update) */
    screen_update_music(&state, &music);

    /* Update streaming music */
    music_manager_update(&music);

    /* Poll all input sources into a single platform-agnostic struct.
     * Must be called exactly once per frame, before game_update().
     * in_game flag: mouse clicks are only treated as fire during gameplay.
     * During menus, mouse input is handled via fi.click_pressed. */
    {
        int in_game = (state.game_mode == STATE_PLAYING ||
                       state.game_mode == STATE_PAUSED ||
                       state.game_mode == STATE_READY_TO_PLAY ||
                       state.game_mode == STATE_READY_TO_PLAY_AGAIN);
        frame_input_poll(&fi, state.drag_enabled, state.tilt_enabled,
                         state.button_speed, state.tilt_speed,
                         0, in_game);

#if defined(UWP_BUILD)
        /* DIAGNOSTIC: every 60 frames during PLAYING, dump input + paddle
         * state so the "paddle freezes after ~5 min" bug can be diagnosed.
         * Remove once tracked down. */
        if (game_initialized && state.game_mode == STATE_PLAYING
            && (game.frame % 60) == 0) {
            FILE *_pd = fopen("D:\\brickblaster-paddle.log", "a");
            if (_pd) {
                fprintf(_pd,
                    "f=%d s=%d demo=%d padx=%d ptract=%d ptrx=%.1f "
                    "mL=%d mR=%d stk=%.2f rev=%d sz_tmr=%d demo_tmr=%d\n",
                    game.frame, game.state, game.demo_active,
                    game.paddle.x, fi.pointer_active, (double)fi.pointer_game_x,
                    fi.move_left, fi.move_right, (double)fi.stick_x,
                    game.paddle.reversed, game.paddle.size_timer,
                    game.demo_timer);
                fclose(_pd);
            }
        }
#endif
        /* Poll P2 input when multiplayer active. */
        if (state.nbs_player > 1) {
            frame_input_poll_p2(&fi, state.control_p2);
            /* Computer-controlled P2: inject AI inputs into the shared
             * FrameInput so game.c's existing P2 pipeline (paddle_2 move,
             * laser fire, magnetic launch) runs unchanged.
             * ASM ref: MAIN.ASM:5131-5144 demo_move_x_player_1 (P2 twin). */
            if (state.control_p2 == 0 && game_initialized)
                demo_ai_player_2(&game, &fi);
        }
        /* Pause: P key, ESC, gamepad Start — no on-screen button (original). */
    }

    /* -----------------------------------------------------------
     * STATE_NEW_PLAY: level advance — handled before draw dispatch
     * MAIN.ASM:4906-4927  new_play: inc current_level; call load_level
     * ----------------------------------------------------------- */
    /* F12 toggles GIF recording (global) */
    if (IsKeyPressed(KEY_F12)) gif_recorder_toggle();

    if (state.game_mode == STATE_NEW_PLAY && game_initialized) {
        game.level_num++;
        if (game.level_num > LEVELS_PER_FILE) {
            /* All 80 levels done — victory! */
            state.game_mode = STATE_FINAL;
        } else {
            game_load_level(&game, game.level_num);
            game_spawn_ball(&game);
            game.state      = STATE_READY_TO_PLAY;
            state.game_mode = STATE_READY_TO_PLAY;
        }
    }

    /* State machine */
    switch (state.game_mode) {

    /* ---------------------------------------------------------------
     * STATE_INTRO: Intro animation
     * MAIN.ASM:176-188
     * --------------------------------------------------------------- */
    case STATE_INTRO:
        intro_assets_load(&intro);
        intro_update(&state, &intro, &fi);
        BeginTextureMode(dc.canvas);
        intro_draw(&intro);
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_canvas_to_screen(&dc, &game);
        FINISH_DRAWING();
        break;

    /* ---------------------------------------------------------------
     * STATE_INTRO_ORIGINAL: 36-frame intro.flc + 6 credit GIFs
     * MAIN.ASM:53-190 display_intro / FILE.ASM:54-114
     * --------------------------------------------------------------- */
    case STATE_INTRO_ORIGINAL:
        intro_original_assets_load(&intro_original);
        intro_original_update(&state, &intro_original, &fi);
        BeginTextureMode(dc.canvas);
        intro_original_draw(&intro_original);
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_canvas_to_screen(&dc, &game);
        FINISH_DRAWING();
        break;

    /* ---------------------------------------------------------------
     * STATE_TITLE: Skip directly to menu (title screen removed)
     * --------------------------------------------------------------- */
    case STATE_TITLE:
        state.current_menu = 1;
        state.game_mode = STATE_MENU;
        break;

    /* ---------------------------------------------------------------
     * STATE_MENU: 7-part menu system
     * MAIN.ASM:221-444
     * --------------------------------------------------------------- */
    case STATE_MENU:
        game_initialized = 0;   /* reset on return to menu */
        /* P1-ASM-18: clear demo_flag whenever we leave the play flow back
         * to the menu — otherwise a demo that ended on game-over would
         * leak the flag into a subsequent real play session. */
        state.demo_flag  = 0;
        prev_ready_state = 0;   /* reset play-again edge detector */
        play_again_timer = 0;
        menu_handle_input(&state, &menu, &audio, &fi);
        BeginTextureMode(dc.canvas);
        menu_draw(&state, &menu);
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_canvas_to_screen(&dc, &game);
        FINISH_DRAWING();
        break;

    /* ---------------------------------------------------------------
     * STATE_HISCORE: High score display and name entry
     * HISCORE.ASM:178-212, 278-344
     * --------------------------------------------------------------- */
    case STATE_HISCORE:
        hiscore_screen_update(&state, &hiscore_screen, &hiscores, &fi);
        BeginTextureMode(dc.canvas);
        ClearBackground(BLACK);
        hiscore_screen_draw(&hiscore_screen, &hiscores, state.hiscore_mode);
#if defined(BRICKBLASTER_MOBILE)
        if (hiscore_screen.name_entry_active) mobile_controls_draw("OK", 1);
#endif
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_canvas_to_screen(&dc, &game);
        FINISH_DRAWING();
        break;

    /* ---------------------------------------------------------------
     * STATE_CREDITS: Credits slideshow
     * MAIN.ASM:139-203
     * --------------------------------------------------------------- */
    case STATE_CREDITS:
        credits_update(&state, &credits, &fi);
        BeginTextureMode(dc.canvas);
        credits_draw(&credits);
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_canvas_to_screen(&dc, &game);
        FINISH_DRAWING();
        break;

    /* ---------------------------------------------------------------
     * STATE_EDIT: Level editor (EDITOR.ASM)
     * --------------------------------------------------------------- */
    case STATE_EDIT:
        if (!editor_state.loaded) editor_init(&editor_state);
        editor_update(&state, &editor_state, &fi);
        BeginTextureMode(dc.canvas);
        editor_draw(&editor_state);
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_canvas_to_screen(&dc, &game);
        FINISH_DRAWING();
        break;

    /* ---------------------------------------------------------------
     * STATE_FINAL: Victory animation
     * FILE.ASM:118-183
     * --------------------------------------------------------------- */
    case STATE_FINAL:
        final_assets_load(&final_screen);   /* lazy: loads 384 frames on first entry */
        final_update(&state, &final_screen, &fi);
        BeginDrawing();
        ClearBackground(BLACK);
        final_draw(&final_screen);
        /* P1-ASM-21/22: after anim, overlay the final_text (solo/coop)
         * or final_dual (duel "winner is ...") modal until the user
         * clicks or 5 s elapse. */
        final_draw_modal(&final_screen, &state, &game);
        FINISH_DRAWING();
        break;

    /* ---------------------------------------------------------------
     * STATE_READY_TO_PLAY / STATE_READY_TO_PLAY_AGAIN
     * Ball sits on paddle; player fires with SPACE or mouse button.
     * MAIN.ASM:1081  call detect_start_game
     * MAIN.ASM:1261, 2757
     * --------------------------------------------------------------- */
    case STATE_READY_TO_PLAY:
    case STATE_READY_TO_PLAY_AGAIN:
        /* ESC bails out of the session back to the main menu — modern UX
         * convention (not in ASM, which only listened for fire on this
         * overlay). Checked before spawn/draw so a stray press doesn't
         * initialise a game that the user immediately abandons. */
        if (IsKeyPressed(KEY_ESCAPE)) {
            state.game_mode    = STATE_MENU;
            state.current_menu = 1;
            game.state         = STATE_MENU;
            game_initialized   = 0;
            break;
        }
        /* P1-ASM-17: MAIN.ASM:4695-4705 fires option_text_again then
         * option_text_ready back-to-back on respawn. Start the timer when
         * we transition into _AGAIN (prev_ready_state tracks the previous
         * game.state across all frames; see the update below). */
        if (state.game_mode == STATE_READY_TO_PLAY_AGAIN &&
            prev_ready_state != STATE_READY_TO_PLAY_AGAIN) {
            play_again_timer = PLAY_AGAIN_FRAMES;
        }
        /* Initialize game on first entry from menu */
        if (!game_initialized) {
            /* Re-seed rand() at game start using high-res timer.
             * MAIN.ASM uses CPU timer ticks so each new game is different.
             * time() only has 1s resolution; GetTime() gives sub-ms precision. */
            srand((unsigned int)(GetTime() * 1000000.0));
            Difficulty diff = (Difficulty)state.difficulte;
            /* game_mode: 0=solo, 1=coop, 2=dual (versus) — MAIN.ASM cfg. */
            int gmode = 0;
            if (state.nbs_player > 1) gmode = state.dual_flag ? 2 : 1;
            game_init(&game, &assets, &audio, diff, gmode);
            game.world = state.world;
            game.control_p2 = state.control_p2;
            /* P1-ASM-34: inject cfg-derived per-difficulty spawn spacing. */
            game_set_powerup_spacing(&game, cfg.delai_between_option);
            /* F3 P1-ASM-36: inject cfg-derived per-difficulty monster delay. */
            game_set_monster_delai(&game, cfg.monster_delai);
            /* F6-01: inject cfg-derived per-powerup per-difficulty spawn freqs.
             * ASM cite: FILE.ASM:965-973 overwrites struc_options option_easy/
             * medium/hard at cfg load — random_options (MAIN.ASM:5493-5504)
             * then reads the updated values. */
            game_set_powerup_freq(&game, cfg.freq_option);
            /* Demo mode: transfer flag so game AI drives the paddle */
            if (state.demo_flag) game.demo_active = 1;
            /* Dev test mode: load test level with all brick types */
            if (state.dev_test) {
                game.dev_test = 1;
                game.dev_powerup_cycle = 0;
                game.lives = BALL_MAX;
                state.dev_test = 0;  /* consumed */
            }
            game_load_level(&game, 1);
            game_spawn_ball(&game);
            game_initialized = 1;
        }

        game_update(&game, &fi);
        /* Sync: fire transitions game to STATE_PLAYING */
        state.game_mode = game.state;

        draw_frame_to_canvas(&dc, &game);
        BeginTextureMode(dc.canvas);
        if (play_again_timer > 0) {
            draw_play_again_screen(&state);
            play_again_timer--;
        } else {
            draw_ready_screen(&state);
        }
#if defined(BRICKBLASTER_MOBILE)
        mobile_controls_draw("FIRE", 1);
#endif
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_canvas_to_screen(&dc, &game);
        FINISH_DRAWING();
        break;

    /* ---------------------------------------------------------------
     * STATE_PLAYING: Main gameplay
     * MAIN.ASM:1061-1175
     * --------------------------------------------------------------- */
    case STATE_PLAYING: {
        /* Pause: P key, gamepad Start, or pause button. */
        if (fi.pause_pressed) {
            game.state      = STATE_PAUSED;
            state.game_mode = STATE_PAUSED;
            pause_cooldown  = PAUSE_COOLDOWN_FRAMES;
            break;
        }

        game_update(&game, &fi);
        /* Sync game state -> screen state */
        state.game_mode = game.state;

        draw_frame_to_canvas(&dc, &game);
        BeginTextureMode(dc.canvas);
        if (state.demo_flag) draw_demo_overlay();
#if defined(BRICKBLASTER_MOBILE)
        /* Fire enabled only when ball is on paddle or gun is active */
        mobile_controls_draw("FIRE",
            game.paddle.has_gun ||
            game.state == STATE_READY_TO_PLAY ||
            game.state == STATE_READY_TO_PLAY_AGAIN);
        /* Hint text removed — control mode help is in pause menu */
#endif
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_canvas_to_screen(&dc, &game);
        /* Original: no on-screen pause button */
        FINISH_DRAWING();
        break;
    }

    /* ---------------------------------------------------------------
     * STATE_PAUSED: Pause overlay
     * MAIN.ASM:1257-1276
     * --------------------------------------------------------------- */
    case STATE_PAUSED: {
        /* Cooldown: ignore all unpause input for the first N frames after pausing.
         * Prevents the same tap that triggered pause from immediately unpausing. */
        /* Handle music/sfx toggle + resume/exit button taps every frame */
        int resume_pressed = pause_handle_input(&state, &fi);
        /* EXIT may have changed state to STATE_MENU — don't override it */
        if (state.game_mode != STATE_PAUSED) break;

        /* ESC from a paused game exits to the main menu — desktop UX
         * convention (David feedback, 2026-04-21). Checked before the
         * generic "any key resumes" block so ESC gets the priority
         * interpretation. Also matches pause_pressed logic in
         * input_frame.c where ESC toggles pause from PLAYING. */
        if (IsKeyPressed(KEY_ESCAPE)) {
            state.game_mode    = STATE_MENU;
            state.current_menu = 1;
            game.state         = STATE_MENU;
            break;
        }

        /* M / S are consumed by pause_handle_input as audio toggles and
         * must NOT trigger resume — swallow those events before the
         * generic "any key resumes" check. */
        int audio_toggle_frame = IsKeyPressed(KEY_M) || IsKeyPressed(KEY_S);

        if (pause_cooldown > 0) {
            pause_cooldown--;
        } else if (resume_pressed ||
                   (!audio_toggle_frame && GetKeyPressed() != 0) ||
                   fi.click_pressed ||
                   fi.pause_pressed ||
                   gamepad_confirm() || gamepad_back()
        ) {
            /* MAIN.ASM:1265-1271 @@wait: resume on ANY key (`cmp B [ebp+all],Off`)
             * OR on mouse click (`call read_click`). */
            game.state      = STATE_PLAYING;
            state.game_mode = STATE_PLAYING;
        }
        draw_frame_to_canvas(&dc, &game);   /* frozen game underneath */
        BeginTextureMode(dc.canvas);
        draw_pause_screen(&state);
#if defined(BRICKBLASTER_MOBILE)
        mobile_controls_draw("FIRE", 1);
#endif
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_canvas_to_screen(&dc, &game);
        /* Original: no on-screen pause button (unpause via P/ESC/Start) */
        FINISH_DRAWING();
        break;
    }

    /* ---------------------------------------------------------------
     * STATE_GAME_OVER: Game over overlay
     * MAIN.ASM:4666-4695
     * --------------------------------------------------------------- */
    case STATE_GAME_OVER:
        /* ESC short-circuits the hiscore entry and returns to menu —
         * modern UX convention (not in ASM). */
        if (IsKeyPressed(KEY_ESCAPE)) {
            game_over_timer  = 0;
            game_initialized = 0;
            state.game_mode  = STATE_MENU;
            input_wait_click_release();
            break;
        }
        game_over_timer++;
        draw_frame_to_canvas(&dc, &game);   /* frozen game underneath */
        BeginTextureMode(dc.canvas);
        {
            int winner = -1;
            if (game.game_mode == 2) {
                if (game.lives < 0 && game.lives_2 >= 0) winner = 1;
                else if (game.lives_2 < 0 && game.lives >= 0) winner = 0;
            }
            draw_game_over_screen(&state, &game_over_timer,
                                  game.game_mode, winner);
        }
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        draw_canvas_to_screen(&dc, &game);
        FINISH_DRAWING();

        /* P1-ASM-33: ASM display_score is an interactive loop (wait_synchro
         * + read_click). Replace the fixed 180-frame wait with "min 30
         * frames, then skip on any input" — matches ASM semantics while
         * still giving the user a beat to register the overlay. */
        {
            int can_skip = (game_over_timer >= 30) &&
                           (GetKeyPressed() != 0 || fi.click_pressed ||
                            gamepad_confirm() || gamepad_back());
            if (!(can_skip || game_over_timer >= GAME_OVER_DELAY_FRAMES)) break;
        }
        {
            game_over_timer  = 0;
            game_initialized = 0;
            /* HISCORE.ASM:28-29  dual_flag skips hiscore entry entirely —
             * duel scores must NOT contaminate the coop leaderboard. */
            if (game.game_mode == 2) {
                state.game_mode = STATE_MENU;
                break;
            }
            /* HISCORE.ASM:216-274  detect_new_score -- check if score qualifies */
            int hs_mode = (state.nbs_player > 1) ? 1 : 0;
            /* Coop mode 1: combined score P1+P2 (dual handled by early break above). */
            int hs_score = game.score;
            if (game.game_mode == 1) hs_score = game.score + game.score_2;
            if (hiscore_qualifies(&hiscores, hs_mode, hs_score)) {
                int rank = hiscore_insert(&hiscores, hs_mode, "???????????????",
                                          hs_score, game.level_num);
                hiscore_screen.entry_rank = (rank >= 0) ? rank : 0;
                hiscore_screen.name_entry_active = 1;
                hiscore_screen.name_entry_pos = 0;
                /* HISCORE.ASM:291-294 - Get_name fills all 15 slots with ' ' before input.
                 * letter value 26 = space. */
                for (int _li = 0; _li < HISCORE_NAME_LEN; _li++)
                    hiscore_screen.letter_values[_li] = 26;
                state.hiscore_mode = hs_mode;
                state.game_mode = STATE_HISCORE;
                /* Drain any key still held from the game-over skip press —
                 * without this, ENTER stays HELD into the hiscore screen and
                 * IsKeyPressed(KEY_ENTER) only fires on the next release+press,
                 * making the confirm button look broken. */
                input_wait_click_release();
            } else {
                state.game_mode = STATE_MENU;
            }
        }
        break;

    /* ---------------------------------------------------------------
     * STATE_NEW_PLAY: handled before switch (level advance)
     * --------------------------------------------------------------- */
    case STATE_NEW_PLAY:
        /* Handled above */
        break;

    /* ---------------------------------------------------------------
     * Unknown state
     * --------------------------------------------------------------- */
    default:
        TraceLog(LOG_ERROR, "Unknown game state: %d", state.game_mode);
        state.game_mode = STATE_MENU;
        break;
    }

    /* Track previous state for the play-again edge detector (P1-ASM-17).
     * Only the _AGAIN → _AGAIN persistence matters here; other transitions
     * are fine. */
    prev_ready_state = state.game_mode;

    /* GIF recorder: capture canvas once per frame if recording. Also draw REC
     * indicator in window coordinates so it appears over the letterbox. */
    if (gif_recorder_is_recording()) {
        gif_recorder_capture(dc.canvas);
        int ww = GetScreenWidth();
        DrawCircle(ww - 22, 22, 8, (Color){220, 40, 40, 255});
        DrawText("REC", ww - 52, 16, 14, (Color){220, 40, 40, 255});
    }

#if defined(PLATFORM_WEB)
    SwapScreenBuffer();
#endif
}

int main(void) {
    /* ---------------------------------------------------------------
     * Initialization
     * MAIN.ASM:1-59 - setup window, audio, load assets
     * --------------------------------------------------------------- */
    /* Resizable so the canvas/window always matches the viewport.
     * On web: raylib calls emscripten_set_canvas_element_size to fill the
     * viewport — the game uses its own letterbox renderer for any size.
     * Do NOT combine with a JS resizeCanvas() CSS override: that creates a
     * canvas.clientWidth != canvas.width mismatch that breaks mouse coords.
     *
     * On Android: pass 0,0 so raylib uses the native display size directly.
     * Requesting a larger size (e.g. 1280x960) triggers raylib's downscale
     * path which sets a viewport larger than the EGL surface, clipping all
     * rendered geometry while glClear still writes (OpenGL spec — clear
     * ignores the viewport). */
    srand((unsigned int)time(NULL));  /* seed RNG so powerups vary each session */

#if defined(PLATFORM_ANDROID)
    InitWindow(0, 0, "Blaster");
#else
    InitWindow(WINDOW_W, WINDOW_H, "Blaster");
#endif
#if defined(PLATFORM_WEB)
    /* InitWindow sets canvas to WINDOW_W x WINDOW_H, but the resize callback
     * only fires on browser resize events — not at startup. Force the canvas
     * to match the actual viewport immediately so the game fits the screen. */
    {
        int vw = EM_ASM_INT({ return window.innerWidth; });
        int vh = EM_ASM_INT({ return window.innerHeight; });
        SetWindowSize(vw, vh);
    }
#endif

    /* Production: suppress raylib INFO/DEBUG log noise */
    SetTraceLogLevel(LOG_WARNING);

    /* Taskbar / window icon */
    Image icon = LoadImage(ASSETS_BASE "blaster_icon.png");
    if (icon.data) { SetWindowIcon(icon); UnloadImage(icon); }

    /* Disable ESC closing the window — ESC is used for pause/back navigation.
     * Quit is handled via state.quit_requested. */
    SetExitKey(KEY_NULL);

    /* Seed rand() so powerup sequences differ each game.
     * MAIN.ASM uses a get_random based on timer ticks; we use time(). */
    srand((unsigned int)time(NULL));
    /* Frame pacing is manual via SUPPORT_CUSTOM_FRAME_CONTROL.
     * Do NOT call SetTargetFPS() — it has no effect. */
    InitAudioDevice();
#if defined(PLATFORM_ANDROID)
    /* Pixel Watch crown / rotary encoder input — init JNI bridge */
    wear_input_init(GetAndroidApp());
#endif
#if defined(BRICKBLASTER_MOBILE)
    /* Accelerometer tilt input — init JNI bridge */
    tilt_input_init(GetAndroidApp());
#endif
    /* Detect device language for UI translations */
    i18n_init();
#if defined(PLATFORM_ANDROID)
    i18n_detect_android(GetAndroidApp());
#endif

    /* Initialize state machine */
    screen_state_init(&state);

    /* Load game assets (sprites, backgrounds, audio) */
    assets_load(&assets);
    audio_init(&audio);
    draw_init(&dc, &assets);

    /* Load screen assets that are needed immediately at startup.
     * intro and final are loaded lazily on first state entry —
     * intro has 36 frames and final has 418 frames, so deferring
     * them cuts startup time dramatically (especially on Wear OS / web). */
    overlays_init(&assets);
    editor_bind_assets(&assets);
    final_bind_font(&assets);
    menu_assets_load(&menu, &assets);
    hiscore_screen_load(&hiscore_screen, &assets);
    credits_assets_load(&credits);

    /* Initialize music manager */
    music_manager_init(&music);

    /* Load high scores from file */
    hiscore_init_defaults(&hiscores);
    hiscore_load(&hiscores, "data/blaster.scr");

    /* P1-ASM-23: load the ASM-style Blaster.cfg text file if present.
     * We look in the data/ folder — mirror of the 1999 game folder.
     * Iter 2 fix #14: the BBCF binary data/blaster.cfg has been removed
     * (rolled back with the modern-UI settings system), so we reclaim the
     * canonical filename and match the 1999 DOS build exactly. */
    settings_defaults(&cfg);
    (void)settings_load_cfg(&cfg, "data/blaster.cfg");

    /* P1-ASM-24: restore persisted volume.  The ASM persists two bytes
     * (User_Volume / User_Volume_Sfx); our .usr stores two floats in
     * [0..1] range mapped to the state.music_enabled / sfx_enabled flags.
     * On a missing file the defaults from screen_state_init() apply. */
    {
        float vm = 1.0f, vs = 1.0f;
        if (settings_load_usr(&vm, &vs, "data/blaster.usr")) {
            state.music_enabled = (vm > 0.0f) ? 1 : 0;
            state.sfx_enabled   = (vs > 0.0f) ? 1 : 0;
        }
    }

    /* Start with intro animation */
    /* On Android (Wear OS), skip intro: the dark fade-in triggers watch ambient
     * mode sensor → APP_CMD_PAUSE → FLAG_WINDOW_MINIMIZED → freeze */
#if defined(PLATFORM_ANDROID)
    state.game_mode = STATE_MENU;
#else
    state.game_mode = STATE_INTRO_ORIGINAL;  /* TakoHi slide is at the END */
#endif

    /* ---------------------------------------------------------------
     * Main Loop
     * MAIN.ASM:60-128 - state machine
     *
     * Web (Emscripten): hand control to the browser event loop.
     * Native: traditional while loop.
     * --------------------------------------------------------------- */
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    {
        double frame_time_prev = GetTime();
        while (!WindowShouldClose() && !state.quit_requested) {
#if !defined(PLATFORM_ANDROID)
            if (IsWindowMinimized()) {
                PollInputEvents();
                SwapScreenBuffer();
                WaitTime(0.05);
                frame_time_prev = GetTime();
                continue;
            }
#endif
            /* CUSTOM FRAME CONTROL: poll input ONCE at top, then draw, then swap.
             * This guarantees fresh input before any logic and eliminates the
             * double-click bug caused by PollInputEvents inside EndDrawing. */
            PollInputEvents();
            UpdateDrawFrame();
            SwapScreenBuffer();

            /* Manual frame pacing (replaces SetTargetFPS) */
            {
                double now = GetTime();
                double elapsed = now - frame_time_prev;
                double target = 1.0 / 60.0;
                if (elapsed < target) WaitTime(target - elapsed);
                frame_time_prev = GetTime();
            }
        }
    }
#endif

    /* ---------------------------------------------------------------
     * Cleanup
     * --------------------------------------------------------------- */
    gif_recorder_shutdown();  /* finalize GIF if still recording */
    hiscore_save(&hiscores, "data/blaster.scr");
    /* P1-ASM-24: persist current volume toggles to .usr so they survive a
     * restart — mirrors ASM Write_Config_User (FILE.ASM:822-832). */
    {
        float vm = state.music_enabled ? 1.0f : 0.0f;
        float vs = state.sfx_enabled   ? 1.0f : 0.0f;
        (void)settings_save_usr(vm, vs, "data/blaster.usr");
    }

    intro_assets_unload(&intro);
    intro_original_assets_unload(&intro_original);
    menu_assets_unload(&menu);
    hiscore_screen_unload(&hiscore_screen);
    credits_assets_unload(&credits);
    final_assets_unload(&final_screen);

    music_manager_cleanup(&music);
    draw_shutdown(&dc);
    assets_unload(&assets);
    audio_shutdown(&audio);

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
