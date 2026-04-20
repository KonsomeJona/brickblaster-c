#include "powerup.h"
/* powerup.c — BrickBlaster Powerup (Option) system
 *
 * All logic derived from MAIN.ASM.  Every numeric constant has a comment
 * citing the ASM file and line number.
 *
 * Integer-only arithmetic throughout.
 */

#include <stdlib.h>  /* rand() used for get_random equivalent */

/* ============================================================================
 * Frequency table
 * Source: MAIN.ASM:6963-6986  struc_options table.
 * Fields per row: <status, easy, medium, hard, sprite_addr, handler_ptr, text>
 * We capture only the three frequency integers.
 *
 * Row order MUST match PowerupType enum (MAIN.ASM:6963-6986 / constants.h).
 *   BALL_3=0, BALL_6=1, BALL_9=2, BALL_20=3,
 *   DEATH=4, NEW_LIFE=5, SHOOT=6, SLOW_BALL=7, FAST_BALL=8,
 *   IRON_BALL=9, TELEPOD=10, NIGHT=11, SMALL_SHIP=12, LARGE_SHIP=13,
 *   REVERSE=14, NEXT_LEVEL=15, DEL_MONSTER=16, MAGNETIC=17,
 *   ADD_MONSTER=18, GHOST=19, BONUS=20, MALUS=21, MINI_SHOOT=22,
 *   COLLISION=23
 * ========================================================================== */
/* F6-01: values now match the shipping Blaster.cfg:16-39 (Freq_Option_*
 * rows). These are the DOS-1999 values that the cfg loader overwrites at
 * startup via FILE.ASM:965-973. Row order matches PowerupType enum. */
const PowerupFreq powerup_freq_table[POWERUP_COUNT] = {
/*  { easy, medium, hard }   PowerupType           Blaster.cfg line  */
    { 2, 3, 4 },  /* POWERUP_BALL_3       cfg:16  Freq_Option_3_ball      */
    { 3, 4, 5 },  /* POWERUP_BALL_6       cfg:17  Freq_Option_6_ball      */
    { 4, 5, 6 },  /* POWERUP_BALL_9       cfg:18  Freq_Option_9_ball      */
    { 8, 9,10 },  /* POWERUP_BALL_20      cfg:19  Freq_Option_20_ball     */
    { 2, 3, 4 },  /* POWERUP_DEATH        cfg:20  Freq_Option_death       */
    { 8, 9,10 },  /* POWERUP_NEW_LIFE     cfg:21  Freq_Option_new_life    */
    { 1, 3, 4 },  /* POWERUP_SHOOT        cfg:22  Freq_Option_shoot       */
    { 2, 3, 4 },  /* POWERUP_SLOW_BALL    cfg:23  Freq_Option_slow_ball   */
    { 2, 3, 4 },  /* POWERUP_FAST_BALL    cfg:24  Freq_Option_fast_ball   */
    { 5, 5, 6 },  /* POWERUP_IRON_BALL    cfg:25  Freq_Option_iron_ball   */
    { 2, 3, 4 },  /* POWERUP_TELEPOD      cfg:26  Freq_Option_telepod     */
    { 5, 6, 7 },  /* POWERUP_NIGHT        cfg:27  Freq_Option_night       */
    { 2, 3, 4 },  /* POWERUP_SMALL_SHIP   cfg:28  Freq_Option_small_ship  */
    { 2, 3, 4 },  /* POWERUP_LARGE_SHIP   cfg:29  Freq_Option_large_ship  */
    { 2, 3, 4 },  /* POWERUP_REVERSE      cfg:30  Freq_Option_reverse     */
    { 9,10,11 },  /* POWERUP_NEXT_LEVEL   cfg:31  Freq_Option_next_level  */
    { 2, 3, 4 },  /* POWERUP_DEL_MONSTER  cfg:32  Freq_Option_del_monster */
    { 2, 3, 4 },  /* POWERUP_MAGNETIC     cfg:33  Freq_Option_magnetic    */
    { 2, 3, 4 },  /* POWERUP_ADD_MONSTER  cfg:34  Freq_Option_add_monster */
    { 3, 4, 5 },  /* POWERUP_GHOST        cfg:35  Freq_Option_ghost       */
    { 2, 3, 4 },  /* POWERUP_BONUS        cfg:36  Freq_Option_bonus       */
    { 2, 3, 4 },  /* POWERUP_MALUS        cfg:37  Freq_Option_malus       */
    { 2, 3, 4 },  /* POWERUP_MINI_SHOOT   cfg:38  Freq_Option_mini_shoot  */
    { 2, 2, 2 },  /* POWERUP_COLLISION    cfg:39  Freq_Option_collision   */
};


/* ============================================================================
 * powerup_init
 *
 * Drop a powerup from the centre of a destroyed brick.
 *
 * MAIN.ASM:5537  mov [edx.sprite_pos_x],ebx    (brick X passed in ebx)
 * MAIN.ASM:5538  mov [edx.sprite_pos_y],edi    (brick Y passed in edi)
 * MAIN.ASM:5539  mov [edx.sprite_sens_y],+2    (fall speed = 2 px/frame)
 *
 * Centering: the ASM caller sets ebx to the brick's grid X (bord_x + col*32).
 * To visually centre the 26-pixel powerup sprite within the 32-pixel brick we
 * shift X by (BRICK_W - OPTION_W) / 2 = (32 - 26) / 2 = 3 pixels.
 * Blaster.inc:352  brique_size_x = 32
 * Blaster.inc:173  option_size_x = 26
 * ========================================================================== */
void powerup_init(Powerup *p, PowerupType type, int brick_x, int brick_y)
{
    p->type   = type;
    /* Centre sprite in brick column: (BRICK_W - OPTION_W) / 2 = 3
     * MAIN.ASM:5537  pos_x = brick_x (caller passes brick column left edge) */
    p->x      = brick_x + (BRICK_W - OPTION_W) / 2;
    p->y      = brick_y;  /* MAIN.ASM:5538 */
    p->vy     = OPTION_FALL_SPEED;  /* MAIN.ASM:5539  sprite_sens_y = +2 */
    p->active = 1;
    p->timer  = DELAI_OPTION;  /* Blaster.inc:417  DELAI_OPTION = 600 */
    p->owner  = 0;  /* F5 P1-ASM-35: default P1; caller overrides per destroyer */
}


/* ============================================================================
 * powerup_update
 *
 * Called once per frame while the powerup is falling.
 *
 * Movement (Refresh_Options MAIN.ASM:5575-5577):
 *   pos_x += sens_x   (sens_x is 0 for options — no horizontal drift)
 *   pos_y += sens_y   (sens_y = +2)
 *
 * Off-screen check (MAIN.ASM:5588-5597):
 *   if (pos_y + option_size_y + sens_y) >= screen_y → mark Lost
 *   Equivalent: pos_y + OPTION_H + vy >= SCREEN_H
 *
 * Timer countdown:
 *   Decrements each frame. Returns 1 when timer reaches 0.
 *
 * Returns 1 if powerup should be removed (off-screen or timer expired), else 0.
 * ========================================================================== */
int powerup_update(Powerup *p)
{
    if (!p->active) return 1;

    /* Move down by vy
     * MAIN.ASM:5575-5577  pos_y += sprite_sens_y */
    p->y += p->vy;

    /* Decrement timer (DELAI_OPTION countdown) */
    if (p->timer > 0) {
        p->timer--;
    }

    /* Off-screen check: MAIN.ASM:5588-5591
     *   add eax,[edx.sprite_size_y]   → pos_y + OPTION_H
     *   add eax,[edx.sprite_sens_y]   → + vy (look-ahead by one frame)
     *   cmp eax,screen_y              → screen_y = 480
     *   jb @@next                     → skip removal if still on screen */
    if (p->y + OPTION_H + p->vy >= SCREEN_H) {
        p->active = 0;
        return 1;
    }

    /* Timer expired */
    if (p->timer <= 0) {
        p->active = 0;
        return 1;
    }

    return 0;
}


/* ============================================================================
 * powerup_collected
 *
 * Test whether the powerup overlaps the paddle (AABB).
 *
 * Y window check (Refresh_Options MAIN.ASM:5579-5585):
 *   mov ebx, limite_y                     → ebx = PADDLE_ROW_Y (416)
 *   cmp [edx.sprite_pos_y], ebx           → powerup.y < PADDLE_ROW_Y?
 *   jb @@next                             → not yet at paddle level
 *   add ebx, [edx.sprite_size_y]          → ebx = PADDLE_ROW_Y + OPTION_H
 *   cmp [edx.sprite_pos_y], ebx           → powerup.y > PADDLE_ROW_Y + OPTION_H?
 *   ja @@cont2                            → passed paddle, skip collection call
 *   call detect_prise_option              → in range, check X overlap
 *
 * X range check (detect_prise_option MAIN.ASM:5617-5624):
 *   mov eax, [edx.sprite_pos_x]           → option.x
 *   cmp eax, cursor_1.sprite_pos_x        → option.x < paddle.x?
 *   jb @@player2                          → below: no overlap
 *   add eax, [edx.sprite_size_x]          → option.x + OPTION_W
 *   mov ebx, cursor_1.sprite_pos_x        → paddle.x
 *   add ebx, cursor_1.sprite_size_x       → paddle.x + paddle.w
 *   cmp eax, ebx                          → option right edge > paddle right edge?
 *   ja @@player2                          → above: no overlap
 *   (falls through to @@detected)         → collected
 *
 * Returns 1 if collected, 0 otherwise.
 * ========================================================================== */
int powerup_collected(const Powerup *p, const Paddle *paddle)
{
    if (!p->active) return 0;

    /* Powerups are drawn at 2x size, centred on the original sprite position.
     * Draw code: ox = p->x - OPTION_W/2, oy = p->y - OPTION_H/2, scaled 2x.
     * Visual bounds: [p->x - OPTION_W/2 .. p->x + 3*OPTION_W/2] x same for y.
     * Hitbox matches the visual. */
    int vis_left   = p->x - OPTION_W / 2;
    int vis_right  = vis_left + OPTION_W * 2;
    int vis_top    = p->y - OPTION_H / 2;
    int vis_bottom = vis_top + OPTION_H * 2;

    /* Y: visual must overlap the paddle row window */
    if (vis_bottom < PADDLE_ROW_Y) return 0;
    if (vis_top > PADDLE_ROW_Y + OPTION_H * 2) return 0;

    /* X: visual must overlap the paddle */
    if (vis_right <= paddle->x) return 0;
    if (vis_left >= paddle->x + paddle->w) return 0;

    return 1;
}


/* ============================================================================
 * powerup_random_type
 *
 * Select a powerup type for spawning, mirroring random_options (MAIN.ASM:5452-5552).
 *
 * The ASM algorithm:
 *   1. Increment and compare delai_random_option against threshold (caller's job).
 *      We skip that step; the caller decides when to call this function.
 *
 *   2. Pick random index 0..(options_number-2) = 0..22  (MAIN.ASM:5473):
 *        mov eax, options_number-1   → eax = 23
 *        call get_random             → eax = rand() % 23 → [0..22]
 *      (Note: options_number=24, options_number-1=23 passed to get_random which
 *       returns 0..N-1 = 0..22. Index 23 = COLLISION has status=Off so even if
 *       rolled it would be rejected by the status check.)
 *
 *   3. Look up [eax.option_status]: if Off → skip (@@end with no spawn).
 *      COLLISION (index 23) has status=Off unless 2-player mode.
 *
 *   4. Load frequency for current difficulty (MAIN.ASM:5493-5504).
 *
 *   5. Frequency gate (MAIN.ASM:5506-5513):
 *        cmp eax,1   je @@forced     → freq==1: always spawn
 *        cmp eax,Off je @@again      → freq==0: retry random (skip)
 *        dec eax
 *        call get_random             → rand() % (freq-1)
 *        or eax,eax
 *        jnz @@end                   → non-zero: no spawn this call
 *      → jz falls through to @@forced: spawn.
 *
 * Returns POWERUP_COUNT when no powerup should spawn.
 *
 * Note: this function performs a single candidate draw without the inter-frame
 * delay (delai_random_option / delai_between_option). The caller must implement
 * the timing gate.
 * ========================================================================== */
PowerupType powerup_random_type(Difficulty diff)
{
    int freq;
    int roll;
    /* MAIN.ASM:5473  mov eax,options_number-1 → range is 0..22
     * options_number = 24  (MAIN.ASM:6959) */
    int idx = rand() % (POWERUP_COUNT - 1);  /* 0..22, index 23 (COLLISION) excluded */

    /* COLLISION (index 23) has status=Off; since we only pick 0..22 it is
     * automatically excluded — matching MAIN.ASM behaviour where its status
     * flag prevents it from ever being picked in random mode.
     * MAIN.ASM:5482-5483  cmp [eax.option_status],Off  je @@end */

    /* Look up frequency for this difficulty
     * MAIN.ASM:5493-5504 */
    switch (diff) {
        case DIFFICULTY_MEDIUM:
            freq = powerup_freq_table[idx].medium;
            break;
        case DIFFICULTY_HARD:
            freq = powerup_freq_table[idx].hard;
            break;
        case DIFFICULTY_EASY:
        default:
            freq = powerup_freq_table[idx].easy;
            break;
    }

    /* MAIN.ASM:5506  cmp eax,1  je @@forced → freq==1: always spawn */
    if (freq == 1) {
        return (PowerupType)idx;
    }

    /* MAIN.ASM:5508  cmp eax,Off  je @@again → freq==0: never spawn this entry
     * We return POWERUP_COUNT to signal "no spawn" */
    if (freq == 0) {
        return (PowerupType)POWERUP_COUNT;
    }

    /* MAIN.ASM:5510-5513
     *   dec eax              → freq - 1
     *   call get_random      → rand() % (freq-1)
     *   or eax,eax
     *   jnz @@end            → non-zero: no spawn
     * Spawn only when get_random returns 0, i.e. probability = 1/(freq-1). */
    roll = rand() % (freq - 1);
    if (roll != 0) {
        return (PowerupType)POWERUP_COUNT;  /* no spawn */
    }

    return (PowerupType)idx;
}


/* ============================================================================
 * powerup_duration
 *
 * Return default effect duration in frames.
 *   0  = instant (handler sets current_option = Off immediately)
 *  -1  = permanent until replaced
 * 600  = DELAI_OPTION (set in detect_prise_option MAIN.ASM:5691)
 *
 * Instant powerups — their handlers call:
 *   mov current_option,Off  immediately (MAIN.ASM:6352,6367,6382,6394,...)
 *
 * Timed powerups use the DELAI_OPTION=600 counter set at collection time
 * (MAIN.ASM:5691  mov current_option_count,DELAI_OPTION).
 * ========================================================================== */
int powerup_duration(PowerupType type)
{
    switch (type) {
        /* --- Instant-effect powerups ---
         * handler sets current_option=Off right after applying effect. */
        case POWERUP_BALL_3:      /* MAIN.ASM:6344  option_3_ball_p:    mov current_option,Off */
        case POWERUP_BALL_6:      /* MAIN.ASM:6358  option_6_ball_p:    mov current_option,Off */
        case POWERUP_BALL_9:      /* MAIN.ASM:6374  option_9_ball_p:    mov current_option,Off */
        case POWERUP_BALL_20:     /* MAIN.ASM:6388  option_20_ball_p:   mov current_option,Off */
        case POWERUP_DEATH:       /* MAIN.ASM:6404  option_death_p:     mov current_option,Off */
        case POWERUP_NEW_LIFE:    /* MAIN.ASM:6426  option_new_life_p:  mov current_option,Off */
        case POWERUP_DEL_MONSTER: /* MAIN.ASM:6731  option_del_monster_p: instant destruction */
        case POWERUP_ADD_MONSTER: /* MAIN.ASM:6760  option_add_monster_p: instant spawn */
        case POWERUP_BONUS:       /* MAIN.ASM:6802  option_bonus_p:     instant score */
        case POWERUP_MALUS:       /* MAIN.ASM:6816  option_malus_p:     instant score */
        case POWERUP_NEXT_LEVEL:  /* MAIN.ASM:6726  option_next_level_p: instant level advance */
        case POWERUP_COLLISION:   /* MAIN.ASM:6830  option_collision_p: instant 2P effect */
        case POWERUP_GHOST:       /* MAIN.ASM:6788  mov current_option,Off — ghost is instant;
                                   * ghost balls self-destruct on contact, no timed state needed */
            return 0;

        /* --- Timed powerups --- duration = DELAI_OPTION = 600 frames
         * All set current_option_count = DELAI_OPTION at collection
         * (MAIN.ASM:5691  mov current_option_count,DELAI_OPTION). */
        case POWERUP_SHOOT:       /* MAIN.ASM:6509  option_shoot_p      */
        case POWERUP_SLOW_BALL:   /* MAIN.ASM:6602  option_slow_ball_p  */
        case POWERUP_FAST_BALL:   /* MAIN.ASM:6621  option_fast_ball_p  */
        case POWERUP_IRON_BALL:   /* MAIN.ASM:6453  option_iron_ball_p  */
        case POWERUP_TELEPOD:     /* MAIN.ASM:6470  option_telepod_p    */
        case POWERUP_NIGHT:       /* MAIN.ASM:6641  option_night_p — count
                                   * set by shared pipeline (MAIN.ASM:5691),
                                   * NOT by the handler itself. option_night_p
                                   * clears current_option back to Off but
                                   * count keeps ticking; @@current_option_off
                                   * eventually calls init_palette → clears
                                   * night_flag. Kept in timed list so that
                                   * collecting NIGHT while another timed
                                   * option is active properly deactivates
                                   * the prior (implicit in ASM via shared
                                   * pipeline overwrite). */
        case POWERUP_SMALL_SHIP:  /* MAIN.ASM:6491  option_small_ship_p */
        case POWERUP_LARGE_SHIP:  /* MAIN.ASM:6500  option_large_ship_p */
        case POWERUP_REVERSE:     /* MAIN.ASM:6562  option_reverse_p    */
        case POWERUP_MAGNETIC:    /* MAIN.ASM:6742  option_magnetic_p   */
        case POWERUP_MINI_SHOOT:  /* MAIN.ASM:6554  option_mini_shoot_p */
            return DELAI_OPTION;  /* 600 frames — Blaster.inc:417 */

        default:
            return 0;
    }
}
