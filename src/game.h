#pragma once
/* game.h — BrickBlaster main game state machine
 *
 * Central coordinator that wires all modules together.
 * Mirrors the MAIN.ASM frame loop at MAIN.ASM:1061-1175 (label "main:").
 *
 * ============================================================================
 * MAIN.ASM frame loop order (main: label, MAIN.ASM:1061)
 * ============================================================================
 *
 *  1. erase_sprites           (clear sprite layer)
 *  2. detect_new_option       (spawn falling powerups on brick destruction)
 *  3. detect_current_option_end (tick active powerup timers)
 *  4. detect_new_life_player_1/2 (bonus life threshold check)
 *  5. detect_large/small_cursor_player_1/2 (apply paddle size powerups)
 *  6. detect_shoot_player_1/2 (tick gun cooldown)
 *  7. create_monster          (monster spawn logic)
 *  8. detect_init_palette     (night mode palette effect)
 *  9. detect_collision_cursor (ball ↔ paddle collision for all active balls)
 * 10. refresh_mouse           (input: paddle movement)
 * 11. detect_start_game       (if READY_TO_PLAY: launch ball on fire)
 * 12. refresh_ball            (ball movement + wall/brick collision)
 * 13. refresh_shoot           (projectile movement)
 * 14. refresh_monster         (monster AI)
 * 15. refresh_options         (powerup fall + collection)
 * 16. refresh_sprites         (draw pass — handled by Task 13)
 * 17. detect_game_over_player_1/2  (ball-lost → lose life → restart/game-over)
 * 18. detect_next_level       (all breakable bricks gone → next level)
 * 19. Keyboard test           (ESC, P=pause, demo timeout)
 * 20. Demo timer decrement    (MAIN.ASM:1165 dec demo_counter)
 *
 * ============================================================================
 * SPEED INCREASE LOGIC (MAIN.ASM:3387-3409  detect_inc_speed)
 * ============================================================================
 *
 * Each ball sprite has a sprite_speed_counter field initialised to speed_delai
 * (FILE.ASM:1133  speed_delai dd 1500).
 * Every frame the counter is decremented while the ball moves downward
 * (sprite_sens_y < 0 — note ASM sign convention: negative = going up,
 *  so downward-moving ball has sprite_sens_y >= 0 and counter is NOT decremented
 *  when vy >= 0; actually the ASM condition at MAIN.ASM:3390 is:
 *   "cmp [edx.sprite_sens_y],0  jns @@cont" — jns = jump if not sign = jump if >= 0
 *   so the decrement is skipped when sens_y >= 0 and ONLY runs when sens_y < 0
 *   i.e. when the ball is moving UPWARD).
 * When counter reaches 0, speed is incremented on both axes and the counter
 * is reset to speed_level (the current speed-up period).
 *
 * speed_level formula (MAIN.ASM:4906-4911):
 *   speed_level = speed_delai - (current_level - 1) * 17
 *   — higher levels → shorter period → faster auto-speedup.
 *
 * We model this per-ball: each Ball gets speed_counter and speed_level fields.
 * Game-level: we cache the computed speed_level once per level load.
 *
 * ============================================================================
 * DEMO MODE (MAIN.ASM:1159-1168)
 * ============================================================================
 *
 * When demo_flag is On and test_flag is Off:
 *   - read_click: any mouse button pressed exits demo (ret).
 *   - dec demo_counter each frame (MAIN.ASM:1165).
 *   - when demo_counter == 0 → ret (returns to intro loop, restarts demo).
 *   DELAI_DEMO = 800 frames (Blaster.inc:420).
 *   We expose this as game.demo_timer counting down from DELAI_DEMO.
 *
 * game_update() resets demo_timer on any keyboard/mouse input
 * (mirrors ASM "read_click jnz @@exit" — any click exits demo).
 *
 * ============================================================================
 * SPEED CONSTANTS (FILE.ASM:1127-1138 data section)
 * ============================================================================
 *
 *   speed_start_easy   = 2   (initial vx for easy difficulty)
 *   speed_start_medium = 3
 *   speed_start_hard   = 4
 *   change_speed_level_easy   = 3  (divisor for per-level speed boost)
 *   change_speed_level_medium = 4
 *   change_speed_level_hard   = 4
 *   speed_delai        = 1500 (frames between auto-speedup; initial speed_counter)
 *   nbs_ball_start     = 2   (starting lives, player_nbs_ball)
 *   start_level        = 1
 *   bonus_extra_life   = 10000  (score threshold per bonus life)
 */

#include "constants.h"
#include "screen_manager.h"
#include "ball.h"
#include "paddle.h"
#include "brick.h"
#include "powerup.h"
#include "level.h"
#include "hiscore.h"
#include "monster.h"
#include "assets.h"
#include "audio.h"
#include "input_frame.h"

/* ============================================================================
 * Speed/difficulty constants (FILE.ASM:1127-1143 data section)
 * ============================================================================ */

/* Initial ball X speed by difficulty.  MAIN.ASM:5295-5307  mov eax,speed_start_* */
#define SPEED_START_EASY    2   /* FILE.ASM:1127  speed_start_easy   dd 2 */
#define SPEED_START_MEDIUM  3   /* FILE.ASM:1128  speed_start_medium dd 3 */
#define SPEED_START_HARD    4   /* FILE.ASM:1129  speed_start_hard   dd 4 */

/* Divisor for per-level speed boost.  MAIN.ASM:5296-5307  mov ecx,change_speed_level_* */
#define CHANGE_SPEED_EASY   3   /* FILE.ASM:1134  change_speed_level_easy   dd 3 */
#define CHANGE_SPEED_MEDIUM 4   /* FILE.ASM:1135  change_speed_level_medium dd 4 */
#define CHANGE_SPEED_HARD   4   /* FILE.ASM:1136  change_speed_level_hard   dd 4 */

/* Frames between automatic speed-up events.  FILE.ASM:1133  speed_delai dd 1500 */
#define SPEED_DELAI         1500

/* Starting lives.  FILE.ASM:1137  nbs_ball_start dd 2
 * (player_nbs_ball starts at 2; game over when it reaches -1 → 3 total lives) */
#define NBS_BALL_START      2

/* Score for bonus extra life.  FILE.ASM:1143  bonus_extra_life dd 10000 */
#define BONUS_EXTRA_LIFE    10000

/* Maximum simultaneous falling powerups on screen.
 * struc_options table has 8 option slots (Option_1..Option_8, MAIN.ASM:7057-7064) */
#define MAX_POWERUPS_ACTIVE 8

/* Maximum simultaneous projectiles (Shoot_1..Shoot_20, but paddle fires 2 at once) */
#define MAX_PROJECTILES     20

/* ============================================================================
 * Projectile — paddle laser shot
 * MAIN.ASM  vaisseau_tir_speed = 1 (Blaster.inc:242)
 * ============================================================================ */
typedef struct {
    int x, y;      /* pixel position top-left */
    int vy;         /* speed upward: Blaster.inc:242  vaisseau_tir_speed = 1 (moves up, so -1) */
    int active;     /* 1 = in flight */
    int is_big;     /* 1 = big projectile (POWERUP_SHOOT), 0 = mini (POWERUP_MINI_SHOOT) */
    int owner;      /* 0 = P1, 1 = P2 — sprite_player in MAIN.ASM:1960/2169 */
} Projectile;

/* ============================================================================
 * Game — the central game state
 * ============================================================================ */
typedef struct {
    /* -------------------------------------------------------------------
     * State machine
     * Blaster.inc:456-462 READY_TO_PLAY, PLAYING, GAME_OVER, etc.
     * ------------------------------------------------------------------- */
    GameMode     state;

    /* -------------------------------------------------------------------
     * Settings
     * ------------------------------------------------------------------- */
    Difficulty   difficulty;
    int          game_mode;     /* 0 = 1P solo, 1 = 2P coop, 2 = 2P duel */
    int          world;         /* 0 = Blaster.lv0, 1 = lv1, 2 = lv2 */

    /* -------------------------------------------------------------------
     * Player state (player_1 struct in ASM, MAIN.ASM:7151)
     * ------------------------------------------------------------------- */
    int          score;         /* player_counter_score */
    int          lives;         /* player_nbs_ball (starts at NBS_BALL_START=2) */
    int          bonus_life_threshold; /* player_bonus_life (next score for extra life) */
    int          level_num;     /* current_level (1-80) */

    /* -------------------------------------------------------------------
     * Entities
     * MAIN.ASM:7066-7085  Ball_1..Ball_20 — 20 active ball slots total.
     * BALL_MAX (19) = nbs_ball_max = max *extra* balls spawnable above slot 0.
     * Array size = BALL_MAX + 1 = 20, indices 0..BALL_MAX.
     * ------------------------------------------------------------------- */
    Ball         balls[BALL_MAX + 1];
    int          ball_count;    /* number of active (in-play) ball slots */

    Paddle       paddle;
    /* Multiplayer (game_mode > 0): second paddle, score & lives.
     * MAIN.ASM:1004-1006, 2068, 4638, 6478 — cursor_2 / player_2. */
    Paddle       paddle_2;
    int          score_2;
    int          lives_2;
    int          bonus_life_threshold_2;
    int          control_p2;    /* 0=computer, 1=keyboard (ZQSD+F), 2=joystick */

    /* Per-player ball-lost tagging (duel mode). Set when a ball exits the
     * bottom; consumed by handle_life_lost / per-player respawn. MAIN.ASM:4595
     * detect_game_over_player_1 — per-player path. */
    int          p1_ball_lost_pending;
    int          p2_ball_lost_pending;

    Brick        bricks[BRICK_COUNT]; /* 13*30 = 390 bricks */

    Powerup      powerups[MAX_POWERUPS_ACTIVE];
    int          powerup_count; /* active falling powerups on screen */

    Projectile   projectiles[MAX_PROJECTILES];
    int          proj_count;

    /* -------------------------------------------------------------------
     * Active powerup effect flags
     * Mirrors current_option / player_1.player_current_option
     * These are timed effects (DELAI_OPTION = 600 frames each).
     * ------------------------------------------------------------------- */
    /* MAGNETIC per-player bitmask. Bits: PLAYER_ONE=1, PLAYER_TWO=2.
     * Mirrors ASM `magnetic_flag` at Blaster.inc:13-14 and MAIN.ASM:6755-6759
     * option_magnetic_p which sets the bit for current_player only.
     * P1-ASM-12 fix: per-player magnetic state. */
    int          magnetic_flag;        /* POWERUP_MAGNETIC per-player bitmask */
    int          iron_active;          /* POWERUP_IRON_BALL — ball destroys bricks without bouncing */
    /* NOTE: laser state is per-paddle (Paddle.laser_timer / mini_laser).
     * The SHOOT/MINI_SHOOT powerups do not occupy current_option — they run
     * independently on each paddle so one player collecting a laser does
     * not cancel the other's active laser. Fix for P2-cancels-P1 bug.
     * Per ASM MAIN.ASM:6509-6603 (option_shoot_p / option_mini_shoot_p):
     *   separate counters count_tir_big_1/_2 and count_tir_left_1/_2.
     * REVERSE / SMALL_SHIP / LARGE_SHIP are also per-paddle
     * (Paddle.reverse_timer / size_timer) — no global game flag needed. */
    int          night_active;         /* POWERUP_NIGHT     — score doubled */
    int          ghost_active;         /* POWERUP_GHOST     — ball passes walls */
    int          current_option_count; /* countdown timer for active timed powerup (DELAI_OPTION=600) */
    PowerupType  current_option;       /* active timed powerup type, or POWERUP_COUNT if none */

    /* Pickup text banner — stamped on every apply_powerup, rendered at the
     * panel_info position (MAIN.ASM:347 last_print + panel_info.sprite_adrs,
     * Blaster.cfg option_text_* strings).  Instant powerups also flash the
     * text for a short duration; timed ones linger roughly current_option_count. */
    int          pickup_text_timer;    /* frames remaining; 0 = hidden */
    PowerupType  pickup_text_type;     /* last-collected powerup */

    /* POWERUP_COLLISION (duel / coop) — MAIN.ASM:6830-6836 option_collision_p
     * stores (cursor_2.x - cursor_1.x) at pickup time.  detect_collision_cursor
     * (MAIN.ASM:2274-2316) branches on the sign every frame to decide which
     * paddle clamps which. */
    int          collision_flag;

    /* -------------------------------------------------------------------
     * Demo mode  (MAIN.ASM:1159-1168, Blaster.inc:420  DELAI_DEMO=800)
     * ------------------------------------------------------------------- */
    int          demo_active;   /* demo_flag: 1 = demo/attract mode running */
    int          demo_timer;    /* demo_counter: counts down from DELAI_DEMO (800) */

    /* -------------------------------------------------------------------
     * Speed increase timer (per-game, not per-ball)
     * FILE.ASM:1133  speed_delai = 1500
     * MAIN.ASM:4906-4911  speed_level = speed_delai - (level-1)*17
     * ------------------------------------------------------------------- */
    int          speed_counter; /* frames until next auto-speedup (counts down) */
    int          speed_level;   /* current speed-up period length (decreases each level) */

    /* -------------------------------------------------------------------
     * Option (powerup) spawn cooldown
     * FILE.ASM:1139-1141  delai_between_option_* = 60 frames
     * game_manager.gd:76  OPTION_SPAWN_COOLDOWN = 60
     * ------------------------------------------------------------------- */
    int          option_spawn_timer; /* frames since last powerup spawned */

    /* Per-difficulty spacing between powerup spawns, injected from
     * Blaster.cfg Time_Between_Option (FILE.ASM:1139-1141). 0 = use the
     * compiled-in DELAI_BETWEEN_OPTION default. P1-ASM-34. */
    int          cfg_delai_between_option[3];

    /* F3 P1-ASM-36: Per-difficulty monster spawn delay, injected from
     * Blaster.cfg Freq_Create_Monster (FILE.ASM:985-989, 1130-1132).
     * 0 = use the compiled-in MONSTER_DELAI_* default. */
    int          cfg_monster_delai[3];

    /* F6-01: Per-powerup per-difficulty spawn frequencies, injected from
     * Blaster.cfg Freq_Option_* (FILE.ASM:962-973 + 1143 struc_options
     * option_easy/medium/hard). Row order matches PowerupType enum.
     * If the whole row is 0 (default zero-init after memset) we fall back
     * to powerup_freq_table[] in powerup.c. ASM cite: MAIN.ASM:5493-5504
     * random_options loads [eax.option_easy/medium/hard] — values are
     * overwritten at cfg load by FILE.ASM:965-973. */
    int          cfg_freq_option[POWERUP_COUNT][3];

    /* -------------------------------------------------------------------
     * Loaded data
     * ------------------------------------------------------------------- */
    Level        current_level;
    Hiscores     hiscores;
    Assets      *assets;        /* pointer owned by main() — not freed here */
    AudioState  *audio;         /* pointer owned by main() — not freed here */

    /* -------------------------------------------------------------------
     * Frame counter (for animation, timing)
     * ------------------------------------------------------------------- */
    int          frame;

    /* -------------------------------------------------------------------
     * Monsters (MAIN.ASM:7096-7100  Monster_1..Monster_4)
     * ------------------------------------------------------------------- */
    Monster      monsters[NBS_MONSTER];
    int          monster_spawn_counter;  /* counter_monster: frames since last spawn */

    /* -------------------------------------------------------------------
     * Brick break animations
     * DRAW.ASM:Refresh_Sprites cycles break_ball sprites (5 frames, 9x9).
     * Blaster.inc:344  break_ball_blue_o = 046+(screen_x*000)
     * ------------------------------------------------------------------- */
    #define MAX_BREAK_ANIMS 16
    struct {
        int x, y;       /* pixel position of destroyed brick centre */
        int frame;      /* 0..BREAK_NBS_ANIM-1 */
        int active;
    } break_anims[MAX_BREAK_ANIMS];

    /* -------------------------------------------------------------------
     * Dev test mode (F9 toggle)
     * Every brick destroy spawns a powerup, cycling through all types.
     * ------------------------------------------------------------------- */
    int          dev_test;          /* 1 = dev test mode active */
    int          dev_powerup_cycle; /* next powerup type to spawn (0..23) */
} Game;


/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/* Initialise all game state.  Call once after window/audio are ready.
 * Sets state=STATE_MENU, loads hiscores, resets all counters.
 * MAIN.ASM:995-1045  start_new_game / start_game labels */
void game_init(Game *g, Assets *assets, AudioState *audio, Difficulty diff, int mode);

/* Load a level by number (1..80) into g->current_level and g->bricks[].
 * Computes speed_level for this level.
 * MAIN.ASM:1001-1002  mov eax,start_level → mov current_level,eax
 * MAIN.ASM:4906-4911  speed_level = speed_delai - (current_level-1)*17 */
void game_load_level(Game *g, int level_num);

/* Place ball on paddle centre, set magnetic.  Does NOT change g->state;
 * callers must set the appropriate READY_TO_PLAY / READY_TO_PLAY_AGAIN state.
 * MAIN.ASM:5173-5231  init_start_game
 *   ball.pos_y = cursor.pos_y - ball.size_y
 *   ball.pos_x = cursor.pos_x + cursor.size_x/2 - ball.size_x/2 */
void game_spawn_ball(Game *g);


/* ============================================================================
 * Per-frame update — call once per frame at 60 Hz
 *
 * Mirrors MAIN.ASM:1061-1175 "main:" label frame loop in this order:
 *   1. Input → paddle move (refresh_mouse / MOUSE.ASM)
 *   2. If STATE_READY_TO_PLAY: track ball on paddle; fire on space
 *      (detect_start_game  MAIN.ASM:5235-5351)
 *   3. For each active ball: collision_walls → collision_paddle →
 *      collision_bricks → ball_move  (refresh_ball  MAIN.ASM:1082)
 *   4. Projectile movement (refresh_shoot  MAIN.ASM:1083)
 *   5. Powerup fall + collection (refresh_options  MAIN.ASM:1085)
 *   6. Active powerup timer tick (detect_current_option_end  MAIN.ASM:1068)
 *   7. Check level complete: no active normal bricks remain
 *      (detect_next_level  MAIN.ASM:4120)
 *   8. Check all balls lost → lose life (detect_game_over_player_1  MAIN.ASM:4595)
 *   9. Demo timer tick / input exit (MAIN.ASM:1159-1168)
 *  10. Periodic speed-up (detect_inc_speed  MAIN.ASM:3387)
 *  11. Bonus life check every BONUS_EXTRA_LIFE points (MAIN.ASM:6451-6492)
 * ============================================================================ */
void game_update(Game *g, const FrameInput *input);


/* ============================================================================
 * Accessors (read-only view, no side effects)
 * ============================================================================ */

/* Count of currently active (in-play) balls. */
int game_active_ball_count(const Game *g);

/* 1 if all breakable normal bricks have been destroyed (level complete). */
int game_level_complete(const Game *g);

/* P1-ASM-34: inject per-difficulty powerup spawn spacing from Blaster.cfg
 * Time_Between_Option (6,10,12). Call once after game_init() or any time
 * the cfg changes. Values are frames (matches ASM data at FILE.ASM:1139-1141).
 * Pass NULL to revert to the compiled-in DELAI_BETWEEN_OPTION default. */
void game_set_powerup_spacing(Game *g, const int delai_per_diff[3]);

/* F3 P1-ASM-36: inject per-difficulty monster spawn delay from Blaster.cfg
 * Freq_Create_Monster (FILE.ASM:985-989). Values are frames matching ASM
 * data at FILE.ASM:1130-1132 (600/500/300). Pass NULL to revert to the
 * compiled-in MONSTER_DELAI_* defaults. */
void game_set_monster_delai(Game *g, const int delai_per_diff[3]);

/* F6-01: inject per-powerup per-difficulty spawn frequencies from Blaster.cfg
 * Freq_Option_* (FILE.ASM:962-973). `freq` is indexed [PowerupType][0=easy,
 * 1=medium, 2=hard] — matches the GameConfig.freq_option[24][3] loader.
 * ASM cite: MAIN.ASM:5493-5504 random_options reads option_easy/medium/hard
 * which are overwritten at cfg load (FILE.ASM:965-973). Pass NULL to revert
 * to the compiled-in powerup_freq_table[] in powerup.c. */
void game_set_powerup_freq(Game *g, const int freq[POWERUP_COUNT][3]);
