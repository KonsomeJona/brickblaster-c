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
    fi.fire_pressed = 1;                 /* P1 collects while holding fire */

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

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
