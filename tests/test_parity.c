/* test_parity.c — headless instrumental tests for the ASM-parity behaviours.
 *
 * The game logic is raylib-free: game.c, ball.c, brick.c, collision.c,
 * powerup.c, monster.c, paddle.c and asm_random.c call no raylib API at all,
 * and audio is skipped entirely when Game.audio is NULL. So the whole rule set
 * can be driven from a plain main() with synthetic FrameInput frames, with no
 * window, no assets and no timing.
 *
 * These cover the paths that are awkward to reach by hand — duel ownership,
 * the carried-option slot, night mode, the board reset on a lost life — and
 * that regressed at least once during the August 2026 parity work.
 *
 * Run from the repository root (level files are loaded via a relative path).
 */

#include "../src/game.h"
#include "../src/ball.h"
#include "../src/powerup.h"
#include "../src/monster.h"
#include "../src/collision.h"
#include "../src/brick.h"
#include "../src/level.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
static const char *g_case = "";

static void check(int cond, const char *what) {
    if (cond) { g_pass++; }
    else { g_fail++; printf("  FAIL  [%s] %s\n", g_case, what); }
}
#define CHECK(c) check((c), #c)

static void begin(const char *name) { g_case = name; printf("- %s\n", name); }

/* A frame with nothing pressed. */
static FrameInput idle_input(void) {
    FrameInput fi;
    memset(&fi, 0, sizeof(fi));
    fi.button_speed_mul = 1.0f;
    fi.tilt_speed_mul   = 1.0f;
    return fi;
}

/* Bring a game up in PLAYING with one ball in flight. */
static void boot(Game *g, int mode) {
    game_init(g, NULL, NULL, DIFFICULTY_EASY, mode);
    game_load_level(g, 1);
    game_spawn_ball(g);
    g->state = STATE_PLAYING;
    g->balls[0].is_magnetic = 0;
    g->balls[0].vx = 2;
    g->balls[0].vy = -3;
}

/* Collection happens in STEP 5 of game_update, while the paddle state is
 * derived in STEP 1 — so a picked-up effect only shows on the FOLLOWING frame.
 * That is the ASM order too: detect_large_cursor / detect_shoot run at
 * MAIN.ASM:1071-1076, refresh_options (which contains detect_prise_option) at
 * MAIN.ASM:1085. This helper therefore ticks twice. */
static void collect_and_settle(Game *g, const FrameInput *fi) {
    game_update(g, fi);   /* pick it up */
    game_update(g, fi);   /* derive the paddle from the new slot */
}

/* Leave exactly one ball in play, owned by `owner`. */
static void only_one_ball(Game *g, int owner) {
    int i;
    for (i = 1; i < g->ball_count; i++) g->balls[i].active = 0;
    g->balls[0].active = 1;
    g->balls[0].owner  = owner;
}

/* Drop a powerup exactly onto a paddle so the next update collects it. */
static void put_powerup_on(Game *g, PowerupType t, Paddle *pad, int owner) {
    Powerup *p = &g->powerups[g->powerup_count];
    powerup_init(p, t, 0, 0);
    /* powerup_collected wants p->y inside [PADDLE_ROW_Y, +OPTION_H] and the
     * sprite ENTIRELY within the paddle width (MAIN.ASM:5579-5585, 5617-5624). */
    p->x     = pad->x + pad->w / 2 - OPTION_W / 2;
    p->y     = PADDLE_ROW_Y;
    p->owner = owner;
    p->vy    = 0;
    g->powerup_count++;
}

/* ------------------------------------------------------------------ */

static void t_ball_lost_line(void) {
    begin("ball_lost: paddle centre line, downward only (MAIN.ASM:4526-4541)");
    Ball b; memset(&b, 0, sizeof(b));
    b.vy = 4;
    b.y = 423; CHECK(ball_lost(&b) == 0);
    b.y = 424; CHECK(ball_lost(&b) == 1);
    b.y = 470; CHECK(ball_lost(&b) == 1);
    b.vy = -4;                       /* travelling up: js @@end */
    b.y = 470; CHECK(ball_lost(&b) == 0);
}

static void t_option_slot_is_exclusive(void) {
    begin("one carried option per player (MAIN.ASM:5689, 6703/6710/6717)");
    Game g; boot(&g, 0);
    FrameInput fi = idle_input();

    put_powerup_on(&g, POWERUP_SHOOT, &g.paddle, 0);
    collect_and_settle(&g, &fi);
    CHECK(g.player_option[0] == POWERUP_SHOOT);
    CHECK(g.paddle.has_gun == 1);

    put_powerup_on(&g, POWERUP_LARGE_SHIP, &g.paddle, 0);
    collect_and_settle(&g, &fi);
    CHECK(g.player_option[0] == POWERUP_LARGE_SHIP);
    CHECK(g.paddle.size == PADDLE_SIZE_LARGE);
    CHECK(g.paddle.has_gun == 0);        /* the gun is GONE, not stacked */
}

static void t_instant_option_wipes_both(void) {
    begin("an instant powerup clears BOTH players (MAIN.ASM:6352-6354)");
    Game g; boot(&g, 2);
    FrameInput fi = idle_input();

    put_powerup_on(&g, POWERUP_SHOOT, &g.paddle, 0);
    game_update(&g, &fi);
    put_powerup_on(&g, POWERUP_REVERSE, &g.paddle_2, 1);
    game_update(&g, &fi);
    CHECK(g.player_option[0] == POWERUP_SHOOT);
    CHECK(g.player_option[1] == POWERUP_REVERSE);

    put_powerup_on(&g, POWERUP_BALL_3, &g.paddle, 0);   /* instant */
    collect_and_settle(&g, &fi);
    CHECK(g.player_option[0] == POWERUP_COUNT);
    CHECK(g.player_option[1] == POWERUP_COUNT);
    CHECK(g.paddle.has_gun == 0);
    CHECK(g.paddle_2.reversed == 0);
}

static void t_night_survives_its_own_pickup(void) {
    begin("NIGHT stays active after pickup, and doubles the score");
    Game g; boot(&g, 0);
    FrameInput fi = idle_input();

    put_powerup_on(&g, POWERUP_NIGHT, &g.paddle, 0);
    game_update(&g, &fi);
    CHECK(g.night_active == 1);          /* regressed once: default: wiped it */

    int before = g.score;
    put_powerup_on(&g, POWERUP_BONUS, &g.paddle, 0);
    game_update(&g, &fi);
    CHECK(g.score > before);
}

static void t_duel_powerup_belongs_to_its_owner(void) {
    begin("duel: only the tagged player collects (MAIN.ASM:5626-5630)");
    Game g; boot(&g, 2);
    FrameInput fi = idle_input();

    /* Tagged for P2 but dropped on P1's paddle: P1 must not get it. */
    put_powerup_on(&g, POWERUP_LARGE_SHIP, &g.paddle, 1);
    game_update(&g, &fi);
    CHECK(g.player_option[0] == POWERUP_COUNT);
    CHECK(g.paddle.size == PADDLE_SIZE_NORMAL);
}

static void t_duel_hold_fire_gives_it_away(void) {
    begin("duel: holding fire hands the option to the opponent (MAIN.ASM:5655-5670)");
    Game g; boot(&g, 2);
    FrameInput fi = idle_input();
    /* HELD, not pressed: read_click_player_1 is INT 33h AX=3, a button-state
     * read (MOUSE.ASM:509-513). Asserting on the press edge would let an
     * edge-triggered implementation pass, which is exactly what it used to do. */
    fi.fire_held = 1;                    /* P1 collects while holding fire */

    put_powerup_on(&g, POWERUP_LARGE_SHIP, &g.paddle, 0);
    game_update(&g, &fi);
    CHECK(g.player_option[1] == POWERUP_LARGE_SHIP);   /* went to P2 */
    CHECK(g.player_option[0] == POWERUP_COUNT);
}

static void t_life_lost_clears_the_board(void) {
    begin("a lost life resets monsters, shots and the magnet (MAIN.ASM:1036-1045)");
    Game g; boot(&g, 0);
    FrameInput fi = idle_input();

    only_one_ball(&g, 0);
    monster_add_now(g.monsters, &g.monster_spawn_counter, DIFFICULTY_EASY, NULL);
    g.projectiles[0].active = 1; g.proj_count = 1;
    g.magnetic_flag = PLAYER_ONE;
    g.balls[0].is_magnetic = 1;
    CHECK(g.monsters[0].active == 1);

    g.balls[0].is_magnetic = 0;
    g.balls[0].x  = PLAY_X1 + 4;         /* far from the paddle: a real miss */
    g.paddle.x    = PLAY_X2 - g.paddle.w - 4;
    g.balls[0].y  = 460;                 /* past the death line */
    g.balls[0].vy = 6;
    game_update(&g, &fi);

    CHECK(g.monsters[0].active == 0);
    CHECK(g.proj_count == 0);
    CHECK(g.magnetic_flag == 0);
    CHECK(g.monster_spawn_counter == 0);
}

static void t_one_fire_launches_both_balls(void) {
    begin("one click serves BOTH balls (MAIN.ASM:5286 single read_click)");
    Game g;
    game_init(&g, NULL, NULL, DIFFICULTY_EASY, 1);   /* coop */
    game_load_level(&g, 1);
    game_spawn_ball(&g);
    g.state = STATE_READY_TO_PLAY;

    FrameInput fi = idle_input();
    fi.fire_pressed = 1;                 /* P1 only */
    fi.p2_fire      = 0;
    game_update(&g, &fi);

    CHECK(g.state == STATE_PLAYING);
    int still_stuck = 0, i;
    for (i = 0; i < g.ball_count; i++)
        if (g.balls[i].active && g.balls[i].is_magnetic) still_stuck++;
    CHECK(still_stuck == 0);
}

static void t_duel_ends_on_first_player_out(void) {
    begin("duel ends at the FIRST player out (MAIN.ASM:4674-4678)");
    Game g; boot(&g, 2);
    FrameInput fi = idle_input();

    only_one_ball(&g, 0);                /* duel spawns one ball per paddle */
    g.lives   = 0;
    g.lives_2 = 2;
    g.balls[0].owner = 0;
    g.balls[0].is_magnetic = 0;
    g.balls[0].x  = PLAY_X1 + 4;
    g.paddle.x    = PLAY_X2 - g.paddle.w - 4;
    g.paddle_2.x  = PLAY_X2 - g.paddle_2.w - 4;
    g.balls[0].y  = 460;
    g.balls[0].vy = 6;
    game_update(&g, &fi);

    CHECK(g.lives < 0);
    CHECK(g.lives_2 >= 0);               /* a survivor exists: never a draw */
    CHECK(g.state == STATE_GAME_OVER);
}

static void t_coop_pools_the_score(void) {
    begin("coop routes every point to player_1 (FONTE.ASM:88-91)");
    Game g; boot(&g, 1);
    FrameInput fi = idle_input();

    put_powerup_on(&g, POWERUP_BONUS, &g.paddle_2, 1);   /* P2 collects */
    game_update(&g, &fi);
    CHECK(g.score > 0);
    CHECK(g.score_2 == 0);               /* no separate counter outside duel */
}

static void t_five_monster_slots(void) {
    begin("five monster slots, not four (MAIN.ASM:7096-7100)");
    Game g; boot(&g, 0);
    int i, spawned = 0;
    for (i = 0; i < 8; i++)
        spawned += monster_add_now(g.monsters, &g.monster_spawn_counter,
                                   DIFFICULTY_EASY, NULL);
    CHECK(spawned == 5);
    CHECK(NBS_MONSTER == 5);
}

static void t_wall_bounce_keeps_position(void) {
    begin("a wall bounce never rewrites the position (MAIN.ASM:3497-3535)");
    Ball b; memset(&b, 0, sizeof(b));
    b.x = PLAY_X1 + 3; b.y = 200; b.vx = -4; b.vy = 2;
    int x_before = b.x;
    collision_walls(&b, 0);
    CHECK(b.vx == 4);                    /* bounced */
    CHECK(b.x == x_before);              /* but not snapped onto the wall */
}


static void t_duel_counts_balls_per_player(void) {
    begin("duel: a life is lost as soon as YOUR last ball drops (MAIN.ASM:4607-4610)");
    Game g; boot(&g, 2);
    FrameInput fi = idle_input();

    /* P1 keeps a ball safely in flight; P2's only ball falls past the line. */
    g.ball_count = 2;
    g.balls[0].active = 1; g.balls[0].owner = 0;
    g.balls[0].x = PLAY_X1 + 60; g.balls[0].y = 200;
    g.balls[0].vx = 2; g.balls[0].vy = -3; g.balls[0].is_magnetic = 0;
    g.balls[1].active = 1; g.balls[1].owner = 1;
    g.balls[1].x = PLAY_X1 + 4;  g.balls[1].y = 460;
    g.balls[1].vx = 0; g.balls[1].vy = 6;  g.balls[1].is_magnetic = 0;
    g.paddle_2.x = PLAY_X2 - g.paddle_2.w - 4;   /* nowhere near it: a real miss */
    int lives_1_before = g.lives, lives_2_before = g.lives_2;

    game_update(&g, &fi);

    /* nbs_ball_in_play is counted per player in duel, so P2 pays immediately
     * instead of waiting for P1 to die too. */
    CHECK(g.lives_2 == lives_2_before - 1);
    CHECK(g.lives   == lives_1_before);
}

static void t_bonus_life_steps_by_the_cfg_value(void) {
    begin("bonus life re-reads bonus_extra_life on every award (MAIN.ASM:6460-6461)");
    Game g;
    game_init(&g, NULL, NULL, DIFFICULTY_EASY, 0);
    game_load_level(&g, 1);
    int speed_start[3] = {2, 3, 4};
    game_set_cfg_scalars(&g, 5, 1500, speed_start, 5000);   /* Extra_Life 5000 */
    CHECK(g.bonus_life_threshold == 5000);

    game_spawn_ball(&g);
    g.state = STATE_PLAYING;
    g.balls[0].is_magnetic = 0;

    /* Collecting an option is worth +20 (MAIN.ASM:5710-5716), enough to cross
     * the threshold. It must then move to 10 000, not to 15 000 — which is
     * what stepping by the compiled BONUS_EXTRA_LIFE produced. */
    FrameInput fi = idle_input();
    g.score = 4995;
    put_powerup_on(&g, POWERUP_SHOOT, &g.paddle, 0);
    game_update(&g, &fi);
    CHECK(g.score >= 5000);
    CHECK(g.bonus_life_threshold == 10000);
}

static void t_life_lost_serves_both_players_again(void) {
    begin("coop: both balls come back after a lost life (MAIN.ASM:1036-1045 start_game)");
    Game g; boot(&g, 1);
    FrameInput fi = idle_input();

    only_one_ball(&g, 0);
    g.balls[0].is_magnetic = 0;
    g.balls[0].x  = PLAY_X1 + 4;
    g.paddle.x    = PLAY_X2 - g.paddle.w - 4;
    g.balls[0].y  = 460;
    g.balls[0].vy = 6;
    game_update(&g, &fi);
    CHECK(g.state == STATE_READY_TO_PLAY_AGAIN);

    /* Run the paddle explosion out. The deferred serve used to be pre-empted
     * by a defensive one-ball respawn, so P2 never got a ball back. */
    int i;
    for (i = 0; i < PADDLE_EXPLO_TICKS + 2; i++) game_update(&g, &fi);
    CHECK(g.ball_count == 2);
    CHECK(g.balls[0].owner == 0);
    CHECK(g.balls[1].owner == 1);
}

/* ------------------------------------------------------------------------
 * Port extensions: the atoll world, edited copies, the editor's test run.
 * ------------------------------------------------------------------------ */

/* Flood fill from the open bottom of the board through every cell that is
 * not indestructible or a teleporter (breakables open up as they break):
 * each N/M/T brick must be reached. */
static int breakables_reachable(const unsigned char *lv) {
    unsigned char seen[BRICK_COUNT] = { 0 };
    int stack[BRICK_COUNT], top = 0, i;

    for (i = 25 * BRICK_COLS; i < 26 * BRICK_COLS; i++) {
        int t = lv[i] & 0x38;
        if (lv[i] && (t == 0x08 || t == 0x18)) continue;
        seen[i] = 1; stack[top++] = i;
    }
    while (top) {
        int c = stack[--top], r = c / BRICK_COLS, k = c % BRICK_COLS, d;
        const int nb[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
        for (d = 0; d < 4; d++) {
            int rr = r + nb[d][0], kk = k + nb[d][1], n, t;
            if (rr < 0 || rr > 25 || kk < 0 || kk >= BRICK_COLS) continue;
            n = rr * BRICK_COLS + kk;
            t = lv[n] & 0x38;
            if (seen[n] || (lv[n] && (t == 0x08 || t == 0x18))) continue;
            seen[n] = 1; stack[top++] = n;
        }
    }
    for (i = 0; i < BRICK_COUNT; i++) {
        int t = lv[i] & 0x38;
        if (lv[i] && t != 0x08 && t != 0x18 && !seen[i]) return 0;
    }
    return 1;
}

static void t_atoll_world_is_sound(void) {
    begin("atoll: 8 levels, EDITOR.ASM brush bytes only, every breakable reachable");
    static unsigned char buf[LEVEL_FILE_SIZE];
    int L, i, c, b;

    CHECK(level_read_world(WORLD_ATOLL, buf, 0) == 0);
    CHECK(level_count_buffer(buf) == 8);
    for (L = 0; L < 8; L++) {
        const unsigned char *lv = buf + L * BRICK_COUNT;
        int ok_bytes = 1, ok_rows = 1, breakables = 0;
        for (i = 0; i < BRICK_COUNT; i++) {
            int legal = (lv[i] == 0);
            for (b = 0; b < 5; b++)
                for (c = 0; c < 4; c++)
                    if (lv[i] == level_brush_code(b, c)) legal = 1;
            if (!legal) ok_bytes = 0;
            if (lv[i] && (i < 2 * BRICK_COLS || i >= 26 * BRICK_COLS)) ok_rows = 0;
            if (lv[i] && (lv[i] & 0x38) != 0x08 && (lv[i] & 0x38) != 0x18) breakables++;
        }
        CHECK(lv[0] != 0xFF);
        CHECK(ok_bytes);
        CHECK(ok_rows);
        CHECK(breakables >= 20);
        CHECK(breakables_reachable(lv));
    }
    /* Past the last level: 0xFF slots, as in the 1999 files. */
    CHECK(buf[8 * BRICK_COUNT] == 0xFF && buf[LEVEL_FILE_SIZE - 1] == 0xFF);
}

static void t_edited_copy_replaces_world(void) {
    begin("editor: the edited copy of a world is what the game plays; removing it restores the shipped one");
    static unsigned char buf[LEVEL_FILE_SIZE];
    int shipped;
    Game g;

    level_set_user_dir("bb_test_user/");
    CHECK(level_remove_user_world(0) == 0);
    shipped = level_count(0);
    CHECK(shipped == 40);
    CHECK(level_read_world(0, buf, 1) == 0);

    buf[2 * BRICK_COLS] = level_brush_code(1, 2);                 /* level 1 edited */
    memset(buf + shipped * BRICK_COUNT, 0, BRICK_COUNT);          /* level 41 appended */
    buf[shipped * BRICK_COUNT + 3 * BRICK_COLS + 6] = level_brush_code(0, 3);
    CHECK(level_write_user_world(0, buf) == 0);
    CHECK(level_count(0) == shipped + 1);

    game_init(&g, NULL, NULL, DIFFICULTY_EASY, 0);
    g.world = 0;
    game_load_level(&g, 1);
    CHECK(g.current_level.bricks[2 * BRICK_COLS] == level_brush_code(1, 2));
    game_load_level(&g, shipped + 1);
    CHECK(g.bricks[3 * BRICK_COLS + 6].active);
    CHECK(!game_level_complete(&g));

    CHECK(level_remove_user_world(0) == 0);
    CHECK(level_count(0) == shipped);
    level_set_user_dir("data/");
    remove("bb_test_user");
}

static void t_trim_drops_emptied_last_levels(void) {
    begin("editor: an emptied last level turns back into a 0xFF slot");
    static unsigned char buf[LEVEL_FILE_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    memset(buf, 0, 3 * BRICK_COUNT);
    buf[0] = level_brush_code(0, 0);                             /* level 1 only */
    CHECK(level_count_buffer(buf) == 3);
    level_trim_world(buf);
    CHECK(level_count_buffer(buf) == 1);
    memset(buf, 0, BRICK_COUNT);                                 /* all empty */
    level_trim_world(buf);
    CHECK(level_count_buffer(buf) == 1);                         /* one level stays */
}

static void t_editor_test_plays_memory_grid(void) {
    begin("editor test run: game_load_level_bricks plays the grid in memory");
    unsigned char grid[BRICK_COUNT];
    Game g;
    int n = 2 * BRICK_COLS + 6;

    memset(grid, 0, sizeof(grid));
    grid[n] = level_brush_code(0, 3);
    grid[n + BRICK_COLS] = level_brush_code(2, 1);
    game_init(&g, NULL, NULL, DIFFICULTY_MEDIUM, 0);
    g.world = WORLD_ATOLL;
    game_load_level_bricks(&g, 5, grid);
    CHECK(g.level_num == 5);
    CHECK(g.current_level.brick_count == 2);
    CHECK(g.bricks[n].active && g.bricks[n].hp == 1);
    CHECK(g.bricks[n + BRICK_COLS].type == BRICK_INDESTRUCTIBLE);
    CHECK(!game_level_complete(&g));
    brick_hit(&g.bricks[n], 1);
    CHECK(game_level_complete(&g));   /* the indestructible one does not count */
}

int main(void) {
    printf("BrickBlaster — instrumental parity tests\n\n");

    t_ball_lost_line();
    t_wall_bounce_keeps_position();
    t_five_monster_slots();
    t_option_slot_is_exclusive();
    t_instant_option_wipes_both();
    t_night_survives_its_own_pickup();
    t_duel_powerup_belongs_to_its_owner();
    t_duel_hold_fire_gives_it_away();
    t_life_lost_clears_the_board();
    t_one_fire_launches_both_balls();
    t_duel_ends_on_first_player_out();
    t_coop_pools_the_score();
    t_duel_counts_balls_per_player();
    t_bonus_life_steps_by_the_cfg_value();
    t_life_lost_serves_both_players_again();
    t_atoll_world_is_sound();
    t_edited_copy_replaces_world();
    t_trim_drops_emptied_last_levels();
    t_editor_test_plays_memory_grid();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
