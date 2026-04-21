/* game.c — BrickBlaster main game state machine
 *
 * Central coordinator: wires ball, paddle, brick, powerup, collision modules.
 * All logic derived from MAIN.ASM — see game.h for full architecture notes.
 *
 * MAIN.ASM key labels:
 *   start_new_game : 995    — reset score/lives, set level
 *   start_game     : 1032   — init sprites, cursor, ball, panel
 *   rebuild_all    : 1049   — create wall, init demo/options/monster
 *   main           : 1061   — per-frame game loop
 *   detect_start_game : 5235 — READY_TO_PLAY ball launch on fire
 *   detect_game_over_player_1 : 4595
 *   detect_next_level : 4120
 *   detect_inc_speed  : 3387
 */

#include "game.h"
#include "collision.h"
#include "audio.h"
#include "demo.h"
#include <stdio.h>     /* snprintf */
#include <string.h>    /* memset */
#include <stdlib.h>    /* abs */

#include "raylib.h"    /* TraceLog */

/* Forward declarations for static helpers used before their definitions */
static int compute_speed_level(int level_num);
static PowerupType dev_next_powerup(Game *g);
static void compute_launch_velocity(Difficulty diff, int level_num, int *out_vx, int *out_vy);
static void deactivate_current_option(Game *g);
static void inc_score(Game *g, int owner, int ecx);
static void dec_score(Game *g, int owner, int ecx);
static void detect_collision_cursor(Game *g);

/* P1-ASM-34: Time_Between_Option per-difficulty spacing, read from cfg if
 * set by game_set_powerup_spacing(), falling back to DELAI_BETWEEN_OPTION.
 * Difficulty values map 1/2/4 → easy/medium/hard indices 0/1/2. */
static int powerup_spacing(const Game *g) {
    int idx;
    switch (g->difficulty) {
        case DIFFICULTY_EASY:   idx = 0; break;
        case DIFFICULTY_HARD:   idx = 2; break;
        case DIFFICULTY_MEDIUM:
        default:                idx = 1; break;
    }
    int v = g->cfg_delai_between_option[idx];
    return (v > 0) ? v : DELAI_BETWEEN_OPTION;
}

void game_set_powerup_spacing(Game *g, const int delai_per_diff[3]) {
    if (!g) return;
    if (delai_per_diff) {
        g->cfg_delai_between_option[0] = delai_per_diff[0];
        g->cfg_delai_between_option[1] = delai_per_diff[1];
        g->cfg_delai_between_option[2] = delai_per_diff[2];
    } else {
        g->cfg_delai_between_option[0] = 0;
        g->cfg_delai_between_option[1] = 0;
        g->cfg_delai_between_option[2] = 0;
    }
}

/* F3 P1-ASM-36: accept per-difficulty overrides from Blaster.cfg
 * Freq_Create_Monster. Mirrors game_set_powerup_spacing. */
void game_set_monster_delai(Game *g, const int delai_per_diff[3]) {
    if (!g) return;
    if (delai_per_diff) {
        g->cfg_monster_delai[0] = delai_per_diff[0];
        g->cfg_monster_delai[1] = delai_per_diff[1];
        g->cfg_monster_delai[2] = delai_per_diff[2];
    } else {
        g->cfg_monster_delai[0] = 0;
        g->cfg_monster_delai[1] = 0;
        g->cfg_monster_delai[2] = 0;
    }
}

/* F6-01: inject per-powerup per-difficulty spawn frequencies from Blaster.cfg
 * Freq_Option_*. ASM cite: FILE.ASM:965-973 overwrites the struc_options
 * option_easy/medium/hard fields read by MAIN.ASM:5493-5504 random_options. */
void game_set_powerup_freq(Game *g, const int freq[POWERUP_COUNT][3]) {
    if (!g) return;
    if (freq) {
        for (int i = 0; i < POWERUP_COUNT; i++) {
            g->cfg_freq_option[i][0] = freq[i][0];
            g->cfg_freq_option[i][1] = freq[i][1];
            g->cfg_freq_option[i][2] = freq[i][2];
        }
    } else {
        for (int i = 0; i < POWERUP_COUNT; i++) {
            g->cfg_freq_option[i][0] = 0;
            g->cfg_freq_option[i][1] = 0;
            g->cfg_freq_option[i][2] = 0;
        }
    }
}

/* F6-01: pick a random powerup using the cfg-injected freq table if set,
 * else fall back to powerup.c's hardcoded powerup_freq_table[]. Mirrors
 * the full random_options loop (MAIN.ASM:5472-5513):
 *   - rand idx in [0..POWERUP_COUNT-2] (COLLISION excluded)
 *   - look up freq[idx][diff_idx]
 *   - freq==1 → always spawn; freq==0 → no spawn; freq>=2 → spawn iff
 *     get_random(freq-1)==0, i.e. 1/(freq-1) probability.
 * Returns POWERUP_COUNT on "no spawn".
 *
 * Note on probability math: in the ASM, freq is the raw Freq_Option value.
 * For Blaster.cfg freq=N, the spawn probability is 1/(N-1) per call — e.g.
 * Freq_Option_3_ball=(2,3,4) → 1/1, 1/2, 1/3 easy/med/hard. This matches
 * the intuition that the cfg comment "0-10" encodes "rarer as N grows". */
static PowerupType pick_powerup_cfg(const Game *g) {
    int diff_idx;
    switch (g->difficulty) {
        case DIFFICULTY_EASY:   diff_idx = 0; break;
        case DIFFICULTY_HARD:   diff_idx = 2; break;
        case DIFFICULTY_MEDIUM:
        default:                diff_idx = 1; break;
    }

    /* Detect if we have any cfg override (at least one non-zero entry).
     * If not, fall through to powerup_random_type() which uses the
     * hardcoded table. */
    int has_override = 0;
    for (int i = 0; i < POWERUP_COUNT; i++) {
        if (g->cfg_freq_option[i][0] != 0 ||
            g->cfg_freq_option[i][1] != 0 ||
            g->cfg_freq_option[i][2] != 0) {
            has_override = 1;
            break;
        }
    }
    if (!has_override) return powerup_random_type(g->difficulty);

    /* MAIN.ASM:5473 mov eax,options_number-1 → 0..POWERUP_COUNT-2 (COLLISION excluded). */
    int idx = rand() % (POWERUP_COUNT - 1);
    int freq = g->cfg_freq_option[idx][diff_idx];

    /* MAIN.ASM:5506 cmp eax,1 je @@forced */
    if (freq == 1) return (PowerupType)idx;
    /* MAIN.ASM:5508 cmp eax,Off je @@again — here we return "no spawn". */
    if (freq <= 0) return (PowerupType)POWERUP_COUNT;
    /* MAIN.ASM:5510-5513 dec eax; call get_random; or eax,eax; jnz @@end. */
    int roll = rand() % (freq - 1 > 0 ? freq - 1 : 1);
    if (roll != 0) return (PowerupType)POWERUP_COUNT;
    return (PowerupType)idx;
}

/* --------------------------------------------------------------------------
 * Dev test level: all brick types, easy to destroy, guaranteed powerup drops.
 *
 * Layout (13 cols x 10 rows used, rows 0-9):
 *   Row 0: 13 normal green bricks (hp=1) — easy to break, lots of powerups
 *   Row 1: 13 normal blue bricks (hp=1)
 *   Row 2: 13 normal violet bricks (hp=2)
 *   Row 3: 13 normal orange bricks (hp=3)
 *   Row 4: 4 indestructible, 5 normal, 4 indestructible
 *   Row 5: 13 transparent bricks
 *   Row 6: 4 teleporter, 5 normal green, 4 teleporter
 *   Row 7: 13 normal green (hp=1)
 *   Row 8: 13 normal blue (hp=1)
 *   Row 9: 13 normal orange (hp=1)
 * -------------------------------------------------------------------------- */
static void load_dev_test_level(Game *g)
{
    int i, col, row;
    unsigned char raw;

    /* Clear all bricks first */
    for (i = 0; i < BRICK_COUNT; i++) {
        g->current_level.bricks[i] = ABSENTE;
    }

    for (row = 0; row < 10; row++) {
        for (col = 0; col < BRICK_COLS; col++) {
            i = row * BRICK_COLS + col;
            switch (row) {
            case 0:
                /* Green normal hp=1: color=0x00 | type=0x20 | hp=1 = 0x21 */
                raw = BRICK_COLOR_GREEN | NORMALE | 1;
                break;
            case 1:
                /* Blue normal hp=1 */
                raw = BRICK_COLOR_BLUE | NORMALE | 1;
                break;
            case 2:
                /* Violet normal hp=2 */
                raw = BRICK_COLOR_VIOLET | NORMALE | 2;
                break;
            case 3:
                /* Orange normal hp=3 */
                raw = BRICK_COLOR_ORANGE | NORMALE | 3;
                break;
            case 4:
                /* Mix: indestructible on edges, normal in middle */
                if (col < 4 || col >= 9)
                    raw = INCASSABLE;
                else
                    raw = BRICK_COLOR_GREEN | NORMALE | 1;
                break;
            case 5:
                /* Transparent */
                raw = BRICK_COLOR_GREEN | TRANSPARENTE | 1;
                break;
            case 6:
                /* Mix: teleporter on edges, normal in middle */
                if (col < 4 || col >= 9)
                    raw = BRICK_COLOR_BLUE | TELEPORTEUSE | 1;
                else
                    raw = BRICK_COLOR_GREEN | NORMALE | 1;
                break;
            case 7:
                raw = BRICK_COLOR_GREEN | NORMALE | 1;
                break;
            case 8:
                raw = BRICK_COLOR_BLUE | NORMALE | 1;
                break;
            case 9:
                raw = BRICK_COLOR_ORANGE | NORMALE | 1;
                break;
            default:
                raw = ABSENTE;
                break;
            }
            g->current_level.bricks[i] = raw;
        }
    }

    /* Initialize Brick structs from raw bytes */
    for (i = 0; i < BRICK_COUNT; i++) {
        brick_init(&g->bricks[i], i, g->current_level.bricks[i]);
    }

    /* Reset falling powerups and projectiles */
    memset(g->powerups, 0, sizeof(g->powerups));
    g->powerup_count = 0;
    memset(g->projectiles, 0, sizeof(g->projectiles));
    g->proj_count = 0;

    g->speed_level   = compute_speed_level(1);
    g->speed_counter = g->speed_level * 3;  /* 3x: compensate for 60fps vs original 18fps */
}

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/* Ball launch speed by difficulty.
 * MAIN.ASM:5295-5307  mov eax,speed_start_*  FILE.ASM:1127-1129 */
static int speed_start(Difficulty diff) {
    if (diff == DIFFICULTY_HARD)   return SPEED_START_HARD;
    if (diff == DIFFICULTY_MEDIUM) return SPEED_START_MEDIUM;
    return SPEED_START_EASY;
}

/* Divisor for per-level speed boost at launch.
 * MAIN.ASM:5296-5307  mov ecx,change_speed_level_*  FILE.ASM:1134-1136 */
static int change_speed_divisor(Difficulty diff) {
    if (diff == DIFFICULTY_HARD)   return CHANGE_SPEED_HARD;
    if (diff == DIFFICULTY_MEDIUM) return CHANGE_SPEED_MEDIUM;
    return CHANGE_SPEED_EASY;
}

/* Compute speed_level for a given level number.
 * MAIN.ASM:4906-4911:
 *   mov edx,speed_delai         ; edx = 1500
 *   mov speed_level,edx
 *   sub speed_level,eax         ; eax = (current_level-1)*17
 * → speed_level = speed_delai - (current_level - 1) * 17 */
static int compute_speed_level(int level_num) {
    int sl = SPEED_DELAI - (level_num - 1) * 17;
    if (sl < 1) sl = 1;   /* clamp to at least 1 frame */
    return sl;
}

/* Compute initial launch velocity for ball 1.
 * MAIN.ASM:5330-5343:
 *   mov ball_1.sprite_sens_x,eax        ; eax = speed_start
 *   inc eax; neg eax
 *   mov ball_1.sprite_sens_y,eax        ; vy = -(speed_start + 1)
 *   ; per-level speed boost:
 *   mov eax,level_number; cdq; idiv ecx ; eax = level_number / change_speed_divisor
 *   mov ecx,eax
 *   mov eax,current_level; cdq; idiv ecx ; eax = current_level / (level_number/divisor)
 *   add ball_1.sprite_sens_x,eax
 *   sub ball_1.sprite_sens_y,eax
 *
 * level_number = total levels in file = LEVELS_PER_FILE = 80
 */
static void compute_launch_velocity(Difficulty diff, int level_num, int *out_vx, int *out_vy) {
    int spd   = speed_start(diff);               /* MAIN.ASM:5295  eax = speed_start */
    int cdiv  = change_speed_divisor(diff);       /* MAIN.ASM:5296  ecx = change_speed_divisor */
    int vx    = spd;                             /* MAIN.ASM:5330  ball_1.sprite_sens_x = eax */
    int vy    = -(spd + 1);                      /* MAIN.ASM:5331-5333  inc eax; neg eax → vy */

    /* Per-level boost: MAIN.ASM:5335-5343 */
    int lvl_div = LEVELS_PER_FILE / cdiv;        /* level_number/cdiv  (integer div) */
    if (lvl_div < 1) lvl_div = 1;
    int boost = level_num / lvl_div;             /* current_level/lvl_div */
    vx += boost;                                 /* MAIN.ASM:5342  add ball_1.sprite_sens_x,eax */
    vy -= boost;                                 /* MAIN.ASM:5343  sub ball_1.sprite_sens_y,eax */

    /* Clamp to speed limits — ball_clamp_speed will enforce at runtime */
    *out_vx = vx;
    *out_vy = vy;
}

/* --------------------------------------------------------------------------
 * inc_score / dec_score — helpers mirroring FONTE.ASM:40-112
 *
 * FONTE.ASM:73-112 inc_score:
 *   difficulty: +1 (medium) / +2 (hard) to ecx BEFORE loop
 *   dual_flag off → ebp = player_1
 *   option_night_flag On → shl ecx,1 (double ecx)
 *   loop ecx times: counter_score += 10 (clamped at 999990)
 *
 * FONTE.ASM:40-69 dec_score:
 *   NOTE: ASM dec_score does NOT apply difficulty bonus — only night x2 and
 *   the per-iter -10 loop, with floor at 0. Mirrored exactly here.
 *
 * Bonus life check inlined here (MAIN.ASM:6451-6472 detect_new_life_player_1):
 *   when player_counter_score >= player_bonus_life:
 *     player_bonus_life += bonus_extra_life (10000)
 *     mov ecx,8 ; call inc_score  → +80 pts base + difficulty (P1-ASM-8)
 *     option_new_life_p → inc player_nbs_ball (clamp to NBS_BALL_MAX)
 *
 * `owner`: 0 = player_1, 1 = player_2. In dual (game_mode==2) mode the
 * owner's score/life counters are used; in solo/coop both route to
 * player_1's counters per ASM `dual_flag off → ebp=player_1` at FONTE.ASM:89.
 * -------------------------------------------------------------------------- */
static void inc_score_raw(Game *g, int owner, int ecx, int recurse_bonus_life);

static void inc_score(Game *g, int owner, int ecx) {
    inc_score_raw(g, owner, ecx, 1);
}

static void inc_score_raw(Game *g, int owner, int ecx, int recurse_bonus_life) {
    /* FONTE.ASM:76-84  difficulty bonus added BEFORE ecx multiplier */
    if (g->difficulty == DIFFICULTY_HARD)        ecx += 2;
    else if (g->difficulty == DIFFICULTY_MEDIUM) ecx += 1;

    /* FONTE.ASM:92-95  option_night_flag → shl ecx,1 */
    if (g->night_active) ecx <<= 1;

    /* FONTE.ASM:96  jecxz @@end — zero/negative count skipped */
    if (ecx <= 0) return;

    {
        int delta = ecx * 10;  /* FONTE.ASM:98-104  loop add 10 per iter */
        int *target_score = (g->game_mode == 2 && owner == 1) ? &g->score_2 : &g->score;
        int *target_thr   = (g->game_mode == 2 && owner == 1) ? &g->bonus_life_threshold_2 : &g->bonus_life_threshold;
        int *target_lives = (g->game_mode == 2 && owner == 1) ? &g->lives_2 : &g->lives;

        *target_score += delta;
        if (*target_score > 999990) *target_score = 999990;  /* FONTE.ASM:99 clamp */

        /* MAIN.ASM:6451-6472 detect_new_life — bonus life trigger */
        if (recurse_bonus_life && *target_score >= *target_thr) {
            *target_thr += BONUS_EXTRA_LIFE;                /* MAIN.ASM:6460-6461 */
            (*target_lives)++;                              /* MAIN.ASM:6436 option_new_life_p */
            if (*target_lives > BALL_MAX) *target_lives = BALL_MAX;
            if (g->audio) audio_play(g->audio, SFX_NEW_LIFE);  /* MAIN.ASM:6429 */
            /* MAIN.ASM:6463-6465  mov ecx,8; call inc_score — +80 bonus (P1-ASM-8)
             * Recurse without the bonus-life check to avoid infinite loop if the
             * +80 would itself trigger another life. */
            inc_score_raw(g, owner, 8, 0);
        }
    }
}

static void dec_score(Game *g, int owner, int ecx) {
    /* FONTE.ASM:40-69 dec_score has NO difficulty multiplier — only night x2. */
    if (g->night_active) ecx <<= 1;          /* FONTE.ASM:48-50 */
    if (ecx <= 0) return;                    /* FONTE.ASM:52 jecxz @@end */

    {
        int delta = ecx * 10;                /* FONTE.ASM:54-61 loop sub 10 per iter */
        int *target_score = (g->game_mode == 2 && owner == 1) ? &g->score_2 : &g->score;
        *target_score -= delta;
        if (*target_score < 0) *target_score = 0;  /* FONTE.ASM:55 cmp 0; je @@exit */
    }
}

/* --------------------------------------------------------------------------
 * game_init
 *
 * MAIN.ASM:995  start_new_game:
 *   mov eax,start_level → mov current_level,eax
 *   mov eax,nbs_ball_start → mov player_1.player_nbs_ball,eax
 *   call init_score
 * -------------------------------------------------------------------------- */
void game_init(Game *g, Assets *assets, AudioState *audio, Difficulty diff, int mode) {
    memset(g, 0, sizeof(*g));

    g->assets     = assets;
    g->audio      = audio;
    g->difficulty = diff;
    g->game_mode  = mode;
    g->world      = 0;                    /* default world 0 (Blaster.lv0) */

    /* MAIN.ASM:1001-1006  start_new_game: initialise player fields */
    g->lives      = NBS_BALL_START;       /* FILE.ASM:1137  nbs_ball_start dd 2 */
    g->score      = 0;                    /* init_score clears counter */
    g->level_num  = 1;                    /* FILE.ASM:1138  start_level dd 1 */
    g->bonus_life_threshold = BONUS_EXTRA_LIFE; /* FILE.ASM:1143  bonus_extra_life dd 10000 */

    g->state      = STATE_READY_TO_PLAY;  /* MAIN.ASM:118  mov game_mode,READY_TO_PLAY */

    /* Demo mode off by default — set by caller for attract mode */
    g->demo_active = 0;
    g->demo_timer  = DELAI_DEMO;          /* Blaster.inc:420  DELAI_DEMO = 800 */

    /* Speed timer for first level */
    g->speed_level   = compute_speed_level(1);  /* MAIN.ASM:4906-4911 */
    g->speed_counter = g->speed_level * 3;      /* 3x: compensate for 60fps vs original 18fps */

    g->option_spawn_timer = 0;

    /* Active powerup state — all off */
    g->magnetic_flag      = 0;  /* P1-ASM-12: per-player bitmask, MAIN.ASM:3316 */
    g->iron_active        = 0;
    /* laser / reverse / size state now per-paddle (Paddle.laser_timer,
     * reverse_timer, size_timer) — initialised by paddle_init below. */
    g->night_active       = 0;
    g->ghost_active       = 0;
    g->current_option_count = 0;
    g->current_option     = POWERUP_COUNT;  /* sentinel: no active timed powerup */

    g->ball_count     = 0;
    g->powerup_count  = 0;
    g->proj_count     = 0;
    g->frame          = 0;

    /* Dev test mode — off by default, toggle with F9 */
    g->dev_test          = 0;
    g->dev_powerup_cycle = 0;

    /* Initialise paddle at centre of play area.
     * MAIN.ASM:1037  call init_cursor */
    paddle_init(&g->paddle);

    /* Multiplayer (game_mode > 0): init paddle_2 offset from paddle_1.
     * MAIN.ASM:1004-1006  player_2 init. */
    paddle_init(&g->paddle_2);
    if (g->game_mode > 0) {
        /* MAIN.ASM:1742-1748  Init_Cursor duel split:
         *   cursor_1.pos_x = screen_center + screen_center/4 - vaisseau_size_x/2  (right)
         *   cursor_2.pos_x = screen_center - screen_center/4 - vaisseau_size_x/2  (left)
         * Both keep pos_y = limite_y (set by paddle_init). */
        g->paddle.x    = SCREEN_CENTER + SCREEN_CENTER / 4 - g->paddle.w / 2;
        g->paddle_2.x  = SCREEN_CENTER - SCREEN_CENTER / 4 - g->paddle_2.w / 2;
    }
    g->score_2 = 0;
    g->lives_2 = NBS_BALL_START;
    g->bonus_life_threshold_2 = BONUS_EXTRA_LIFE;

    /* Load default hiscores.
     * hiscore_load would be called externally for saved scores. */
    hiscore_init_defaults(&g->hiscores);

    /* Zero all bricks — will be filled by game_load_level */
    memset(g->bricks, 0, sizeof(g->bricks));
}

/* --------------------------------------------------------------------------
 * game_load_level
 *
 * MAIN.ASM:1001  mov eax,start_level → mov current_level,eax
 * MAIN.ASM:4906-4911  speed_level computation
 * -------------------------------------------------------------------------- */
void game_load_level(Game *g, int level_num) {
    char path[256];
    int i;
    int load_rc;

    g->level_num = level_num;

    /* Build path to level file.
     * FILE.ASM:1195-1196  "mov eax,'.lv?'; mov al,world" — world char appended */
    snprintf(path, sizeof(path), ASSETS_BASE "levels/Blaster.lv%d", g->world);

    /* Load raw level data (390 brick bytes) */
    load_rc = level_load(&g->current_level, path, level_num);
    if (load_rc != 0) {
        /* World 2 assets are lowercase in this repo (blaster.lv2). */
        snprintf(path, sizeof(path), ASSETS_BASE "levels/blaster.lv%d", g->world);
        load_rc = level_load(&g->current_level, path, level_num);
    }
    if (load_rc != 0) {
        /* Level load failed — zero all bricks, continue with empty level */
        for (i = 0; i < BRICK_COUNT; i++) {
            brick_init(&g->bricks[i], i, ABSENTE);
        }
        return;
    }

    /* Initialise Brick structs from raw bytes.
     * MAIN.ASM:4882-4883  screen XY computed from col/row */
    for (i = 0; i < BRICK_COUNT; i++) {
        brick_init(&g->bricks[i], i, g->current_level.bricks[i]);
    }

    /* Clear any active timed powerup effects when the level changes.
     * Prevents slow-ball/fast-ball/etc. carrying over to the next level. */
    deactivate_current_option(g);
    /* Iron ball is permanent (not timed) — clear it explicitly on level change. */
    g->iron_active = 0;
    for (i = 0; i < g->ball_count; i++) g->balls[i].is_iron = 0;
    /* Per-paddle laser / size / reverse state — not tied to current_option. */
    g->paddle.has_gun        = 0;
    g->paddle.laser_timer    = 0;
    g->paddle.mini_laser     = 0;
    g->paddle.size_timer     = 0;
    g->paddle.reverse_timer  = 0;
    g->paddle.reversed       = 0;
    paddle_set_size(&g->paddle, PADDLE_SIZE_NORMAL);
    g->paddle_2.has_gun      = 0;
    g->paddle_2.laser_timer  = 0;
    g->paddle_2.mini_laser   = 0;
    g->paddle_2.size_timer   = 0;
    g->paddle_2.reverse_timer = 0;
    g->paddle_2.reversed     = 0;
    paddle_set_size(&g->paddle_2, PADDLE_SIZE_NORMAL);

    /* Recompute speed_level for this level.
     * MAIN.ASM:4906-4911  speed_level = speed_delai - (current_level-1)*17 */
    g->speed_level   = compute_speed_level(level_num);
    g->speed_counter = g->speed_level * 3;  /* 3x: compensate for 60fps vs original 18fps */

    /* Reset falling powerups when level changes */
    memset(g->powerups, 0, sizeof(g->powerups));
    g->powerup_count = 0;

    /* Reset projectiles */
    memset(g->projectiles, 0, sizeof(g->projectiles));
    g->proj_count = 0;

    /* Clear per-player ball-lost pending flags on level change. */
    g->p1_ball_lost_pending = 0;
    g->p2_ball_lost_pending = 0;

    /* Init monster system for this level.
     * MAIN.ASM:2927  call init_Monster (selects sprite variant) */
    monster_init_level(g->monsters, level_num);
    g->monster_spawn_counter = 0;

    /* Reset break animations */
    memset(g->break_anims, 0, sizeof(g->break_anims));
}

/* --------------------------------------------------------------------------
 * game_spawn_ball
 *
 * Place ball 0 on paddle centre, magnetic, state → READY_TO_PLAY.
 * MAIN.ASM:5173-5231  init_start_game:
 *   ball.pos_y = cursor.pos_y - ball.size_y
 *   ball.pos_x = cursor.pos_x + cursor.size_x/2 - ball.size_x/2
 *   set start_flag → magnetic_flag  (ball stays on paddle)
 * -------------------------------------------------------------------------- */
void game_spawn_ball(Game *g) {
    int bx, by;

    /* MAIN.ASM:5187-5194  ball X: paddle_x + paddle_w/2 - ball_w/2 */
    bx = g->paddle.x + g->paddle.w / 2 - BALL_W / 2;
    /* MAIN.ASM:5181-5184  ball Y: paddle_y - ball_h */
    by = g->paddle.y - BALL_H;

    ball_init(&g->balls[0], bx, by);
    g->balls[0].is_magnetic = 1; /* MAIN.ASM:5217  or magnetic_flag,PLAYER_ONE */
    g->balls[0].owner = 0;       /* P1 */
    g->ball_count = 1;

    /* Multiplayer: spawn a second ball on paddle_2 owned by P2. */
    if (g->game_mode > 0) {
        int bx2 = g->paddle_2.x + g->paddle_2.w / 2 - BALL_W / 2;
        int by2 = g->paddle_2.y - BALL_H;
        ball_init(&g->balls[1], bx2, by2);
        g->balls[1].is_magnetic = 1;
        g->balls[1].owner = 1;   /* P2 */
        g->ball_count = 2;
    }

    /* Bug #5: Do NOT set g->state here.  Callers set the appropriate state:
     *   - STATE_READY_TO_PLAY      for first spawn (game_init / level advance)
     *   - STATE_READY_TO_PLAY_AGAIN for respawn after losing a life (MAIN.ASM:4695) */
}

/* --------------------------------------------------------------------------
 * game_active_ball_count
 * -------------------------------------------------------------------------- */
int game_active_ball_count(const Game *g) {
    int i, n = 0;
    for (i = 0; i < g->ball_count; i++) {
        if (g->balls[i].active) n++;
    }
    return n;
}

/* --------------------------------------------------------------------------
 * game_level_complete
 *
 * MAIN.ASM:4120-4143  detect_next_level:
 *   Scan level_tbl; if any non-absente, non-incassable byte found → not done.
 *   (absente = 0x00, incassable = 0x08; anything >= incassable is indestructible/special)
 *   When scan reaches sentinel (-1 / 0xFF) without finding breakable brick → complete.
 * -------------------------------------------------------------------------- */
int game_level_complete(const Game *g) {
    int i;
    /* MAIN.ASM:4128-4138  @@again: inc esi; cmp B[esi-1],-1 je @@end; cmp B,absente; cmp B,incassable jae @@again */
    for (i = 0; i < BRICK_COUNT; i++) {
        unsigned char b = g->current_level.bricks[i];
        if (b == ABSENTE)  continue;           /* empty cell */
        if (b == INVALIDE) continue;           /* sentinel-like */
        /* MAIN.ASM:4135  cmp B,incassable jae @@again — indestructible/teleporter are >= 0x08 */
        if (BRICK_TYPE(b) == BRICK_INDESTRUCTIBLE) continue;
        if (BRICK_TYPE(b) == BRICK_TELEPORTER)     continue;
        /* Found a breakable brick still alive — level not complete */
        if (g->bricks[i].active && g->bricks[i].hp > 0) return 0;
    }
    return 1;  /* all breakable bricks destroyed */
}

/* --------------------------------------------------------------------------
 * Internal: clear the currently active timed powerup effect.
 * MAIN.ASM:6295-6336  — used when a timed powerup expires, is replaced,
 * or when the player loses a life.
 * -------------------------------------------------------------------------- */
static void deactivate_current_option(Game *g) {
    int i;
    /* P1-ASM-31: MAIN.ASM:6322 play_sound iff_option_off — but only when
     * there is a timed effect to clear. Guard against no-op calls (called
     * by apply_powerup / game_load_level even when nothing is active). */
    int had_option = (g->current_option != POWERUP_COUNT && g->current_option_count > 0);
    if (had_option && g->audio) audio_play(g->audio, SFX_POWERUP_OFF);

    switch (g->current_option) {
    case POWERUP_IRON_BALL:
        g->iron_active = 0;
        for (i = 0; i < g->ball_count; i++) g->balls[i].is_iron = 0;
        break;
    case POWERUP_MAGNETIC:
        /* MAIN.ASM:6322 (iff_option_off) clears magnetic_flag via reset_magnetic
         * which sets sprite_magnetic=Off on all balls (MAIN.ASM:3299-3313). */
        g->magnetic_flag = 0;
        for (i = 0; i < g->ball_count; i++) {
            /* P1-ASM-11: magnetic catch preserved vx/vy — on deactivate the
             * ball simply resumes moving with its retained velocity (MAIN.ASM
             * move_ball falls through to @@ok when sprite_magnetic==Off). */
            g->balls[i].is_magnetic = 0;
        }
        break;
    case POWERUP_GHOST:
        g->ghost_active = 0;
        /* Remove any remaining ghost balls when powerup expires */
        for (i = 0; i < g->ball_count; i++) {
            if (g->balls[i].is_ghost) g->balls[i].active = 0;
        }
        break;
    case POWERUP_COLLISION:
        /* collision_flag is only consulted while current_option ==
         * POWERUP_COLLISION, but reset for hygiene.  Paddle bounds are
         * restored automatically next frame by detect_collision_cursor's
         * @@ok branch (MAIN.ASM:2296-2305). */
        g->collision_flag = 0;
        break;
    /* POWERUP_SHOOT / MINI_SHOOT / SMALL_SHIP / LARGE_SHIP / REVERSE are NOT
     * tracked via current_option — they are per-paddle (Paddle.laser_timer /
     * size_timer / reverse_timer) and ticked down independently in
     * tick_option_timer, so one paddle collecting a shape/laser does not
     * cancel the other's active effect. See MAIN.ASM:6491,6500,6509-6603,6562.
     *
     * POWERUP_NIGHT is also not tracked via current_option (see
     * MAIN.ASM:6641-6663 option_night_p / apply_powerup). Night is cleared
     * unconditionally below, mirroring init_palette's `mov option_night_flag,Off`
     * (MAIN.ASM:6684) called from @@current_option_off (MAIN.ASM:6327). */
    default:
        break;
    }
    g->current_option_count = 0;
    g->current_option = POWERUP_COUNT;
    g->night_active = 0;  /* MAIN.ASM:6684 init_palette clears option_night_flag */
}

/* --------------------------------------------------------------------------
 * Internal: spawn extra balls from paddle centre.
 * MAIN.ASM:option_3_ball_p / option_6_ball_p / option_9_ball_p / option_20_ball_p
 * All four handlers share the same logic — only the count differs.
 * owner: 0=P1, 1=P2 — spawn at owner's paddle, tag balls with owner so
 * brick kills and lost-ball lives attribute correctly in duel mode.
 * -------------------------------------------------------------------------- */
static void spawn_extra_balls(Game *g, int count, int owner) {
    int vx, vy;
    Paddle *owner_paddle = (g->game_mode > 0 && owner == 1) ? &g->paddle_2 : &g->paddle;

    /* Iter 2 fix #8: MAIN.ASM:1567-1583 add_ball reuses inactive slots FIRST,
     * only appending to the end when no gap is available. Scan [0, ball_count)
     * for inactive slots, then fall through to appending.
     * `batch_idx` tracks position within this spawn batch so the fan spread
     * (vx offset) remains identical to before. */
    int spawned = 0;
    int batch_idx = 0;

    /* Pass 1: reuse gaps */
    for (int slot = 0; slot < g->ball_count && spawned < count; slot++) {
        if (g->balls[slot].active) continue;
        ball_init(&g->balls[slot],
                  owner_paddle->x + owner_paddle->w / 2 - BALL_W / 2,
                  owner_paddle->y - BALL_H);
        compute_launch_velocity(g->difficulty, g->level_num, &vx, &vy);
        vx += batch_idx - count / 2;
        ball_set_velocity(&g->balls[slot], vx, vy);
        ball_clamp_speed(&g->balls[slot]);
        g->balls[slot].is_magnetic = 0;
        g->balls[slot].owner = owner;
        spawned++;
        batch_idx++;
    }

    /* Pass 2: append until count reached or array is full */
    int end_cap = BALL_MAX + 1;
    while (spawned < count && g->ball_count < end_cap) {
        int slot = g->ball_count;
        ball_init(&g->balls[slot],
                  owner_paddle->x + owner_paddle->w / 2 - BALL_W / 2,
                  owner_paddle->y - BALL_H);
        compute_launch_velocity(g->difficulty, g->level_num, &vx, &vy);
        vx += batch_idx - count / 2;
        ball_set_velocity(&g->balls[slot], vx, vy);
        ball_clamp_speed(&g->balls[slot]);
        g->balls[slot].is_magnetic = 0;
        g->balls[slot].owner = owner;
        g->ball_count++;
        spawned++;
        batch_idx++;
    }
}

/* --------------------------------------------------------------------------
 * Internal: apply a collected powerup effect to the game state.
 *
 * Instant powerups take effect immediately and clear current_option.
 * Timed powerups set current_option and current_option_count = DELAI_OPTION.
 * Derived from MAIN.ASM powerup handler functions (option_*_p labels).
 * -------------------------------------------------------------------------- */
static void apply_powerup(Game *g, PowerupType type, int collected_by) {
    int i;
    int duration = powerup_duration(type);  /* 0=instant, >0=timed, -1=permanent */
    int is_timed = (duration > 0);
    int is_per_paddle_laser = (type == POWERUP_SHOOT || type == POWERUP_MINI_SHOOT);
    /* Per-paddle size/reverse (MAIN.ASM:6491/6500/6562 option_*_ship_p / option_reverse_p)
     * — like laser, these run independently per player and do not occupy
     * the global current_option slot. */
    int is_per_paddle_shape = (type == POWERUP_SMALL_SHIP ||
                               type == POWERUP_LARGE_SHIP ||
                               type == POWERUP_REVERSE);
    Paddle *owner_paddle = (g->game_mode > 0 && collected_by == 1) ? &g->paddle_2 : &g->paddle;
    /* Duel: route score/life events to the collecting player's counters. */
    int *target_score = (g->game_mode == 2 && collected_by == 1) ? &g->score_2 : &g->score;
    int *target_lives = (g->game_mode == 2 && collected_by == 1) ? &g->lives_2 : &g->lives;

    /* Only one (global) timed option can be active at once.
     * Replacing a timed option must clear the previous effect first.
     * EXCEPTION: SHOOT / MINI_SHOOT / SMALL_SHIP / LARGE_SHIP / REVERSE are
     * per-paddle (MAIN.ASM:6491,6500,6509-6603,6562) — they do NOT occupy
     * current_option and must not wipe any other timed effect nor cancel
     * the other paddle's effect.
     * Iter 2 fix #9: MAGNETIC is ALSO per-player via magnetic_flag bitmask
     * and MUST NOT overwrite current_option (MAIN.ASM:6745-6750 push/pop). */
    int is_per_player_magnetic = (type == POWERUP_MAGNETIC);
    if (is_timed && !is_per_paddle_laser && !is_per_paddle_shape
        && !is_per_player_magnetic
        && g->current_option_count > 0) {
        deactivate_current_option(g);
    }

    /* First clear any conflicting size powerup on the OWNER paddle only —
     * each size powerup overrides the previous on that paddle. */
    if (type == POWERUP_SMALL_SHIP || type == POWERUP_LARGE_SHIP) {
        owner_paddle->size_timer = 0;
        paddle_set_size(owner_paddle, PADDLE_SIZE_NORMAL);
    }

    switch (type) {
    /* ------------------------------------------------------------------
     * Instant: extra balls
     * MAIN.ASM:option_3_ball_p etc. — spawn additional balls from paddle
     * ------------------------------------------------------------------ */
    /* Bring total ball count up to the named target.
     * MAIN.ASM:option_3/6/9/20_ball_p — named by intended total.
     * FIX: was spawning a fixed extra count, which overshoot when multiple
     * balls were already active (e.g. collecting 9-ball with 6 already live
     * gave 14 instead of 9). Now fills up to the target only. */
    case POWERUP_BALL_3:
        /* MAIN.ASM:option_3_ball_p  mov ecx,1; call add_ball — spawns 1 extra */
        spawn_extra_balls(g, 1, collected_by);
        if (g->audio) audio_play(g->audio, SFX_MULTI_BALL);
        break;
    case POWERUP_BALL_6:
        /* MAIN.ASM:option_6_ball_p  mov ecx,3; call add_ball — spawns 3 extra */
        spawn_extra_balls(g, 3, collected_by);
        if (g->audio) audio_play(g->audio, SFX_MULTI_BALL);
        break;
    case POWERUP_BALL_9:
        /* MAIN.ASM:option_9_ball_p  mov ecx,6; call add_ball — spawns 6 extra */
        spawn_extra_balls(g, 6, collected_by);
        if (g->audio) audio_play(g->audio, SFX_MULTI_BALL);
        break;
    case POWERUP_BALL_20:
        /* MAIN.ASM:option_20_ball_p  mov ecx,20; call add_ball
         * add_ball skips active slots without decrementing ecx → fills all
         * inactive slots (up to 20 total). Equivalent to fill-to-max. */
        if (g->ball_count < BALL_MAX + 1) spawn_extra_balls(g, BALL_MAX + 1 - g->ball_count, collected_by);
        if (g->audio) audio_play(g->audio, SFX_MULTI_BALL);
        break;

    /* ------------------------------------------------------------------
     * Instant: life events
     * MAIN.ASM:6404-6416  option_death_p / option_new_life_p
     * ------------------------------------------------------------------ */
    case POWERUP_DEATH:
        /* MAIN.ASM:6414-6416  cmp player_nbs_ball,0; je @@end; dec
         * Per-player: only the collector loses a life. */
        if (*target_lives > 0) (*target_lives)--;
        if (g->audio) audio_play(g->audio, SFX_DEATH_POWERUP);
        break;
    case POWERUP_NEW_LIFE:
        /* MAIN.ASM:6426 option_new_life_p — per-player: only the collector
         * gains the life. MAIN.ASM:6436-6439  inc; cmp NBS_BALL_MAX; clamp. */
        (*target_lives)++;
        if (*target_lives > BALL_MAX) *target_lives = BALL_MAX;
        if (g->audio) audio_play(g->audio, SFX_NEW_LIFE);
        break;

    /* ------------------------------------------------------------------
     * Instant: score events
     * MAIN.ASM:6794-6808 option_bonus_p  mov ecx,8; call inc_score → +80
     *   (easy) / +90 (medium) / +100 (hard), doubled in night mode (P0-ASM-3).
     * MAIN.ASM:6811-6826 option_malus_p  mov ecx,12; call dec_score → -120
     *   (no difficulty bonus in ASM dec_score).
     * ------------------------------------------------------------------ */
    case POWERUP_BONUS:
        inc_score(g, collected_by, 8);   /* MAIN.ASM:6799 */
        break;
    case POWERUP_MALUS:
        dec_score(g, collected_by, 12);  /* MAIN.ASM:6817 */
        break;

    /* ------------------------------------------------------------------
     * Instant: skip to next level
     * MAIN.ASM:option_next_level_p — sets current_option=option_next_level_o
     * then detect_next_level checks that flag.
     * ------------------------------------------------------------------ */
    case POWERUP_NEXT_LEVEL:
        g->state = STATE_NEW_PLAY;  /* trigger level advance in main loop */
        break;

    /* ------------------------------------------------------------------
     * Timed: weapon powerups
     * MAIN.ASM:detect_shoot_player_1  — laser active countdown
     * ------------------------------------------------------------------ */
    case POWERUP_SHOOT:
        /* MAIN.ASM:6512-6542  option_shoot_p — applies to current_player only.
         * Strictly per-paddle: setting owner's laser does NOT touch the other
         * paddle, and does not occupy the global current_option slot. */
        owner_paddle->laser_timer = (duration > 0) ? duration : DELAI_OPTION;
        owner_paddle->mini_laser  = 0;    /* big shoot */
        owner_paddle->has_gun     = 1;
        owner_paddle->gun_cooldown = 0;
        break;
    case POWERUP_MINI_SHOOT:
        /* MAIN.ASM:6557-6603  option_mini_shoot_p — applies to current_player only */
        owner_paddle->laser_timer = (duration > 0) ? duration : DELAI_OPTION;
        owner_paddle->mini_laser  = 1;
        owner_paddle->has_gun     = 1;
        owner_paddle->gun_cooldown = 0;
        break;

    /* ------------------------------------------------------------------
     * Timed: ball state powerups
     * ------------------------------------------------------------------ */
    case POWERUP_SLOW_BALL:
        /* MAIN.ASM:option_slow_ball_p — just ret (no immediate speed change).
         * Ongoing deceleration is applied every frame via tick_speed_counter
         * while current_option == SLOW_BALL (detect_dec_speed equivalent). */
        g->current_option = type;
        g->current_option_count = (duration > 0) ? duration : DELAI_OPTION;
        if (g->audio) audio_play(g->audio, SFX_SPEEDUP);
        break;
    case POWERUP_FAST_BALL:
        /* MAIN.ASM:option_fast_ball_p — just ret (no immediate speed change).
         * Ongoing acceleration is applied every frame via tick_speed_counter
         * while current_option == FAST_BALL (detect_inc_speed equivalent). */
        g->current_option = type;
        g->current_option_count = (duration > 0) ? duration : DELAI_OPTION;
        if (g->audio) audio_play(g->audio, SFX_SPEEDUP);
        break;
    case POWERUP_IRON_BALL:
        /* Iron ball persists until next level (not timed).
         * Ball passes through normal bricks, destroying them without bouncing. */
        g->iron_active = 1;
        for (i = 0; i < g->ball_count; i++) {
            g->balls[i].is_iron = 1;
        }
        /* Do NOT set current_option — iron ball is permanent until next level.
         * It will be cleared by handle_life_lost or game_load_level. */
        break;
    case POWERUP_MAGNETIC:
        /* P1-ASM-12: per-player magnetic.
         * MAIN.ASM:6755-6759  option_magnetic_p:
         *   current_player == player_1 → or magnetic_flag,PLAYER_ONE (=1)
         *   current_player == player_2 → or magnetic_flag,PLAYER_TWO (=2)
         * Only the collecting player's paddle magnetises on next ball-contact.
         *
         * Iter 2 fix #9: MAIN.ASM:6745-6750 push/pop old current_option across
         * this handler — magnetic does NOT occupy the global current_option
         * slot; any previously active timed option keeps running. Only set
         * the magnetic_flag bit. */
        g->magnetic_flag |= (collected_by == 1) ? PLAYER_TWO : PLAYER_ONE;
        break;
    case POWERUP_GHOST:
        /* MAIN.ASM:6776-6790  option_ghost_p:
         *   mov ecx,20
         *   cmp nbs_player,2 ; jne @@cont ; mov ecx,10   ← P1-ASM-7: 10 in 2P modes
         *   call add_ball       → spawn ghost balls
         *   call set_ghost      → mark collecting player's balls ghost
         *                         (set_ghost filters by sprite_player==current_player,
         *                          MAIN.ASM:3343-3348; P1-ASM-12 per-player filter) */
        {
            int ghost_count = (g->game_mode > 0) ? 10 : 20;  /* MAIN.ASM:6779-6783 */
            int start = g->ball_count;
            int end = start + ghost_count;
            if (end > BALL_MAX + 1) end = BALL_MAX + 1;
            for (i = start; i < end; i++) {
                int vx, vy;
                ball_init(&g->balls[i],
                          owner_paddle->x + owner_paddle->w / 2 - BALL_W / 2,
                          owner_paddle->y - BALL_H);
                compute_launch_velocity(g->difficulty, g->level_num, &vx, &vy);
                /* Spread ghost balls in different directions */
                int spread = (i - start) - ghost_count / 2;
                vx += spread;
                ball_set_velocity(&g->balls[i], vx, vy);
                ball_clamp_speed(&g->balls[i]);
                g->balls[i].is_magnetic = 0;
                g->balls[i].is_ghost = 1;  /* mark as ghost/bubble */
                g->balls[i].owner = collected_by;  /* MAIN.ASM:sprite_player */
            }
            g->ball_count = end;
            /* MAIN.ASM:3358  unghost_one_ball — first active ball stays normal */
            /* balls[0] is the original ball and is NOT marked as ghost */
        }
        g->ghost_active = 1;
        /* MAIN.ASM:6788  mov current_option,Off — ghost is INSTANT, not timed.
         * The ghost balls are spawned and the powerup effect ends immediately.
         * Ghost balls self-destruct on contact; ghost_active stays on until
         * all ghost balls are gone or player loses a life. */
        break;

    /* ------------------------------------------------------------------
     * Timed: paddle shape powerups
     * MAIN.ASM:detect_large_cursor / detect_small_cursor
     * ------------------------------------------------------------------ */
    case POWERUP_LARGE_SHIP:
        /* MAIN.ASM:6500 option_large_ship_p — per-player. */
        paddle_set_size(owner_paddle, PADDLE_SIZE_LARGE);
        owner_paddle->size_timer = (duration > 0) ? duration : DELAI_OPTION;
        if (g->audio) audio_play(g->audio, SFX_LARGE_PADDLE);
        break;
    case POWERUP_SMALL_SHIP:
        /* MAIN.ASM:6491 option_small_ship_p — per-player. */
        paddle_set_size(owner_paddle, PADDLE_SIZE_SMALL);
        owner_paddle->size_timer = (duration > 0) ? duration : DELAI_OPTION;
        if (g->audio) audio_play(g->audio, SFX_SMALL_PADDLE);
        break;
    case POWERUP_REVERSE:
        /* MAIN.ASM:6562 option_reverse_p — per-player. */
        owner_paddle->reversed = 1;
        owner_paddle->reverse_timer = (duration > 0) ? duration : DELAI_OPTION;
        break;
    case POWERUP_NIGHT:
        /* MAIN.ASM:6641-6663 option_night_p.
         * The shared collection pipeline (MAIN.ASM:5687-5691) has already
         * set current_option=NIGHT and current_option_count=DELAI_OPTION (600).
         * option_night_p then sets option_night_flag=On (darkens palette +
         * x2 score FONTE.ASM:48-50,92-94) and resets current_option back to
         * Off (line 6659) — but LEAVES current_option_count at 600, so the
         * count keeps ticking and eventually triggers @@current_option_off
         * (MAIN.ASM:6327) which calls init_palette → clears night_flag.
         *
         * Two restoration paths:
         *   1. 600-frame timer expiry via detect_current_option_end.
         *   2. Another timed powerup collected mid-NIGHT → detect_init_palette
         *      (MAIN.ASM:6667) sees night_flag=On + current_option!=Off and
         *      clears night early. */
        if (g->night_active) break;                /* ASM:6644-6645 already-on guard */
        g->night_active = 1;
        g->current_option = POWERUP_COUNT;         /* ASM:6659 current_option=Off */
        g->current_option_count = DELAI_OPTION;    /* ASM:5691 via shared pipeline */
        if (g->audio) audio_play(g->audio, SFX_NIGHT);
        break;

    /* ------------------------------------------------------------------
     * TELEPOD: instantly teleport all active balls to a random active
     * teleporter brick. P1-ASM-13: ONE-SHOT per ASM.
     * MAIN.ASM:6634-6637 option_telepod_p: ret (no-op).
     * MAIN.ASM:2873-2898 Refresh_Ball: if current_option==option_telepod_o
     *   teleports each ball once, then @@reset_current_option clears it.
     * The effective behaviour is: teleport all balls on collection, then
     * immediately clear current_option — no ongoing 600-frame window.
     * ------------------------------------------------------------------ */
    case POWERUP_TELEPOD:
        {
            int tele_bricks[BRICK_COUNT];
            int tele_count = 0;
            int tj;
            for (tj = 0; tj < BRICK_COUNT; tj++) {
                if (g->bricks[tj].active && g->bricks[tj].type == BRICK_TELEPORTER)
                    tele_bricks[tele_count++] = tj;
            }
            if (tele_count > 0) {
                for (tj = 0; tj < g->ball_count; tj++) {
                    if (!g->balls[tj].active) continue;
                    int dest = tele_bricks[GetRandomValue(0, tele_count - 1)];
                    g->balls[tj].x = g->bricks[dest].x;
                    g->balls[tj].y = g->bricks[dest].y;
                    g->balls[tj].telepod_timer = DELAI_TELEPOD;
                }
                if (g->audio) audio_play(g->audio, SFX_TELEPOD);
            }
            /* One-shot: do NOT set current_option. MAIN.ASM @@reset_current_option
             * clears it immediately after the first Refresh_Ball. */
        }
        break;

    /* ------------------------------------------------------------------
     * Monster powerups
     * MAIN.ASM:6731  option_del_monster_p — destroy all active monsters
     * MAIN.ASM:6765  option_add_monster_p — spawn one new monster
     * ------------------------------------------------------------------ */
    case POWERUP_DEL_MONSTER:
        /* Iter 2 fix #6+#7: monster SFX is iff_del_monster → MONSTOFF.wav
         * (= SFX_POWERUP_LOST). MAIN.ASM:3151-3168 loop plays SFX per kill. */
        monster_destroy_all(g->monsters, g->audio);
        break;
    case POWERUP_ADD_MONSTER:
        monster_create(g->monsters, &g->monster_spawn_counter, g->difficulty,
                       g->cfg_monster_delai);
        g->monster_spawn_counter = 0;  /* reset so timer doesn't double-spawn */
        break;

    /* COLLISION: 2P effect (coop + duel) — locks the two cursors so they
     * push each other instead of overlapping.
     * MAIN.ASM:6830-6836 option_collision_p stores (cursor_2.x - cursor_1.x)
     * into collision_flag.  The sign picks which paddle clamps which in
     * detect_collision_cursor (MAIN.ASM:2274-2316, ticked each frame).
     * current_option + current_option_count are set by the shared pipeline
     * below (mirroring MAIN.ASM:5687-5691 detect_prise_option), so the
     * clamp effect auto-expires after DELAI_OPTION frames. */
    case POWERUP_COLLISION:
        if (g->game_mode > 0) {
            g->collision_flag = g->paddle_2.x - g->paddle.x;
        }
        g->current_option = type;
        g->current_option_count = (duration > 0) ? duration : DELAI_OPTION;
        break;
    default:
        break;
    }

    /* F3-iter3-01: MAIN.ASM:5687-5691 detect_prise_option sets
     *   mov current_option,eax              (the picked option_*_o adrs)
     *   mov [ebx.player_current_option],eax
     *   mov current_option_count,DELAI_OPTION
     * UNCONDITIONALLY for every collected powerup (after push/pop old_option
     * at MAIN.ASM:5675-5676). The HUD panel_option then draws the current
     * option icon, and detect_current_option_end fires iff_option_off when
     * the timer hits 0. Exception: MAGNETIC handler (MAIN.ASM:6745-6750
     * push/pop old_option) restores the previous current_option across the
     * call, so the magnetic pickup does NOT occupy the current_option slot.
     * All other handlers either leave it set (timed) or write Off themselves
     * (instant — MAIN.ASM:6352/6367/6382/...), but the initial write here
     * drives the HUD update that iter 2 was missing for instant powerups. */
    if (type != POWERUP_MAGNETIC) {
        g->current_option = type;
        g->current_option_count = DELAI_OPTION;
    }

    (void)duration;  /* suppress warning if unused by a path */
}

/* --------------------------------------------------------------------------
 * Internal: tick active powerup timer.
 * MAIN.ASM:1068  detect_current_option_end
 * MAIN.ASM:6295-6336  detect_current_option_end — dec current_option_count;
 *   when == 0 → clear the current_option effect.
 * -------------------------------------------------------------------------- */
static void tick_option_timer(Game *g) {
    /* Per-paddle laser countdowns — independent from global current_option.
     * MAIN.ASM:1883-1901  dec count_tir_big_1 / count_tir_big_2 etc.
     * When a paddle's laser expires, only that paddle loses its gun. */
    if (g->paddle.laser_timer > 0) {
        g->paddle.laser_timer--;
        if (g->paddle.laser_timer == 0) {
            g->paddle.has_gun = 0;
            g->paddle.mini_laser = 0;
        }
    }
    if (g->paddle_2.laser_timer > 0) {
        g->paddle_2.laser_timer--;
        if (g->paddle_2.laser_timer == 0) {
            g->paddle_2.has_gun = 0;
            g->paddle_2.mini_laser = 0;
        }
    }

    /* Per-paddle size/reverse countdowns — MAIN.ASM:6491,6500,6562 per-player. */
    if (g->paddle.size_timer > 0) {
        g->paddle.size_timer--;
        if (g->paddle.size_timer == 0) paddle_set_size(&g->paddle, PADDLE_SIZE_NORMAL);
    }
    if (g->paddle_2.size_timer > 0) {
        g->paddle_2.size_timer--;
        if (g->paddle_2.size_timer == 0) paddle_set_size(&g->paddle_2, PADDLE_SIZE_NORMAL);
    }
    if (g->paddle.reverse_timer > 0) {
        g->paddle.reverse_timer--;
        if (g->paddle.reverse_timer == 0) g->paddle.reversed = 0;
    }
    if (g->paddle_2.reverse_timer > 0) {
        g->paddle_2.reverse_timer--;
        if (g->paddle_2.reverse_timer == 0) g->paddle_2.reversed = 0;
    }

    /* MAIN.ASM:6667-6677 detect_init_palette — if night is on AND another
     * timed powerup is active, clear night (palette restored on next draw
     * since night_active controls the overlay in draw.c). Ported inline
     * here because detect_init_palette runs every frame in ASM's main loop
     * (MAIN.ASM:1078), same cadence as our tick. */
    if (g->night_active && g->current_option != POWERUP_COUNT) {
        g->night_active = 0;
    }

    if (g->current_option_count <= 0) return;
    g->current_option_count--;
    if (g->current_option_count > 0) return;

    /* Timer expired — deactivate the timed powerup effect.
     * MAIN.ASM:6295-6336  sets current_option=Off and restores defaults.
     * Also clears night_active (init_palette, MAIN.ASM:6327 → 6684). */
    deactivate_current_option(g);
}

/* --------------------------------------------------------------------------
 * Internal: handle ball lost → lose a life (or game over).
 * MAIN.ASM:4595-4712  detect_game_over_player_1 / test_game_over
 *
 * Attributes the life loss to the owner(s) of the lost ball(s):
 *   - Solo: decrement g->lives only
 *   - Coop (game_mode 1): pool to g->lives — both players share per ASM
 *   - Duel (game_mode 2): per-player — owner 0 decrements g->lives,
 *     owner 1 decrements g->lives_2. Clears only the losing paddle's gun state.
 * -------------------------------------------------------------------------- */
static void handle_life_lost(Game *g) {
    int i;
    int p1_lost = g->p1_ball_lost_pending;
    int p2_lost = g->p2_ball_lost_pending;

    /* Fallback: if no pending flags set (e.g. initial state), treat as P1.
     * Also ensure there is at least one decrement. */
    if (!p1_lost && !p2_lost) p1_lost = 1;

    /* Consume the pending flags — lives get decremented below. */
    g->p1_ball_lost_pending = 0;
    g->p2_ball_lost_pending = 0;

    /* MAIN.ASM:4669-4672  clear current_option (shared effects: iron,
     * ghost, night, fast/slow ball, telepod).
     * Iter 2 fix #5: magnetic_flag is NOT cleared here — MAIN.ASM:4669-4672
     * test_game_over does not touch magnetic_flag; _Init_Cursor PUSH/POP
     * preserves it. Combined with fix #9 (magnetic never occupies
     * current_option), the magnetic effect SURVIVES life-lost. It is only
     * reset on full game_init (line ~334) or level advance. */
    deactivate_current_option(g);
    g->ghost_active = 0;  /* ghost is instant, not tracked via current_option */

    /* Per-paddle laser/size/reverse state — cleared on the losing paddle only
     * (P1-1 from audit). ASM test_game_over re-inits the losing cursor only. */
    Paddle *lost_paddles[2] = {NULL, NULL};
    int lost_n = 0;
    if (p1_lost) lost_paddles[lost_n++] = &g->paddle;
    if (p2_lost) lost_paddles[lost_n++] = &g->paddle_2;
    for (i = 0; i < lost_n; i++) {
        Paddle *lp = lost_paddles[i];
        lp->has_gun        = 0;
        lp->laser_timer    = 0;
        lp->mini_laser     = 0;
        lp->gun_cooldown   = 0;
        lp->size_timer     = 0;
        lp->reverse_timer  = 0;
        lp->reversed       = 0;
        paddle_set_size(lp, PADDLE_SIZE_NORMAL);
    }

    /* Clear all falling (uncollected) powerups */
    for (i = 0; i < g->powerup_count; i++) g->powerups[i].active = 0;
    g->powerup_count = 0;

    /* MAIN.ASM:4674-4675  dec player_nbs_ball — per-player dispatch.
     * MAIN.ASM:4595 detect_game_over_player_1 per-player path. */
    if (g->game_mode == 2) {
        /* Duel: only the owner(s) of the lost ball(s) lose a life. */
        if (p1_lost) g->lives--;
        if (p2_lost) g->lives_2--;
    } else {
        /* Solo / coop: both share g->lives (coop pools per ASM). */
        g->lives--;
    }

    int p1_dead = (g->lives   < 0);
    int p2_dead = (g->lives_2 < 0);
    int game_over;
    if (g->game_mode == 0)       game_over = p1_dead;            /* solo */
    else if (g->game_mode == 1)  game_over = p1_dead;            /* coop — shared counter */
    else /* game_mode == 2 */    game_over = p1_dead && p2_dead; /* dual — both out */

    if (game_over) {
        /* MAIN.ASM:4678  mov game_mode,GAME_OVER */
        g->state = STATE_GAME_OVER;
        if (g->audio) audio_play(g->audio, SFX_GAME_OVER);
#if defined(BRICKBLASTER_MOBILE)
        {
            static const int death_pattern[] = {0, 100, 50, 200, 50, 100};
            platform_vibrate_pattern(death_pattern, 6);
        }
#endif
        return;
    }

    /* MAIN.ASM:4695  mov game_mode,READY_TO_PLAY_AGAIN */
    g->state = STATE_READY_TO_PLAY_AGAIN;
#if defined(BRICKBLASTER_MOBILE)
    {
        static const int life_lost_pattern[] = {0, 80, 40, 150};
        platform_vibrate_pattern(life_lost_pattern, 4);
    }
#endif

    /* MAIN.ASM:4707  play_sound iff_restart (P1-ASM-31) */
    if (g->audio) audio_play(g->audio, SFX_RESTART);

    /* Deactivate all balls; keep the array but mark all inactive */
    for (i = 0; i < g->ball_count; i++) {
        g->balls[i].active = 0;
    }
    g->ball_count = 0;

    /* MAIN.ASM:1089  jc start_game — respawn ball on paddle */
    game_spawn_ball(g);
}

/* --------------------------------------------------------------------------
 * Internal: periodic auto-speedup.
 * MAIN.ASM:3387-3409  detect_inc_speed (called from refresh_ball per-ball).
 *
 * ASM flow:
 *   3390  cmp sprite_sens_y,0; jns @@cont  — if vy>=0, skip counter decrement
 *   3392  dec sprite_speed_counter
 *   3393  cmp sprite_speed_counter,0; je @@detect_inc_speed_option  — counter hit 0
 *   3395 @@cont:
 *   3396  cmp current_option,option_fast_ball_o; jne @@end  — fast_ball check
 *   3398 @@detect_inc_speed:
 *   3399  mov sprite_speed_counter,speed_level  — reset counter
 *   3401  call inc_speed_x; call inc_speed_y    — apply speed increase
 *
 * Key: speed only auto-applies when (counter hits 0) OR (fast_ball powerup active).
 * With fast_ball: applied every ASM frame. Original game ran at ~18fps; this port
 * runs at 60fps, so fast/slow ball changes are rate-limited to every 3 frames to
 * preserve the original feel (60/18 ≈ 3).
 * detect_dec_speed (MAIN.ASM:3410-3421): same logic but decrement, for slow_ball.
 * -------------------------------------------------------------------------- */
static void tick_speed_counter(Game *g) {
    int i;

    /* MAIN.ASM:detect_dec_speed:3410-3421 — slow_ball: decelerate every ASM
     * frame. Rate-limited to every 3 frames to match 60fps / 18fps ratio.
     * Also reset per-ball speed_counter (MAIN.ASM:3418 mov [edx.sprite_speed_counter],eax). */
    if (g->current_option == POWERUP_SLOW_BALL && (g->frame % 3) == 0) {
        for (i = 0; i < g->ball_count; i++) {
            if (!g->balls[i].active) continue;
            g->balls[i].speed_counter = g->speed_level * 3;  /* F1-C per-ball */
            {
                /* dec_speed_x: MAIN.ASM:3460-3475 */
                int min_x = SPEED_MIN_X + g->balls[i].angle;
                if (g->balls[i].vx > min_x)       g->balls[i].vx--;
                else if (g->balls[i].vx < -min_x) g->balls[i].vx++;
            }
            {
                /* dec_speed_y: MAIN.ASM:3479-3493 */
                int min_y = SPEED_MIN_Y - g->balls[i].angle;
                if (g->balls[i].vy > min_y)        g->balls[i].vy--;
                else if (g->balls[i].vy < -min_y)  g->balls[i].vy++;
            }
        }
        /* Keep global mirror in sync for UI/debug. */
        g->speed_counter = g->speed_level * 3;
        return;
    }

    /* F1-C: Per-ball auto-speedup counter (MAIN.ASM:3388-3394):
     *   for each ball: if sens_y<0 (moving up), dec speed_counter;
     *                  if counter hits 0, apply inc_speed to THAT ball
     *                  and reset counter to speed_level.
     * This is the ASM-faithful per-ball cadence — previously we used one
     * global counter which coupled multi-ball speed-up timing. */
    int fast_active = (g->current_option == POWERUP_FAST_BALL);
    int fast_cap_x = fast_active ? (SPEED_MAX_X / 2) : 0;
    int fast_cap_y = fast_active ? (SPEED_MAX_Y / 2) : 0;
    /* MAIN.ASM:3395-3397 @@cont: check fast_ball powerup. If active, apply
     * every frame (sub-sampled to every 32 here for playability — 60fps
     * would reach SPEED_MAX in <1s otherwise). */
    int fast_pulse = (fast_active && (g->frame % 32) == 0);

    for (i = 0; i < g->ball_count; i++) {
        if (!g->balls[i].active) continue;
        /* F1-C lazy seed: ball_init sets speed_counter=0 (doesn't know
         * speed_level). First tick where we need a value, seed it.
         * MAIN.ASM:1474-1475 spawns with speed_delai; we use the per-level
         * speed_level*3 to match the ASM 18fps→60fps scaling. */
        if (g->balls[i].speed_counter <= 0 && g->balls[i].vy >= 0) {
            g->balls[i].speed_counter = g->speed_level * 3;
        }
        int apply = 0;
        /* MAIN.ASM:3390-3394 per-ball decrement gate. */
        if (g->balls[i].vy < 0) {
            /* If ctr is still 0 (never seeded) seed with speed_level*3 before
             * first decrement, matching the ASM which spawns with
             * sprite_speed_counter = speed_delai (MAIN.ASM:1475). */
            if (g->balls[i].speed_counter <= 0) {
                g->balls[i].speed_counter = g->speed_level * 3;
            }
            g->balls[i].speed_counter--;
            if (g->balls[i].speed_counter <= 0) apply = 1;
        }
        if (fast_pulse) apply = 1;
        if (!apply) continue;

        /* MAIN.ASM:3398-3402 @@detect_inc_speed:
         *   mov eax,speed_level
         *   mov [edx.sprite_speed_counter],eax
         *   call inc_speed_x / inc_speed_y
         * Multiply by 3 to compensate 60fps vs ASM 18fps. */
        g->balls[i].speed_counter = g->speed_level * 3;
        {
            int lim_x = (fast_cap_x > 0) ? fast_cap_x : (SPEED_MAX_X + g->balls[i].angle);
            if (g->balls[i].vx > 0 && g->balls[i].vx < lim_x)
                g->balls[i].vx++;
            else if (g->balls[i].vx < 0 && g->balls[i].vx > -lim_x)
                g->balls[i].vx--;
        }
        {
            int lim_y = (fast_cap_y > 0) ? fast_cap_y : (SPEED_MAX_Y - g->balls[i].angle);
            if (g->balls[i].vy > 0 && g->balls[i].vy < lim_y)
                g->balls[i].vy++;
            else if (g->balls[i].vy < 0 && g->balls[i].vy > -lim_y)
                g->balls[i].vy--;
        }
    }

    /* Keep global mirror — many HUD/debug paths still read g->speed_counter.
     * Mirror the minimum remaining time across active balls so the readout
     * shows the next scheduled speedup. */
    int min_ctr = g->speed_level * 3;
    for (i = 0; i < g->ball_count; i++) {
        if (!g->balls[i].active) continue;
        if (g->balls[i].speed_counter > 0 && g->balls[i].speed_counter < min_ctr)
            min_ctr = g->balls[i].speed_counter;
    }
    g->speed_counter = min_ctr;
}

/* --------------------------------------------------------------------------
 * projectile_hit_brick — check one projectile against the brick grid.
 *
 * ASM ground truth: MAIN.ASM:1083  call refresh_shoot
 *   refresh_shoot iterates each active tir (projectile) and checks whether
 *   the tir's top-centre pixel falls inside a brick cell.  If so it calls
 *   detect_brique (MAIN.ASM:3947) which decrements HP and possibly destroys.
 *
 * Collision test (derived from MAIN.ASM:3947-4004  detect_brique):
 *   1. Convert projectile top-centre to grid col/row
 *      col = (px - bord_x) / brique_size_x      MAIN.ASM:3962  shr ebx,5
 *      row = (py - bord_y) / brique_size_y       MAIN.ASM:3969  shr eax,4
 *   2. Skip empty (absente, MAIN.ASM:3977) / invalid (invalide, MAIN.ASM:3979)
 *   3. Indestructible bricks bounce the projectile but take no damage
 *      (MAIN.ASM:3981  cmp al,incassable  jae @@collision)
 *   4. Normal bricks: dec HP (MAIN.ASM:3996  dec B [esi+ebx])
 *   5. Destroyed when HP==0 (MAIN.ASM:4004  cmp B,absente)
 *
 * The projectile is consumed (active=0) on ANY solid brick hit.
 * Transparent bricks are not solid — projectile passes through.
 *
 * Returns 1 if the projectile was consumed, 0 otherwise.
 * -------------------------------------------------------------------------- */
static int projectile_hit_brick(Game *g, Projectile *p)
{
    int px, py, pw, col, row, idx;
    Brick *b;
    int destroyed, score_delta;

    /* Top-centre of projectile sprite — matches ASM sprite origin check.
     * MAIN.ASM:3962  the x coordinate used is the centre of the sprite */
    pw = p->is_big ? VAISSEAU_TIR_BIG_SIZE_X : VAISSEAU_TIR_SIZE_X;
    px = p->x + pw / 2;
    py = p->y;  /* top edge of projectile (moving upward) */

    /* Bounds check: must be inside the play area brick grid.
     * MAIN.ASM:3497  bord_x (112) .. limite_x (528) */
    if (px < PLAY_X1 || px >= PLAY_X2) return 0;
    if (py < PLAY_Y1 || py >= SCREEN_H) return 0;

    /* Grid cell from pixel — MAIN.ASM:3962-3969 */
    col = (px - BRICK_ORIGIN_X) / BRICK_W;   /* MAIN.ASM:3962  shr ebx,5 */
    row = (py - BRICK_ORIGIN_Y) / BRICK_H;   /* MAIN.ASM:3969  shr eax,4 */
    idx = row * BRICK_COLS + col;
    if (idx < 0 || idx >= BRICK_COUNT) return 0;

    b = &g->bricks[idx];

    /* Skip empty / invalid cells — MAIN.ASM:3977-3979 */
    if (!b->active || b->raw == ABSENTE || b->raw == INVALIDE) return 0;

    /* Apply damage — MAIN.ASM:3996  dec B [esi+ebx] (damage = 1) */
    destroyed = brick_hit(b, 1);

    /* Sound — MAIN.ASM:4032 / MAIN.ASM:4055 */
    if (g->audio) {
        if (b->type == BRICK_INDESTRUCTIBLE) {
            audio_play(g->audio, SFX_WALL_HIT);
        } else {
            audio_play(g->audio, SFX_BRICK_HIT);
        }
    }
#if defined(BRICKBLASTER_MOBILE)
    platform_vibrate(10);
#endif

    /* Update raw level data so game_level_complete() stays accurate */
    if (destroyed) {
        g->current_level.bricks[idx] = ABSENTE;
    }

    /* P0-ASM-1: projectile brick destruction uses inc_score helper
     * (difficulty + night scaling + bonus-life trigger + +80 bonus).
     * MAIN.ASM:4024-4026 call inc_score ecx=1. */
    if (!g->demo_active && destroyed) {
        inc_score(g, p->owner, 1);
    }
    (void)score_delta;  /* kept for compat; unused after helper */

    /* Powerup spawn on destroy — MAIN.ASM:1067 detect_new_option
     * FILE.ASM:1139  delai_between_option = 60 frames */
    if (destroyed && g->powerup_count < MAX_POWERUPS_ACTIVE) {
        int limit = powerup_spacing(g);
        int spawn_ok = g->dev_test || (g->option_spawn_timer >= limit);
        if (spawn_ok) {
            /* F6-01: consult cfg-injected freq table first, fall back to
             * hardcoded powerup_freq_table[] if no override present. */
            PowerupType pt = g->dev_test ? dev_next_powerup(g)
                                         : pick_powerup_cfg(g);
            if (pt < POWERUP_COUNT) {
                int bx = level_brick_x(idx);
                int by = level_brick_y(idx);
                powerup_init(&g->powerups[g->powerup_count], pt, bx, by);
                /* F5 P1-ASM-35: tag with destroyer's owner (projectile path). */
                g->powerups[g->powerup_count].owner = p->owner;
                g->powerup_count++;
                g->option_spawn_timer = 0;
            }
        }
    }

    /* Big shoot (POWERUP_SHOOT): penetrates all breakable bricks, travels to top.
     * MAIN.ASM refresh_shoot — big projectile is a laser beam through the brick column.
     * Only stops at BRICK_INDESTRUCTIBLE walls.
     * Mini shoot (POWERUP_MINI_SHOOT): consumed on first solid brick hit. */
    if (p->is_big && b->type != BRICK_INDESTRUCTIBLE) {
        return 0;   /* projectile survives, keeps moving up */
    }
    p->active = 0;
    return 1;
}

/* --------------------------------------------------------------------------
 * Internal: dev test powerup spawn — guaranteed sequential spawn.
 * Returns the next powerup type to spawn, cycling through all 24.
 * Skips COLLISION (23) since it has no effect in single-player.
 * -------------------------------------------------------------------------- */
static PowerupType dev_next_powerup(Game *g) {
    PowerupType pt = (PowerupType)(g->dev_powerup_cycle % (POWERUP_COUNT - 1));
    g->dev_powerup_cycle = (g->dev_powerup_cycle + 1) % (POWERUP_COUNT - 1);
    return pt;
}

/* --------------------------------------------------------------------------
 * detect_collision_cursor  — MAIN.ASM:2274-2316
 *
 * 1P solo: no-op (paddles never collide).
 * 2P (coop or duel):
 *   - POWERUP_COLLISION active: update paddle clamps so the cursors push
 *     each other, using the sign of collision_flag (set at pickup) to decide
 *     which paddle is on which side.
 *   - Otherwise: restore both paddles' min_x / max_x to the normal play-area
 *     bounds (the @@ok branch MAIN.ASM:2296-2305).
 *
 * Called each frame from game_update before the paddle input step so
 * paddle_clamp (invoked from paddle_move) reads the up-to-date bounds.
 * -------------------------------------------------------------------------- */
static void detect_collision_cursor(Game *g) {
    Paddle *p1 = &g->paddle;
    Paddle *p2 = &g->paddle_2;

    if (g->game_mode == 0) return;             /* MAIN.ASM:2277 cmp nbs_player,2 */

    if (g->current_option != POWERUP_COLLISION) {
        /* @@ok: MAIN.ASM:2296-2305 — reset to normal play-area bounds. */
        p1->min_x = PLAY_X1;
        p2->min_x = PLAY_X1;
        p1->max_x = PLAY_X2 - p1->w;
        p2->max_x = PLAY_X2 - p2->w;
        return;
    }

    if (g->collision_flag >= 0) {
        /* P2 on right of P1 (MAIN.ASM:2285-2292) — P2's left edge clamps
         * to P1's right edge; P1's right edge clamps to P2's left edge. */
        p2->min_x = p1->x + p1->w;
        p1->max_x = p2->x - p1->w;
    } else {
        /* P2 on left of P1 — @@inverse MAIN.ASM:2308-2315. */
        p1->min_x = p2->x + p2->w;
        p2->max_x = p1->x - p2->w;
    }
}

/* --------------------------------------------------------------------------
 * game_update
 *
 * Per-frame game logic.  Must be called once per frame at 60 Hz.
 * Mirrors MAIN.ASM:1061-1175 "main:" frame loop.
 * -------------------------------------------------------------------------- */
void game_update(Game *g, const FrameInput *input) {
    int i, j, dx, any_alive;
    BrickCollision bc;
    int fire_pressed, score_delta;

    g->frame++;

    /* -----------------------------------------------------------------------
     * DEV TEST MODE: F9 toggle or auto-load on first frame if flag set.
     * Loads a test level with all brick types, forces powerup drops.
     * ----------------------------------------------------------------------- */
    if (input->dev_f9_pressed) {
        g->dev_test = !g->dev_test;
        if (g->dev_test) {
            load_dev_test_level(g);
            g->lives = BALL_MAX;
            g->dev_powerup_cycle = 0;
        }
    }
    /* Auto-load test level on first frame when started from menu button */
    if (g->dev_test && g->frame == 1) {
        load_dev_test_level(g);
        g->lives = BALL_MAX;
        g->dev_powerup_cycle = 0;
    }

    /* -----------------------------------------------------------------------
     * GATE: only run gameplay update when actually playing or ready
     * MAIN.ASM:1064  Send game_mode,10,6 — state telemetry
     * ----------------------------------------------------------------------- */
    if (g->state != STATE_PLAYING &&
        g->state != STATE_READY_TO_PLAY &&
        g->state != STATE_READY_TO_PLAY_AGAIN) {
        return;
    }

    /* =======================================================================
     * STEP 0: Active powerup timer tick — FIRST thing every frame
     * MAIN.ASM:1068  call detect_current_option_end  (before refresh_mouse)
     * ======================================================================= */
    tick_option_timer(g);

    /* Powerup spawn cooldown: increment every frame, not per brick destroy.
     * MAIN.ASM:1067  call detect_new_option
     * FILE.ASM:1139-1141  Time_Between_Option per difficulty (P1-ASM-34). */
    {
        int limit = powerup_spacing(g);
        if (g->option_spawn_timer < limit) g->option_spawn_timer++;
    }

    /* =======================================================================
     * STEP 1: INPUT — move paddle (or demo AI if demo_active)
     * MAIN.ASM:1080  call refresh_mouse (MOUSE.ASM)
     * MOUSE.ASM:49-72  keyboard_left: sub speed_counter; keyboard_right: add speed_counter
     * MOUSE.ASM:77  speed_counter dd 6 — paddle speed = 6 px/frame
     * ======================================================================= */
    g->paddle.prev_x   = g->paddle.x;    /* save for magnetic ball tracking */
    g->paddle_2.prev_x = g->paddle_2.x;  /* same for P2 in MP modes */

    /* MAIN.ASM:1079  call detect_collision_cursor — runs before refresh_mouse
     * so paddle_clamp reads up-to-date min_x / max_x bounds. */
    detect_collision_cursor(g);

    dx = 0;
    /* MAIN.ASM:1154-1155  cmp B[ebp+key_p],On → pause (not handled here) */

    if (g->demo_active) {
        /* DEMO MODE: AI controls paddle, ignore real input.
         * MAIN.ASM:5131-5144  demo_move_x_player_1:
         *   paddle.x = ball.x - paddle_w/2  (track ball centre) */
        demo_update_paddle(g);

        /* MAIN.ASM:1163 call read_click; jnz @@exit — LEVEL-triggered. */
        if (input->any_input) {
            g->demo_active = 0;
            g->state       = STATE_MENU;
            return;
        }
    } else {
        /* NORMAL MODE: read input from FrameInput (platform-agnostic) */

        /* Discrete directional input (keyboard, D-pad, mobile buttons) */
        if (input->move_left)  dx -= (int)(g->paddle.speed * input->button_speed_mul);
        if (input->move_right) dx += (int)(g->paddle.speed * input->button_speed_mul);

        /* Analog stick: proportional paddle speed */
        if (input->stick_x != 0.0f)
            dx += (int)(input->stick_x * g->paddle.speed);

        /* Wear OS crown rotary encoder */
        if (input->crown_delta > 0.05f || input->crown_delta < -0.05f)
            dx += (int)(input->crown_delta * g->paddle.speed * 2.0f);

        /* Accelerometer tilt */
        if (input->tilt_x != 0.0f)
            dx += (int)(input->tilt_x * g->paddle.speed * input->tilt_speed_mul);

        /* Pointer snap: absolute paddle positioning from mouse/touch.
         * pointer_game_x is already in game coordinates (post-letterbox). */
        if (input->pointer_active) {
            float game_x = input->pointer_game_x;
            /* MOUSE.ASM:33-47  POWERUP_REVERSE: mirror pointer around screen centre */
            if (g->paddle.reversed) game_x = SCREEN_W - game_x;
            g->paddle.x = (int)game_x - g->paddle.w / 2;
            paddle_clamp(&g->paddle);
            dx = 0;  /* absolute pointer overrides keyboard delta */
            g->demo_timer = DELAI_DEMO;
        }

        if (dx != 0) {
            g->demo_timer = DELAI_DEMO;
        }

        /* paddle_move handles reversal via p->reversed (MOUSE.ASM:33-47).
         * POWERUP_REVERSE sets paddle.reversed=1; paddle_move negates dx when set.
         * Do NOT negate dx here — that would double-negate and cancel the effect. */
        paddle_move(&g->paddle, dx);
    }

    /* Multiplayer: move paddle_2 with P2 input. */
    if (g->game_mode > 0) {
        int dx2 = 0;
        if (input->p2_left)  dx2 -= (int)g->paddle_2.speed;
        if (input->p2_right) dx2 += (int)g->paddle_2.speed;
        if (input->p2_stick_x != 0.0f) dx2 += (int)(input->p2_stick_x * g->paddle_2.speed);
        paddle_move(&g->paddle_2, dx2);
    }

    /* =======================================================================
     * STEP 2: Fire ball if READY_TO_PLAY
     * MAIN.ASM:1081  call detect_start_game
     * ======================================================================= */
    fire_pressed = input->fire_pressed;
    if (fire_pressed) {
        g->demo_timer = DELAI_DEMO;   /* any input resets demo timer */
    }

    /* Iter 2 fix #1: P2 ball must also launch in coop/duel. MAIN.ASM:5280-5349
     * detect_start_game fires BOTH ball_1 and ball_2 via read_click_player_1
     * and read_click_player_2 independently. Each ball launches on its owner's
     * fire input.  In solo mode only P1 exists; any fire counts. */
    {
        int p1_fire = fire_pressed;
        int p2_fire = (g->game_mode > 0) ? input->p2_fire : 0;
        if ((g->state == STATE_READY_TO_PLAY || g->state == STATE_READY_TO_PLAY_AGAIN)
            && (p1_fire || p2_fire)) {
            /* Launch ball(s): MAIN.ASM:5290  mov game_mode,PLAYING */
            g->state = STATE_PLAYING;
            for (i = 0; i < g->ball_count; i++) {
                if (!g->balls[i].active) continue;
                /* Each ball launches on its owner's fire press. In solo the
                 * only ball is owner==0 and p1_fire governs it. */
                int owner = g->balls[i].owner;
                int owner_fire = (owner == 1) ? p2_fire : p1_fire;
                if (!owner_fire) continue;
                {
                    int vx, vy;
                    compute_launch_velocity(g->difficulty, g->level_num, &vx, &vy);
                    ball_set_velocity(&g->balls[i], vx, vy);
                    g->balls[i].is_magnetic = 0;
                }
            }
        }
    }

    /* While ball is on paddle, keep it centred on paddle X.
     * MAIN.ASM:5246-5261  detect_start_game: ball follows cursor
     *
     * Bug #5 fix: if balls[0] is not active during READY_TO_PLAY states,
     * reinitialise defensively so the ball always tracks the paddle. */
    if (g->state == STATE_READY_TO_PLAY || g->state == STATE_READY_TO_PLAY_AGAIN) {
        if (g->ball_count < 1 || !g->balls[0].active) {
            /* Defensive recovery — re-spawn ball on paddle */
            int bx = g->paddle.x + g->paddle.w / 2 - BALL_W / 2;
            int by = g->paddle.y - BALL_H;
            ball_init(&g->balls[0], bx, by);
            g->balls[0].is_magnetic = 1;
            g->ball_count = 1;
        }
        /* Pin each un-launched ball to ITS OWNER'S paddle (P1 → paddle,
         * P2 → paddle_2). Iter 2 fix #1: mirrors detect_start_game in
         * MAIN.ASM:5246-5280 which updates ball_1 and ball_2 independently. */
        for (i = 0; i < g->ball_count; i++) {
            if (!g->balls[i].active) continue;
            if (!g->balls[i].is_magnetic) continue;
            Paddle *own = (g->game_mode > 0 && g->balls[i].owner == 1) ? &g->paddle_2 : &g->paddle;
            g->balls[i].x = own->x + own->w / 2 - BALL_W / 2;
            g->balls[i].y = own->y - BALL_H;
        }
        if (g->state == STATE_READY_TO_PLAY) {
            return;   /* first launch: nothing else to update */
        }
        /* STATE_READY_TO_PLAY_AGAIN: do NOT return — ball is on paddle with
         * vx=0,vy=0 (safe through physics), but falling powerups and monster
         * explosion animations must continue ticking. */
    }

    /* =======================================================================
     * STEP 2c: Magnetic ball re-launch on fire.
     * MAIN.ASM:5246-5280  When magnetic is active and ball is attached (vx==0,vy==0),
     * pressing fire launches the ball upward from the paddle.
     * Per-ball: select owning paddle + fire input so P2's magnetic ball launches
     * from paddle_2 on input->p2_fire.
     * ======================================================================= */
    /* P1-ASM-11: magnetic catch now preserves vx/vy. On fire, ASM move_ball
     * (MAIN.ASM:3238-3247) clears sprite_magnetic AND clears the magnetic_flag
     * bit for the firing player, then falls through so the ball moves by its
     * retained velocity. We keep is_magnetic as a flag; ball.vy was already
     * flipped to point upward by collision_paddle. */
    if (g->magnetic_flag && g->state == STATE_PLAYING) {
        for (i = 0; i < g->ball_count; i++) {
            if (g->balls[i].active && g->balls[i].is_magnetic) {
                int owner_fire = (g->balls[i].owner == 1) ? input->p2_fire : fire_pressed;
                if (!owner_fire) continue;
                /* MAIN.ASM:3238-3245 clear magnetic_flag bit for this player */
                g->magnetic_flag &= ~((g->balls[i].owner == 1) ? PLAYER_TWO : PLAYER_ONE);
                g->balls[i].is_magnetic = 0;
                /* Velocity already retained from catch — ball resumes with (vx, vy). */
            }
        }
    }

    /* Keep magnetically attached balls pinned to their OWNING paddle X
     * (decal_x preserved). MAIN.ASM:3260-3268 move_ball @@player_one pins
     * pos_x = cursor_1.pos_x + decal_x, pos_y = cursor_1.pos_y - size_y. */
    if (g->magnetic_flag && g->state == STATE_PLAYING) {
        int pad_dx   = g->paddle.x   - g->paddle.prev_x;
        int pad2_dx  = g->paddle_2.x - g->paddle_2.prev_x;
        for (i = 0; i < g->ball_count; i++) {
            if (g->balls[i].active && g->balls[i].is_magnetic) {
                Paddle *own = (g->game_mode > 0 && g->balls[i].owner == 1) ? &g->paddle_2 : &g->paddle;
                int own_dx  = (own == &g->paddle_2) ? pad2_dx : pad_dx;
                g->balls[i].y = own->y - BALL_H;
                g->balls[i].x += own_dx;
                if (g->balls[i].x < own->x) g->balls[i].x = own->x;
                if (g->balls[i].x + BALL_W > own->x + own->w)
                    g->balls[i].x = own->x + own->w - BALL_W;
            }
        }
    }

    /* =======================================================================
     * STEP 2b: Paddle shooting — fire projectiles when gun is active.
     * MAIN.ASM:1083  call refresh_shoot / detect_shoot_player_1
     *
     * ASM logic (MAIN.ASM:5060-5120  detect_shoot_player_1):
     *   if laser_active == 0 → skip
     *   dec laser_active
     *   if fire pressed AND gun_cooldown == 0:
     *     spawn 2 projectiles (left + right gun positions)
     *     gun_cooldown = VAISSEAU_TIR_NBS_ANIM (6 frames)
     *   if gun_cooldown > 0: dec gun_cooldown
     * ======================================================================= */
    if (g->paddle.has_gun && g->state == STATE_PLAYING) {
        int is_big = !g->paddle.mini_laser;  /* per-paddle big/mini — MAIN.ASM:6509-6603 */

        /* Tick gun cooldown  MAIN.ASM:1881-1903 */
        if (g->paddle.gun_cooldown > 0)
            g->paddle.gun_cooldown--;

        /* Fire ONE projectile per event.
         * MAIN.ASM:1923-2027  Detect_Shoot: finds first inactive slot, fires
         * ONE projectile, returns. Left/right cannon sprites are muzzle flash
         * visuals only — not separate projectiles.
         * Mini-shoot alternates left/right cannon position each fire. */
        if (fire_pressed && g->paddle.gun_cooldown == 0
            && g->proj_count < MAX_PROJECTILES) {
            Projectile *p = &g->projectiles[g->proj_count];

            if (is_big) {
                /* Big shoot: centre projectile.
                 * Blaster.inc:246-247  vaisseau_decal_x_b / _y_b
                 * MAIN.ASM:1949-1954 */
                p->x = g->paddle.x + VAISSEAU_DECAL_X_B;
                p->y = g->paddle.y + VAISSEAU_DECAL_Y_B;
                p->vy = -VAISSEAU_TIR_BIG_SPEED;
                p->active = 1;
                p->is_big = 1;
            } else {
                /* Mini shoot: ONE projectile, alternating left/right cannon.
                 * MAIN.ASM:1984  xor player_1.player_left_or_right,On */
                g->paddle.gun_side ^= 1;  /* toggle L/R */
                if (g->paddle.gun_side) {
                    p->x = g->paddle.x + VAISSEAU_DECAL_X_R;
                    p->y = g->paddle.y + VAISSEAU_DECAL_Y_R;
                } else {
                    p->x = g->paddle.x + VAISSEAU_DECAL_X_L;
                    p->y = g->paddle.y + VAISSEAU_DECAL_Y_L;
                }
                p->vy = -VAISSEAU_TIR_SPEED;
                p->active = 1;
                p->is_big = 0;
            }
            p->owner = 0;  /* MAIN.ASM:1960,1977  mov [edx.sprite_player],O player_1 */
            g->proj_count++;
            g->paddle.gun_cooldown = is_big ? VAISSEAU_TIR_BIG_NBS_ANIM : VAISSEAU_TIR_NBS_ANIM;

            if (g->audio) audio_play(g->audio, SFX_SHOOT);
#if defined(BRICKBLASTER_MOBILE)
            platform_vibrate(12);
#endif
            /* F1-B: big shoot is ONE-SHOT per pickup.
             * MAIN.ASM:1947-1948 after firing big shoot:
             *   mov current_option,off
             *   mov player_1.player_current_option,Off
             * The big laser fires once then the paddle loses its gun.
             * Mini-shoot (MAIN.ASM:1970-2027) does NOT clear — it stays
             * timed for DELAI_OPTION frames. */
            if (is_big) {
                g->paddle.has_gun      = 0;
                g->paddle.laser_timer  = 0;
                g->paddle.mini_laser   = 0;
                /* Also clear the global current_option slot (set by Fix 3
                 * at collection time) since ASM:1947 clears it too. */
                if (g->current_option == POWERUP_SHOOT) {
                    g->current_option       = POWERUP_COUNT;
                    g->current_option_count = 0;
                }
            }
        }
    }

    /* MAIN.ASM:2068-2236  Detect_Shoot_player_2 — mirrors P1 using paddle_2 */
    if (g->game_mode > 0 && g->paddle_2.has_gun && g->state == STATE_PLAYING) {
        int is_big = !g->paddle_2.mini_laser;  /* per-paddle big/mini — MAIN.ASM:6509-6603 */

        if (g->paddle_2.gun_cooldown > 0)
            g->paddle_2.gun_cooldown--;

        if (input->p2_fire && g->paddle_2.gun_cooldown == 0
            && g->proj_count < MAX_PROJECTILES) {
            Projectile *p = &g->projectiles[g->proj_count];

            if (is_big) {
                p->x = g->paddle_2.x + VAISSEAU_DECAL_X_B;
                p->y = g->paddle_2.y + VAISSEAU_DECAL_Y_B;
                p->vy = -VAISSEAU_TIR_BIG_SPEED;
                p->active = 1;
                p->is_big = 1;
            } else {
                /* MAIN.ASM:2193  xor player_2.player_left_or_right,On */
                g->paddle_2.gun_side ^= 1;
                if (g->paddle_2.gun_side) {
                    p->x = g->paddle_2.x + VAISSEAU_DECAL_X_R;
                    p->y = g->paddle_2.y + VAISSEAU_DECAL_Y_R;
                } else {
                    p->x = g->paddle_2.x + VAISSEAU_DECAL_X_L;
                    p->y = g->paddle_2.y + VAISSEAU_DECAL_Y_L;
                }
                p->vy = -VAISSEAU_TIR_SPEED;
                p->active = 1;
                p->is_big = 0;
            }
            p->owner = 1;  /* MAIN.ASM:2169,2186  mov [edx.sprite_player],O player_2 */
            g->proj_count++;
            g->paddle_2.gun_cooldown = is_big ? VAISSEAU_TIR_BIG_NBS_ANIM : VAISSEAU_TIR_NBS_ANIM;

            if (g->audio) audio_play(g->audio, SFX_SHOOT);
            /* F1-B: big shoot one-shot for P2. MAIN.ASM:2155-2169 player_2
             * path mirrors MAIN.ASM:1947-1948 — big shoot clears
             * current_option + player_2.player_current_option after firing. */
            if (is_big) {
                g->paddle_2.has_gun      = 0;
                g->paddle_2.laser_timer  = 0;
                g->paddle_2.mini_laser   = 0;
                if (g->current_option == POWERUP_SHOOT) {
                    g->current_option       = POWERUP_COUNT;
                    g->current_option_count = 0;
                }
            }
        }
    }

    /* =======================================================================
     * STEP 2d: Monster spawn check.
     * MAIN.ASM:1077  call create_monster
     * FILE.ASM:1130-1132  monster_delai_easy/medium/hard = 600/500/300
     * ======================================================================= */
    if (g->state == STATE_PLAYING) {
        monster_create(g->monsters, &g->monster_spawn_counter, g->difficulty,
                       g->cfg_monster_delai);
    }

    /* =======================================================================
     * STEP 3: Ball physics — for each active ball
     * MAIN.ASM:1082  call refresh_ball
     * Order per ball: collision_walls → collision_paddle → collision_bricks → ball_move
     * ======================================================================= */
    for (i = 0; i < g->ball_count; i++) {
        if (!g->balls[i].active) continue;

        /* Wall bounce — ghost balls bounce normally (no wall wrap).
         * MAIN.ASM  detect_colision_wall:3497 */
        {
            int vx_before = g->balls[i].vx;
            int vy_before_w = g->balls[i].vy;
            collision_walls(&g->balls[i], 0);
#if defined(BRICKBLASTER_MOBILE)
            if (vx_before != g->balls[i].vx || vy_before_w != g->balls[i].vy)
                platform_vibrate(5);
#endif
        }

        /* Paddle collision.
         * MAIN.ASM  detect_colision_cursor:4152
         *
         * P0-ASM-5: in duel mode, cursor_1 only hits P1-owned balls and
         * cursor_2 only hits P2-owned balls.
         *   MAIN.ASM:4158-4161  cmp dual_flag,On; jne @@ok;
         *                        cmp [edx.sprite_player],O cursor_1; jne @@end
         *   MAIN.ASM:4281-4284  same gate for cursor_2/player_2.
         * In solo + coop, either paddle can hit any ball. */
        {
            int vy_before = g->balls[i].vy;
            int prev_vy = g->balls[i].vy;
            int mag_p1 = (g->magnetic_flag & PLAYER_ONE) ? 1 : 0;
            int mag_p2 = (g->magnetic_flag & PLAYER_TWO) ? 1 : 0;

            if (g->game_mode == 2) {
                /* Duel: gate by ball owner */
                if (g->balls[i].owner == 0) {
                    collision_paddle(&g->balls[i], &g->paddle, mag_p1);
                    if (prev_vy > 0 && g->balls[i].vy <= 0) {
                        g->balls[i].owner = 0;
                        if (g->audio) audio_play(g->audio, SFX_BOUNCE);
                    }
                } else {
                    collision_paddle(&g->balls[i], &g->paddle_2, mag_p2);
                    if (prev_vy > 0 && g->balls[i].vy <= 0) {
                        g->balls[i].owner = 1;
                        if (g->audio) audio_play(g->audio, SFX_BOUNCE);
                    }
                }
            } else {
                /* Solo + coop: both paddles can hit any ball */
                collision_paddle(&g->balls[i], &g->paddle, mag_p1);
                if (prev_vy > 0 && g->balls[i].vy <= 0) {
                    g->balls[i].owner = 0;
                    if (g->audio) audio_play(g->audio, SFX_BOUNCE);
                }
                if (g->game_mode == 1) {
                    int prev2 = g->balls[i].vy;
                    collision_paddle(&g->balls[i], &g->paddle_2, mag_p2);
                    if (prev2 > 0 && g->balls[i].vy <= 0) {
                        g->balls[i].owner = 1;
                        if (g->audio) audio_play(g->audio, SFX_BOUNCE);
                    }
                }
            }
#if defined(BRICKBLASTER_MOBILE)
            if (vy_before > 0 && g->balls[i].vy <= 0)
                platform_vibrate(15);
#endif
        }

        /* Ghost ball: explode on paddle contact.
         * MAIN.ASM:4208-4209  cmp sprite_ghost,On → @@end_ghost (break anim + delete).
         * Only destroy ghost ball when it's FALLING DOWN (vy > 0) and reaches paddle.
         * Ghost balls spawn from paddle moving upward (vy < 0) — must not kill them
         * immediately on spawn. They must fly up, hit bricks, then if they return
         * to paddle level while falling, they explode. */
        if (g->balls[i].is_ghost && g->balls[i].vy > 0
            && g->balls[i].y + BALL_H >= g->paddle.y
            && g->balls[i].x + BALL_W > g->paddle.x
            && g->balls[i].x < g->paddle.x + g->paddle.w) {
            g->balls[i].active = 0;
            continue;
        }

        /* Brick collision with trajectory interpolation.
         * MAIN.ASM  detect_colision_brique:3831 */
        bc = collision_bricks(&g->balls[i], g->bricks, BRICK_COUNT);

        /* Teleport timer tick: ball is invisible/frozen for DELAI_TELEPOD frames,
         * then reappears at the destination teleporter brick.
         * MAIN.ASM:4071  Blaster.inc:416  DELAI_TELEPOD = 5 */
        if (g->balls[i].telepod_timer > 0) {
            g->balls[i].telepod_timer--;
            continue;  /* skip all physics while teleporting */
        }

        if (bc.hit && bc.brick_index >= 0) {
            /* Ghost ball: explode on first brick contact.
             * MAIN.ASM:4183-4205  @@end_ghost: switch to break sprite, delete. */
            if (g->balls[i].is_ghost) {
                g->balls[i].active = 0;
                /* Still damage the brick if it was a normal brick */
                continue;
            }

            /* Teleporter brick: ball teleports to a random other teleporter.
             * MAIN.ASM:4071  play_sound iff_telepod */
            if (g->bricks[bc.brick_index].type == BRICK_TELEPORTER) {
                int tele_bricks[BRICK_COUNT];
                int tele_count = 0;
                for (j = 0; j < BRICK_COUNT; j++) {
                    if (j != bc.brick_index && g->bricks[j].active
                        && g->bricks[j].type == BRICK_TELEPORTER)
                        tele_bricks[tele_count++] = j;
                }
                if (tele_count > 0) {
                    int dest = tele_bricks[GetRandomValue(0, tele_count - 1)];
                    g->balls[i].x = g->bricks[dest].x;
                    g->balls[i].y = g->bricks[dest].y;
                    g->balls[i].telepod_timer = DELAI_TELEPOD;
                }
                if (g->audio) audio_play(g->audio, SFX_TELEPOD);
                /* Skip normal scoring/powerup spawn for teleporter hits */
                ball_move(&g->balls[i]);
                if (ball_lost(&g->balls[i])) {
                    if (g->audio) audio_play(g->audio, SFX_BALL_LOST);
#if defined(BRICKBLASTER_MOBILE)
                    platform_vibrate(150);
#endif
                    g->balls[i].active = 0;
                    if (g->balls[i].owner == 1) g->p2_ball_lost_pending = 1;
                    else                         g->p1_ball_lost_pending = 1;
                }
                continue;
            }

            /* Sound on first brick hit.
             * MAIN.ASM:4032  play_sound iff_normale   — normal brick
             * MAIN.ASM:4055  play_sound iff_incassable — indestructible */
            if (g->audio) {
                if (g->bricks[bc.brick_index].type == BRICK_INDESTRUCTIBLE) {
                    audio_play(g->audio, SFX_WALL_HIT);
                } else {
                    audio_play(g->audio, SFX_BRICK_HIT);
                }
            }
#if defined(BRICKBLASTER_MOBILE)
            platform_vibrate(10);
#endif
        }

        /* P0-ASM-4 / P0-ASM-1: iterate ALL bricks damaged this frame so each
         * gets its score, break-anim and powerup-spawn attempt independently.
         * Mirrors MAIN.ASM:3996 `dec B [esi+ebx]` applied per-corner-per-sub-step.
         * This also covers transparent bricks (bc.hit would be 0 but hit_count
         * may still contain them with no-bounce semantics). */
        {
            int hi;
            for (hi = 0; hi < bc.hit_count; hi++) {
                int hidx = bc.hit_indices[hi];
                int hdest = bc.destroyed_flag[hi];
                Brick *hb;
                if (hidx < 0 || hidx >= BRICK_COUNT) continue;
                hb = &g->bricks[hidx];

                /* Update raw level data + spawn break anim on destruction */
                if (hdest) {
                    g->current_level.bricks[hidx] = ABSENTE;
                    for (j = 0; j < MAX_BREAK_ANIMS; j++) {
                        if (!g->break_anims[j].active) {
                            g->break_anims[j].x = hb->x + BRICK_W / 2 - BALL_W / 2;
                            g->break_anims[j].y = hb->y + BRICK_H / 2 - BALL_H / 2;
                            g->break_anims[j].frame = 0;
                            g->break_anims[j].active = 1;
                            break;
                        }
                    }
                }

                /* Play brick-hit SFX for transparent bricks too (they don't
                 * appear in bc.brick_index which only tracks bounceable hits). */
                if (hb->type == BRICK_TRANSPARENT && g->audio && hdest) {
                    audio_play(g->audio, SFX_BRICK_HIT);
                }

                /* P0-ASM-1: score only on destroy, using inc_score helper for
                 * difficulty/night scaling + bonus-life trigger.
                 * MAIN.ASM:4024-4026 call inc_score (ecx=1) → 10/20/30 e/m/h. */
                if (!g->demo_active && hdest) {
                    inc_score(g, g->balls[i].owner, 1);
                }

                /* Powerup spawn — per-frame cooldown gate (only one spawn per
                 * Time_Between_Option window, P1-ASM-34). FILE.ASM:1139-1141 */
                if (hdest && g->powerup_count < MAX_POWERUPS_ACTIVE) {
                    int limit = powerup_spacing(g);
                    int spawn_ok = g->dev_test || (g->option_spawn_timer >= limit);
                    if (spawn_ok) {
                        /* F6-01: consult cfg-injected freq table (with
                         * fallback to hardcoded powerup_freq_table[]). */
                        PowerupType pt = g->dev_test ? dev_next_powerup(g)
                                                     : pick_powerup_cfg(g);
                        if (pt < POWERUP_COUNT) {
                            int bx = level_brick_x(hidx);
                            int by = level_brick_y(hidx);
                            powerup_init(&g->powerups[g->powerup_count], pt, bx, by);
                            /* F5 P1-ASM-35: tag with destroyer's owner (ball path). */
                            g->powerups[g->powerup_count].owner = g->balls[i].owner;
                            g->powerup_count++;
                            g->option_spawn_timer = 0;
                        }
                    }
                }
            }
        }
        (void)score_delta;  /* unused — kept for minimal-diff compat */

        /* Move ball forward by velocity.
         * MAIN.ASM:3250-3256  pos_x += sens_x; pos_y += sens_y */
        ball_move(&g->balls[i]);

        /* Check ball lost (exited bottom of screen).
         * MAIN.ASM:4581  play_sound iff_lost_ball — ball falls off screen */
        if (ball_lost(&g->balls[i])) {
            /* Spawn break animation at ball's last position.
             * MAIN.ASM:break_ball anim — same animation used for brick destruction. */
            for (j = 0; j < MAX_BREAK_ANIMS; j++) {
                if (!g->break_anims[j].active) {
                    g->break_anims[j].x = g->balls[i].x;
                    g->break_anims[j].y = g->balls[i].y;
                    g->break_anims[j].frame = 0;
                    g->break_anims[j].active = 1;
                    break;
                }
            }
            if (g->audio) audio_play(g->audio, SFX_BALL_LOST);
#if defined(BRICKBLASTER_MOBILE)
            platform_vibrate(150);
#endif
            g->balls[i].active = 0;
            /* Tag loss owner — consumed by handle_life_lost / per-player respawn
             * below. MAIN.ASM:4595 detect_game_over_player_1 per-player dispatch. */
            if (g->balls[i].owner == 1) g->p2_ball_lost_pending = 1;
            else                         g->p1_ball_lost_pending = 1;
        }
    }

    /* P0-ASM-4: transparent-brick damage + scoring + powerup spawn is now
     * handled by the multi-hit iteration loop inside the per-ball block above.
     * The previous deferred sync loop is no longer needed since
     * check_brick_at_point reports transparent hits in bc.hit_indices. */

    /* =======================================================================
     * STEP 3b: Monster update + ball-monster collision.
     * MAIN.ASM:1084  call refresh_monster
     * MAIN.ASM:3129  call detect_colision_monster
     * ======================================================================= */
    monster_update(g->monsters);

    /* Monster-brick collision — MAIN.ASM:3097 call detect_colision_wall
     * (described as "handled externally" in monster.h).
     * Monsters bounce off active bricks without destroying them.
     * Simple AABB: monsters move 1px/frame so penetration ≤ 1. */
    for (i = 0; i < NBS_MONSTER; i++) {
        Monster *m = &g->monsters[i];
        if (!m->active) continue;
        for (j = 0; j < BRICK_COUNT; j++) {
            if (!g->bricks[j].active) continue;
            if (g->bricks[j].type == BRICK_TRANSPARENT) continue;
            int col = j % BRICK_COLS;
            int row = j / BRICK_COLS;
            int bx = BRICK_ORIGIN_X + col * BRICK_W;
            int by = BRICK_ORIGIN_Y + row * BRICK_H;
            /* AABB overlap */
            int ox = (m->x + MONSTER_W) - bx;   /* right overlap */
            if (ox <= 0 || (bx + BRICK_W) - m->x <= 0) continue;
            int oy = (m->y + MONSTER_H) - by;
            if (oy <= 0 || (by + BRICK_H) - m->y <= 0) continue;
            int ox2 = (bx + BRICK_W) - m->x;
            int oy2 = (by + BRICK_H) - m->y;
            int pen_x = ox < ox2 ? ox : ox2;
            int pen_y = oy < oy2 ? oy : oy2;
            if (pen_x <= pen_y) {
                m->vx = -m->vx;
                m->x += (ox < ox2) ? -pen_x : pen_x;
            } else {
                m->vy = -m->vy;
                m->y += (oy < oy2) ? -pen_y : pen_y;
            }
            break; /* one bounce per frame */
        }
    }

    /* Ball-monster collision: MAIN.ASM:3174-3224  _detect_colision_monster
     * Uses ball centre vs monster bounding box.
     * On hit: ball bounces randomly, monster explodes, +3 pts. */
    for (i = 0; i < g->ball_count; i++) {
        if (!g->balls[i].active) continue;
        for (j = 0; j < NBS_MONSTER; j++) {
            if (!g->monsters[j].active) continue;
            /* Centre-point vs AABB check (MAIN.ASM:3180-3199) */
            int bcx = g->balls[i].x + BALL_W / 2;
            int bcy = g->balls[i].y + BALL_H / 2;
            if (bcx >= g->monsters[j].x && bcx < g->monsters[j].x + MONSTER_W &&
                bcy >= g->monsters[j].y && bcy < g->monsters[j].y + MONSTER_H) {
                /* Iter 2 fix #3: iron balls skip the monster bounce entirely.
                 * MAIN.ASM:2846 sets rebond=Off when iron active; :3199-3204
                 * skips change_direction if rebond is off.
                 *
                 * F1-F / F3-iter3-03: mirror the ASM 16-entry change_direction
                 * LUT exactly.
                 *   MAIN.ASM:3201-3204  mov eax,15; call get_random; mov
                 *     new_direction,eax; call change_direction
                 *   MAIN.ASM:3915-3930 change_direction branches:
                 *     new_direction == 0       → no-op (ret via @@end)
                 *     new_direction == 0001b=1 → inverse_sens_x
                 *     new_direction == 0010b=2 → inverse_sens_y
                 *     new_direction == 0101b=5 → inverse_sens_x
                 *     new_direction == 0110b=6 → inverse_sens_y
                 *     new_direction == 1001b=9 → inverse_sens_y
                 *     new_direction == 1010b=10→ inverse_sens_x
                 *     new_direction == 1101b=13→ inverse_sens_y
                 *     new_direction == 1110b=14→ inverse_sens_x
                 *     (all other — 3,4,7,8,11,12,15) → inverse_sens_xy
                 * Probability breakdown (of 16):
                 *   no-op:  1 (idx 0)
                 *   X-flip: 4 (idx 1,5,10,14)
                 *   Y-flip: 4 (idx 2,6,9,13)
                 *   XY:     7 (idx 3,4,7,8,11,12,15) */
                if (!g->balls[i].is_iron) {
                    int r = GetRandomValue(0, 15);  /* MAIN.ASM:3201 mov eax,15 */
                    switch (r) {
                        case 0:
                            /* MAIN.ASM:3908 cmp new_direction,Off je @@end */
                            break;
                        case 1: case 5: case 10: case 14:
                            /* MAIN.ASM:3915-3930 X-flip cases */
                            ball_bounce_x(&g->balls[i]);
                            break;
                        case 2: case 6: case 9: case 13:
                            /* MAIN.ASM:3917-3928 Y-flip cases */
                            ball_bounce_y(&g->balls[i]);
                            break;
                        default:
                            /* MAIN.ASM:3932 jmp inverse_sens_xy — call both */
                            ball_bounce_x(&g->balls[i]);
                            ball_bounce_y(&g->balls[i]);
                            break;
                    }
                }
                /* P0-ASM-2: ecx=3 via inc_score helper → 30/40/50 e/m/h.
                 * MAIN.ASM:3210-3214 mov ebp,[edx.sprite_player]; mov ecx,3;
                 * call inc_score — attribute to ball owner. */
                if (!g->demo_active) {
                    inc_score(g, g->balls[i].owner, 3);
                }
                /* Destroy monster: MAIN.ASM:3219  call del_monster.
                 * Iter 2 fix #6: ASM uses iff_del_monster → MONSTOFF.wav, not BOOM. */
                monster_kill(&g->monsters[j]);
                if (g->audio) audio_play(g->audio, SFX_POWERUP_LOST);
#if defined(BRICKBLASTER_MOBILE)
                platform_vibrate(30);
#endif
            }
        }
    }

    /* Projectile-monster collision: projectiles also destroy monsters */
    for (i = 0; i < g->proj_count; i++) {
        if (!g->projectiles[i].active) continue;
        int pw = g->projectiles[i].is_big ? VAISSEAU_TIR_BIG_SIZE_X : VAISSEAU_TIR_SIZE_X;
        int ph = g->projectiles[i].is_big ? VAISSEAU_TIR_BIG_SIZE_Y : VAISSEAU_TIR_SIZE_Y;
        int pcx = g->projectiles[i].x + pw / 2;
        int pcy = g->projectiles[i].y;
        for (j = 0; j < NBS_MONSTER; j++) {
            if (!g->monsters[j].active) continue;
            if (pcx >= g->monsters[j].x && pcx < g->monsters[j].x + MONSTER_W &&
                pcy >= g->monsters[j].y && pcy < g->monsters[j].y + MONSTER_H) {
                monster_kill(&g->monsters[j]);
                g->projectiles[i].active = 0;
                /* Iter 2 fix #6: iff_del_monster → MONSTOFF.wav, not BOOM. */
                if (g->audio) audio_play(g->audio, SFX_POWERUP_LOST);
#if defined(BRICKBLASTER_MOBILE)
                platform_vibrate(30);
#endif
                /* P0-ASM-2: monster kill via projectile uses same inc_score
                 * helper for difficulty/night scaling. MAIN.ASM:3210. */
                if (!g->demo_active) {
                    inc_score(g, g->projectiles[i].owner, 3);
                }
                break;
            }
        }
    }

    /* Periodic auto-speedup.
     * MAIN.ASM:3387-3409  detect_inc_speed (per-ball speed counter) */
    tick_speed_counter(g);

    /* =======================================================================
     * STEP 4: Projectile movement
     * MAIN.ASM:1083  call refresh_shoot
     * Blaster.inc:242  vaisseau_tir_speed = 1 (1 px/frame upward)
     * ======================================================================= */
    for (i = 0; i < g->proj_count; i++) {
        if (!g->projectiles[i].active) continue;
        g->projectiles[i].y -= 4;   /* Blaster.inc:242 vaisseau_tir_speed=1 but at 60fps
                                     * 1px/frame is slower than the ball (vy≈3).
                                     * 4px/frame matches original feel. */
        if (g->projectiles[i].y + VAISSEAU_TIR_SIZE_Y < 0) {
            g->projectiles[i].active = 0;
        } else if (g->projectiles[i].active) {
            projectile_hit_brick(g, &g->projectiles[i]);
        }
    }

    /* Compact projectile array */
    j = 0;
    for (i = 0; i < g->proj_count; i++) {
        if (g->projectiles[i].active) g->projectiles[j++] = g->projectiles[i];
    }
    g->proj_count = j;

    /* =======================================================================
     * STEP 5: Powerup fall + collection
     * MAIN.ASM:1085  call refresh_options
     * MAIN.ASM:5539  sprite_sens_y = +2 (fall at 2 px/frame)
     * MAIN.ASM:5579-5597  collect / expire logic
     * ======================================================================= */
    for (i = 0; i < g->powerup_count; i++) {
        if (!g->powerups[i].active) continue;
        /* P1-ASM-31: detect off-screen fall vs. timer expiry so we can
         * play SFX_POWERUP_LOST on the fall case only (MAIN.ASM:5592). */
        int off_screen_before = (g->powerups[i].y + OPTION_H + g->powerups[i].vy >= SCREEN_H);
        if (powerup_update(&g->powerups[i])) {
            /* powerup_update returns 1 when off-screen or timer expired */
            if (off_screen_before && g->audio) audio_play(g->audio, SFX_POWERUP_LOST);
            g->powerups[i].active = 0;
            continue;
        }
        /* Check collection.
         * MAIN.ASM:5579-5624  detect_prise_option
         * MAIN.ASM:347  play_sound iff_option — generic powerup collect sound
         * Demo mode: play sound but do NOT apply powerup effects.
         * DEMO_MODE_FIX.md: "No powerup effects applied when demo_flag=On". */
        /* MP: check both paddles; the one that overlaps gets the powerup. */
        int collected_by = -1;   /* 0 = P1, 1 = P2 */
        if (powerup_collected(&g->powerups[i], &g->paddle)) collected_by = 0;
        else if (g->game_mode > 0 &&
                 powerup_collected(&g->powerups[i], &g->paddle_2)) collected_by = 1;

        if (collected_by >= 0) {
            if (g->audio) audio_play(g->audio, SFX_POWERUP_COLLECT);
#if defined(BRICKBLASTER_MOBILE)
            platform_vibrate(25);
#endif
            if (!g->demo_active) {
                apply_powerup(g, g->powerups[i].type, collected_by);
                /* MAIN.ASM:5710-5716 detect_prise_option: mov ecx,2; call inc_score
                 * → +20 base, +difficulty bonus, x2 in night mode. */
                inc_score(g, collected_by, 2);
            }
            g->powerups[i].active = 0;
        }
    }

    /* Compact powerup array */
    j = 0;
    for (i = 0; i < g->powerup_count; i++) {
        if (g->powerups[i].active) g->powerups[j++] = g->powerups[i];
    }
    g->powerup_count = j;

    /* =======================================================================
     * STEP 7: Check level complete
     * MAIN.ASM:1094-1095  call detect_next_level; jnc next_level
     * ======================================================================= */
    if (game_level_complete(g)) {
        g->state = STATE_NEW_PLAY;
        /* P1-ASM-31: MAIN.ASM:4141 play_sound iff_next_level on level advance */
        if (g->audio) audio_play(g->audio, SFX_LEVEL_COMPLETE);
#if defined(BRICKBLASTER_MOBILE)
        platform_vibrate(200);
#endif
        return;
    }

    /* =======================================================================
     * STEP 8: Check all balls lost → lose life (or demo respawn)
     * MAIN.ASM:1088-1091  detect_game_over_player_1 / detect_game_over_player_2
     * Demo mode: MAIN.ASM never reaches detect_game_over when demo_flag=On
     *   because the frame loop returns early via demo_timer exit.
     *   We implement as: respawn ball at fixed demo velocity.
     * ======================================================================= */
    if (g->state == STATE_PLAYING) {
        int active_count = 0;
        any_alive = 0;
        for (i = 0; i < g->ball_count; i++) {
            if (g->balls[i].active) { any_alive = 1; active_count++; }
        }
        /* P1-ASM-9: MAIN.ASM:4623-4629 detect_game_over_player_1
         *   test_game_over fires when nbs_ball_in_play == 0, OR when
         *   nbs_ball_in_play < 2 AND coop_flag=On. Coop pool loses a life
         *   whenever in-play drops below 2 (including to 1). */
        int trigger_life_lost =
            (!any_alive && g->ball_count > 0)
            || (g->game_mode == 1 && g->ball_count > 0 && active_count < 2
                && (g->p1_ball_lost_pending || g->p2_ball_lost_pending));
        if (trigger_life_lost) {
            if (g->demo_active) {
                /* Demo: respawn immediately — no life lost.
                 * MAIN.ASM:2761-2771  @@demo ball init (fixed velocity). */
                demo_handle_ball_lost(g);
            } else {
                /* Normal play: MAIN.ASM:4595-4712  test_game_over */
                handle_life_lost(g);
            }
            return;
        }
        /* Compact ball array */
        j = 0;
        for (i = 0; i < g->ball_count; i++) {
            if (g->balls[i].active) g->balls[j++] = g->balls[i];
        }
        g->ball_count = j;
    }

    /* =======================================================================
     * STEP 9: Demo timer
     * MAIN.ASM:1159-1168:
     *   cmp computer_flag,On je @@cont
     *   cmp demo_flag,On jne @@cont
     *   cmp test_flag,On je @@cont
     *   call read_click; jnz @@exit
     *   dec demo_counter
     *   cmp demo_counter,off; jne @@cont
     *   ret          ; → returns to intro, triggers new demo
     * ======================================================================= */
    if (g->demo_active) {
        g->demo_timer--;
        if (g->demo_timer <= 0) {
            /* Demo timed out → return to intro (signal via STATE_MENU) */
            g->demo_timer  = DELAI_DEMO;   /* Blaster.inc:420 */
            g->demo_active = 0;
            g->state       = STATE_MENU;
        }
    }

    /* Tick break animations.
     * DRAW.ASM:Refresh_Sprites advances break_ball frame each BREAK_SPEED frames.
     * Blaster.inc:6  break_nbs_anim=5, Blaster.inc:7  break_speed=1 */
    for (i = 0; i < MAX_BREAK_ANIMS; i++) {
        if (!g->break_anims[i].active) continue;
        g->break_anims[i].frame++;
        if (g->break_anims[i].frame >= BREAK_NBS_ANIM)
            g->break_anims[i].active = 0;
    }
}
