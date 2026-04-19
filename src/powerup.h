#pragma once
/* powerup.h — BrickBlaster Powerup (Option) system
 *
 * Ground truth: MAIN.ASM Refresh_Options, random_options, detect_prise_option,
 * and the struc_options table at MAIN.ASM:6948-6987.
 *
 * Key ASM facts:
 *
 *   Fall speed (sprite_sens_y):
 *     MAIN.ASM:5539  mov [edx.sprite_sens_y],+2
 *     Powerups fall at exactly 2 pixels per frame.
 *
 *   Spawn position:
 *     MAIN.ASM:5537  mov [edx.sprite_pos_x],ebx   (brick centre X)
 *     MAIN.ASM:5538  mov [edx.sprite_pos_y],edi   (brick top Y)
 *     Caller passes ebx = brick_x (top-left), edi = brick_y.
 *     We centre horizontally: spawn_x = brick_x + BRICK_W/2 - OPTION_W/2.
 *
 *   Y collision window with paddle (Refresh_Options MAIN.ASM:5579-5586):
 *     limite_y = 416   (Blaster.inc:448  limite_y = limite_x - bord_x = 528-112)
 *     paddle is at Y = limite_y.
 *     Collision is called when:
 *       powerup.pos_y >= limite_y  AND
 *       powerup.pos_y <= limite_y + option_size_y (24)
 *
 *   X collision check (detect_prise_option MAIN.ASM:5617-5624):
 *     option.pos_x >= cursor.pos_x  AND
 *     option.pos_x + option.size_x <= cursor.pos_x + cursor.size_x
 *
 *   Off-screen removal (Refresh_Options MAIN.ASM:5588-5597):
 *     When option.pos_y + option.size_y + sens_y >= screen_y (480) → mark Lost.
 *
 *   Timer (DELAI_OPTION):
 *     Blaster.inc:417  DELAI_OPTION = 600
 *     The falling powerup timer counts down on the ground so it eventually
 *     disappears. We model it as a 600-frame countdown starting when spawned.
 *     In the original ASM the powerup is removed by going off-screen (no ground
 *     timer for the falling sprite); DELAI_OPTION is the *active* powerup timer
 *     set in detect_prise_option (MAIN.ASM:5691). We expose both behaviours:
 *       - powerup_update() moves the sprite down, marks inactive when off-screen.
 *       - The timer field can be used by callers for the active powerup countdown.
 *
 *   Frequency selection (random_options MAIN.ASM:5506-5513):
 *     freq == 0  → never spawn (cmp eax,Off je @@again → skip)
 *     freq == 1  → always spawn (cmp eax,1 je @@forced)
 *     freq >= 2  → call get_random(freq-1); if result != 0 → skip (not spawned)
 *     i.e. spawn probability = 1/(freq-1+1) = 1/freq  for freq >= 2.
 *
 *   struc_options table (MAIN.ASM:6963-6986):
 *     24 entries. Order matches PowerupType enum in constants.h.
 *     Fields: <status, easy_freq, medium_freq, hard_freq, sprite, handler, text>
 */

#include "constants.h"
#include "paddle.h"

/* Paddle Y from ASM: paddle pos_y = limite_y = limite_x - bord_x = 528-112 = 416
 * Blaster.inc:448  limite_y = limite_x - bord_x  */
#define PADDLE_ROW_Y  416   /* Blaster.inc:448  limite_y — actual sprite Y of paddle */

/* Fall speed from MAIN.ASM:5539 */
#define OPTION_FALL_SPEED  2   /* sprite_sens_y = +2 pixels/frame */

/* Frequency value meaning from MAIN.ASM:5506-5513:
 *   0 = never, 1 = always, N >= 2 → get_random(N-1); spawn only if result == 0
 *   so spawn chance = 1/N  (1 in N frames that pass the threshold). */

/* struc_options frequency row — three entries per powerup (easy/medium/hard).
 * Taken directly from MAIN.ASM:6963-6986. */
typedef struct {
    int easy;       /* MAIN.ASM:6950  option_easy  dd ? */
    int medium;     /* MAIN.ASM:6951  option_medium dd ? */
    int hard;       /* MAIN.ASM:6952  option_hard  dd ? */
} PowerupFreq;

/* Powerup sprite instance — one per falling powerup on screen */
typedef struct {
    PowerupType type;
    int x, y;     /* pixel position (top-left of sprite, 26×24) */
    int vy;       /* falling speed (pixels/frame) — MAIN.ASM:5539 sprite_sens_y = +2 */
    int active;   /* 1 while falling/collectible, 0 when gone */
    int timer;    /* DELAI_OPTION countdown: set to 600 on spawn, decrements each frame.
                   * When timer reaches 0 the powerup is considered expired.
                   * (Active powerup duration, Blaster.inc:417  DELAI_OPTION = 600) */
    int owner;    /* F5 P1-ASM-35: player that destroyed the brick (0=P1, 1=P2).
                   * In dual mode the HUD active-option row for P2 draws 25 rows
                   * lower (MAIN.ASM:5525-5533 option_2_decal = screen_x*25). */
} Powerup;

/* Initialise a falling powerup dropped from the centre of a destroyed brick.
 * brick_x, brick_y: top-left pixel coordinates of the brick.
 * Spawn X is centred: brick_x + BRICK_W/2 - OPTION_W/2
 * MAIN.ASM:5537-5539  sprite_pos_x=brick_centre, sprite_pos_y=brick_y, sprite_sens_y=+2 */
void powerup_init(Powerup *p, PowerupType type, int brick_x, int brick_y);

/* Move powerup down by vy, decrement timer.
 * Returns 1 if off-screen (removed) or timer expired (== 0), 0 otherwise.
 * Off-screen condition from MAIN.ASM:5588-5591:
 *   pos_y + option_size_y + sens_y >= screen_y (480)
 * Timer expiry: timer reaches 0 (DELAI_OPTION countdown). */
int powerup_update(Powerup *p);

/* AABB test — checks if powerup overlaps the paddle.
 * Called by Refresh_Options when powerup.pos_y is in [PADDLE_ROW_Y .. PADDLE_ROW_Y + OPTION_H].
 *
 * Y check (MAIN.ASM:5579-5585):
 *   powerup.y >= PADDLE_ROW_Y  AND  powerup.y <= PADDLE_ROW_Y + OPTION_H
 *
 * X check (detect_prise_option MAIN.ASM:5617-5624):
 *   powerup.x >= paddle.x  AND  powerup.x + OPTION_W <= paddle.x + paddle.w
 *
 * Returns 1 if collected, 0 otherwise. */
int powerup_collected(const Powerup *p, const Paddle *paddle);

/* Select a random powerup type based on difficulty.
 *
 * Implements random_options from MAIN.ASM:5472-5513.
 * Algorithm:
 *   1. Pick random index 0..(options_number-2) = 0..22  [MAIN.ASM:5473  options_number-1]
 *      (index 23 = COLLISION is sampled in the pool but its status=Off so it is skipped)
 *   2. Look up frequency for this difficulty.
 *   3. freq == 0 → not valid, try again.
 *   4. freq == 1 → always spawn this type.
 *   5. freq >= 2 → get_random(freq-1); if result != 0 → not spawned this time.
 *
 * diff: DIFFICULTY_EASY=1, DIFFICULTY_MEDIUM=2, DIFFICULTY_HARD=4  (Blaster.inc:16-18)
 *
 * Returns POWERUP_COUNT (24) when no powerup should spawn this call
 * (frequency roll failed).  Caller checks result < POWERUP_COUNT. */
PowerupType powerup_random_type(Difficulty diff);

/* Return the default duration in frames for the active effect of a powerup.
 *   0  = instant effect (applies immediately, no ongoing timer used)
 *  -1  = permanent until replaced by another powerup
 * >0  = duration in frames (DELAI_OPTION=600 unless noted)
 *
 * Derived from detect_current_option_end (MAIN.ASM:6295-6336) and individual
 * handler functions which set current_option=Off immediately for instant types.
 *
 * Instant types (set current_option=Off in their handler):
 *   BALL_3, BALL_6, BALL_9, BALL_20, DEATH, NEW_LIFE, DEL_MONSTER, ADD_MONSTER,
 *   BONUS, MALUS, NEXT_LEVEL
 *
 * Timed types (current_option_count = DELAI_OPTION = 600):
 *   SHOOT, SLOW_BALL, FAST_BALL, IRON_BALL, TELEPOD, NIGHT, SMALL_SHIP,
 *   LARGE_SHIP, REVERSE, MAGNETIC, GHOST, MINI_SHOOT
 *
 * Special:
 *   COLLISION = 0 (2-player only, instant effect) */
int powerup_duration(PowerupType type);

/* Frequency table — 24 rows matching PowerupType enum order.
 * Source: MAIN.ASM:6963-6986  struc_options instances.
 * Exposed so callers can inspect frequencies without calling powerup_random_type. */
extern const PowerupFreq powerup_freq_table[POWERUP_COUNT];
