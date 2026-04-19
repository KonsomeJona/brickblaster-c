// constants.h  EBrickBlaster game constants
// Translated from BrickBlaster/work400/Blaster/Blaster.inc and HISCORE.ASM
// Every constant has a source reference: // Blaster.inc:LINE or // MAIN.ASM:LINE
#pragma once

// ============================================================================
// SCREEN / DISPLAY
// ============================================================================

#define SCREEN_W        640     // Blaster.inc:423  screen_x
#define SCREEN_H        480     // Blaster.inc:424  screen_y

// Play area boundaries (bord_x/bord_y/limite_x)
#define PLAY_X1         112     // Blaster.inc:445  bord_x
#define PLAY_Y1           0     // Blaster.inc:446  bord_y
#define PLAY_X2         528     // Blaster.inc:447  limite_x
// limite_y = limite_x - bord_x = 416 (playable width in pixels)
#define PLAY_W          416     // Blaster.inc:448  limite_y (misnamed  Eit is width)
#define SCREEN_CENTER   320     // Blaster.inc:449  screen_center = ((528-112)/2)+112

// Sound
#define SB_FREQ         44000   // Blaster.inc:428  Sb_Freq

// ============================================================================
// BRICK GRID
// ============================================================================

#define BRICK_W          32     // Blaster.inc:352  brique_size_x
#define BRICK_H          16     // Blaster.inc:353  brique_size_y

// nbs_brique_x = (limite_x - bord_x) / brique_size_x = (528-112)/32 = 13
#define BRICK_COLS       13     // Blaster.inc:453  nbs_brique_x
// nbs_brique_y = screen_y / brique_size_y = 480/16 = 30
#define BRICK_ROWS       30     // Blaster.inc:454  nbs_brique_y
#define BRICK_COUNT     390     // BRICK_COLS * BRICK_ROWS

// Brick grid origin in screen space
#define BRICK_ORIGIN_X  112     // = PLAY_X1 = bord_x
#define BRICK_ORIGIN_Y    0     // = PLAY_Y1 = bord_y

// Reflection/shine animation frames for indestructible bricks
#define NBS_REFLET        5     // Blaster.inc:350  nbs_reflet

// ============================================================================
// BALL
// ============================================================================

#define BALL_W            9     // Blaster.inc:334  ball_size
#define BALL_H            9     // Blaster.inc:334  ball_size (square sprite)
// NBS_BALL_MAX = 19  Earray index max (0..18), so 19 slots = up to 19 balls
#define BALL_MAX         19     // Blaster.inc:135  NBS_BALL_MAX

// Ball speed limits  EASM comment: +angle ↁE(min_x=1,max_x=14), -angle ↁE(min_y=2,max_y=15)
#define SPEED_MIN_X       1     // Blaster.inc:408  speed_min_x
#define SPEED_MIN_Y       2     // Blaster.inc:409  speed_min_y
#define SPEED_MAX_X      14     // Blaster.inc:410  speed_max_x
#define SPEED_MAX_Y      15     // Blaster.inc:411  speed_max_y
#define SPEED_MAX        15     // Blaster.inc:412  speed_max
#define ANGLE_MAX         1     // Blaster.inc:413  angle_max (only 0 or 1)

// Anti-stuck counters  Ereset ball direction if stuck bouncing in horizontal/vertical loop
#define BLOCAGE_COUNTER  60     // Blaster.inc:387  BLOCAGE_COUNTER
#define BLOCAGE_COUNTER_2 3     // Blaster.inc:388  BLOCAGE_COUNTER_2

// Sprite sheet offset between consecutive ball sprites
#define NEXT_BALL        11     // Blaster.inc:335  next_ball

// ============================================================================
// PADDLE (VAISSEAU)
// ============================================================================

// Normal paddle dimensions
#define PADDLE_W         74     // Blaster.inc:227  vaisseau_size_x
#define PADDLE_H         25     // Blaster.inc:228  vaisseau_size_y

// Large paddle width
#define PADDLE_LARGE_W  105     // Blaster.inc:286  vaisseau_large_size_x
#define PADDLE_LARGE_H   25     // Blaster.inc:287  vaisseau_large_size_y

// Small paddle width
#define PADDLE_SMALL_W   38     // Blaster.inc:291  vaisseau_small_size_x
#define PADDLE_SMALL_H   25     // Blaster.inc:292  vaisseau_small_size_y

// Paddle Y position  Epaddle sits at bottom of play area; actual Y computed at runtime
// from screen_y minus panel height (panel_option_pos_y = 458-12 = 446)
// NOTE: panel_option_pos_y=446 is a UI panel constant, NOT the paddle sprite position.
// MAIN.ASM:1774 Init_Cursor: mov cursor_1.sprite_pos_y,limite_y = 528-112 = 416
#define PADDLE_Y        416     // MAIN.ASM:1774  cursor_1.sprite_pos_y = limite_y = 416

// Paddle edge zone: 23px from each side changes ball angle
#define CURSOR_BORDER    23     // Blaster.inc:451  cursor_border

// Paddle shooting gun offsets
#define VAISSEAU_DECAL_X_L   7  // Blaster.inc:234  vaisseau_decal_x_l (left gun X from left edge)
#define VAISSEAU_DECAL_Y_L  -7  // Blaster.inc:235  vaisseau_decal_y_l (left gun Y above paddle top)
// vaisseau_decal_x_r = vaisseau_size_x - vaisseau_decal_x_l - vaisseau_tir_size_x = 74-7-12 = 55
#define VAISSEAU_DECAL_X_R  55  // Blaster.inc:236  vaisseau_decal_x_r
#define VAISSEAU_DECAL_Y_R  -7  // Blaster.inc:237  vaisseau_decal_y_r
#define VAISSEAU_DECAL_X_B  31  // Blaster.inc:246  vaisseau_decal_x_b (big gun X)
#define VAISSEAU_DECAL_Y_B  -8  // Blaster.inc:247  vaisseau_decal_y_b (big gun Y)

// Projectile dimensions
#define VAISSEAU_TIR_SIZE_X      12  // Blaster.inc:238  vaisseau_tir_size_x
#define VAISSEAU_TIR_SIZE_Y      18  // Blaster.inc:239  vaisseau_tir_size_y
#define VAISSEAU_TIR_NBS_ANIM     6  // Blaster.inc:241  vaisseau_tir_nbs_anim
#define VAISSEAU_TIR_SPEED        1  // Blaster.inc:242  vaisseau_tir_speed
#define VAISSEAU_TIR_BIG_SIZE_X  13  // Blaster.inc:248  vaisseau_tir_big_size_x
#define VAISSEAU_TIR_BIG_SIZE_Y  19  // Blaster.inc:249  vaisseau_tir_big_size_y
#define VAISSEAU_TIR_BIG_NBS_ANIM 4  // Blaster.inc:251  vaisseau_tir_big_nbs_anim
#define VAISSEAU_TIR_BIG_SPEED    1  // Blaster.inc:252  vaisseau_tir_big_speed

// ============================================================================
// MONSTERS
// ============================================================================

#define NBS_MONSTER       4     // Blaster.inc:118  nbs_monster (max monsters on screen)
#define MONSTER_W        32     // Blaster.inc:121  monster_size_x
#define MONSTER_H        32     // Blaster.inc:122  monster_size_y
#define MONSTER_NBS_ANIM 16     // Blaster.inc:124  monster_nbs_anim
#define MONSTER_SPEED     5     // Blaster.inc:125  monster_speed (frames between anim updates)

// Monster explosion
#define EXPLO_W          70     // Blaster.inc:128  explo_size_x
#define EXPLO_H          70     // Blaster.inc:129  explo_size_y
#define EXPLO_NBS_ANIM   13     // Blaster.inc:130  explo_nbs_anim
#define EXPLO_SPEED       1     // Blaster.inc:131  explo_speed

// ============================================================================
// POWERUPS (OPTIONS)
// ============================================================================

#define OPTION_W         26     // Blaster.inc:173  option_size_x
#define OPTION_H         24     // Blaster.inc:174  option_size_y
#define NEXT_OPTION      26     // Blaster.inc:142  next_option (sprite sheet offset between powerups)
#define POWERUP_COUNT    24     // MAIN.ASM:6959     options_number

// ============================================================================
// TIMING (frames at ~60fps)
// ============================================================================

#define DELAI_TELEPOD     5     // Blaster.inc:416  DELAI_TELEPOD (teleport animation delay)
#define DELAI_OPTION    600     // Blaster.inc:417  DELAI_OPTION (powerup active duration)
#define DELAI_DEMO      800     // Blaster.inc:420  DELAI_DEMO (frames of inactivity before demo)
#define DELAI_BETWEEN_OPTION 60 // FILE.ASM:1139  delai_between_option (frames between powerup spawns)

// Iter 2 fix #13: missing timing constants (Blaster.inc:415,418,419,421 + MAIN.ASM intro)
#define DELAI_DATTENTE    600   // Blaster.inc:415  DELAI_DATTENTE (idle frames in menu before demo)
#define DELAI_INFO        500   // Blaster.inc:418  DELAI_INFO (info screen display duration)
#define DELAI_INFO_SOUND   50   // Blaster.inc:419  DELAI_INFO_SOUND (delay before info screen SFX)
#define DELAI_DEMO_MAX     -1   // Blaster.inc:421  DELAI_DEMO_MAX (sentinel: demo runs until input)
#define DELAY_INTRO_1      60   // MAIN.ASM intro timing — first intro stage length (frames)

// Paddle speed
#define PADDLE_SPEED      6     // MOUSE.ASM:77  speed_counter dd 6

// Credits screen timing
#define CREDITS_SLIDE_TIMEOUT 300  // MAIN.ASM:154,163,172,181,190 - 5 seconds @ 60 FPS

// ============================================================================
// SPRITE ANIMATION
// ============================================================================

// Brick break animation
#define BREAK_NBS_ANIM    5     // Blaster.inc:6    break_nbs_anim
#define BREAK_SPEED       1     // Blaster.inc:7    break_speed

// Shoot animation
#define SHOOT_W           8     // Blaster.inc:108  shoot_size_x
#define SHOOT_H          42     // Blaster.inc:109  shoot_size_y
#define SHOOT_NBS_ANIM   11     // Blaster.inc:110  shoot_nbs_anim
#define SHOOT_SPEED       5     // Blaster.inc:111  shoot_speed

// Mini shoot
#define MINI_SHOOT_W      3     // Blaster.inc:115  mini_shoot_size_x
#define MINI_SHOOT_H     13     // Blaster.inc:116  mini_shoot_size_y

// ============================================================================
// UI PANEL POSITIONS (screen coordinates)
// ============================================================================

#define PANEL_LEVEL_POS_X    123    // Blaster.inc:206  panel_level_pos_x
#define PANEL_LEVEL_POS_Y      9    // Blaster.inc:207  panel_level_pos_y
#define PANEL_LEVEL_W         30    // Blaster.inc:204  panel_level_size_x
#define PANEL_LEVEL_H         22    // Blaster.inc:205  panel_level_size_y

#define PANEL_SCORE_POS_X    426    // Blaster.inc:214  panel_score_pos_x
#define PANEL_SCORE_POS_Y      9    // Blaster.inc:215  panel_score_pos_y
#define PANEL_SCORE_W         90    // Blaster.inc:212  panel_score_size_x
#define PANEL_SCORE_H         22    // Blaster.inc:213  panel_score_size_y

// panel_option_pos_x = bord_x + 10 = 112 + 10 = 122
#define PANEL_OPTION_POS_X   122    // Blaster.inc:193  panel_option_pos_x
// panel_option_pos_y = 458 - 12 = 446
#define PANEL_OPTION_POS_Y   446    // Blaster.inc:194  panel_option_pos_y
#define PANEL_OPTION_W        26    // Blaster.inc:191  panel_option_size_x
#define PANEL_OPTION_H        24    // Blaster.inc:192  panel_option_size_y

#define PANEL_INFO_POS_X     195    // Blaster.inc:200  panel_info_pos_x
#define PANEL_INFO_POS_Y     446    // Blaster.inc:201  panel_info_pos_y (= panel_option_pos_y)
#define PANEL_INFO_W         270    // Blaster.inc:198  panel_info_size_x
#define PANEL_INFO_H          22    // Blaster.inc:199  panel_info_size_y

// panel_nbs_ball_pos_x = limite_x - 10 = 528 - 10 = 518
#define PANEL_NBS_BALL_POS_X 518    // Blaster.inc:221  panel_nbs_ball_pos_x
#define PANEL_NBS_BALL_POS_Y1 458   // Blaster.inc:222  panel_nbs_ball_pos_y_1
#define PANEL_NBS_BALL_POS_Y2 469   // Blaster.inc:223  panel_nbs_ball_pos_y_2
#define PANEL_NBS_BALL_W       9    // Blaster.inc:219  panel_nbs_ball_size_x

#define PANEL_MUSIC_POS_X    437    // Blaster.inc:44   panel_music_pos_x
#define PANEL_MUSIC_POS_Y    454    // Blaster.inc:45   panel_music_pos_y
#define PANEL_MUSIC_W         72    // Blaster.inc:42   panel_music_size_x
#define PANEL_MUSIC_H         19    // Blaster.inc:43   panel_music_size_y

// ============================================================================
// LEVEL EDITOR
// ============================================================================



// ============================================================================
// ICON / MENU
// ============================================================================



// ============================================================================
// HIGH SCORE
// ============================================================================
// Source: HISCORE.ASM:475-519

#define HISCORE_ENTRIES       15    // HISCORE.ASM:475  NBS_SCORE (entries per mode)
#define HISCORE_MODES          2    // HISCORE.ASM:487-517  two tables: score_1..15 and coop_score_1..15
#define HISCORE_NAME_LEN      15    // HISCORE.ASM:477  name_size
// struc_score layout: score_value(4) + score_text(8:'000000  ') + score_level(4:'00  ') + score_name(15) + score_end(1) = 32
#define HISCORE_ENTRY_SIZE    32    // HISCORE.ASM:479-485  size struc_score (4+8+4+15+1)
// Total save data: HISCORE_ENTRY_SIZE * HISCORE_ENTRIES * HISCORE_MODES = 32 * 15 * 2 = 960
#define HISCORE_SAVE_SIZE    960    // HISCORE.ASM:519  Score_Size = size struc_score * nbs_score * 2

// ============================================================================
// BRICK ENCODING MACROS
// Brick byte layout: [CC|TTT|HHHHH] = color(2)|type(3)|hitpoints(5)
// ============================================================================

// Color mask (bits 7-6)
// Note: BRICK_ prefix used to avoid collision with raylib color defines
#define COULEUR_DE_BRIQUE    0xC0   // Blaster.inc:393
#define BRICK_COLOR_GREEN    0x00   // Blaster.inc:394  verte  (Green)
#define BRICK_COLOR_BLUE     0x40   // Blaster.inc:395  bleu   (Blue)
#define BRICK_COLOR_VIOLET   0x80   // Blaster.inc:396  violet (Violet/Purple)
#define BRICK_COLOR_ORANGE   0xC0   // Blaster.inc:397  orange (Orange)

// Type mask (bits 5-3)
#define TYPE_DE_BRIQUE       0x38   // Blaster.inc:399
#define NORMALE              0x20   // Blaster.inc:400  Normal breakable brick
#define TRANSPARENTE         0x10   // Blaster.inc:401  Transparent (ball passes through)
#define INCASSABLE           0x08   // Blaster.inc:402  Indestructible
#define TELEPORTEUSE         0x18   // Blaster.inc:403  Teleporter

// Special byte values
#define ABSENTE              0x00   // Blaster.inc:405  Empty cell
#define INVALIDE             0xFF   // Blaster.inc:406  Invalid/unused

// Hitpoints mask (bits 4-0)
#define RESISTANCE_DE_BRIQUE 0x1F   // Blaster.inc:391  HP mask

// Convenience extraction macros
#define BRICK_TYPE(b)    ((b) & TYPE_DE_BRIQUE)

// ============================================================================
// POWERUP ENUM
// Order MUST match MAIN.ASM:6963-6986 struc_options table exactly.
// Verified against commit c672fcb which fixed BONUS/MALUS swap.
// ============================================================================

typedef enum {
    POWERUP_BALL_3       =  0,  // MAIN.ASM:6963  Option_3_ball      "   1 ball"
    POWERUP_BALL_6       =  1,  // MAIN.ASM:6964  Option_6_ball      "  3 balls"
    POWERUP_BALL_9       =  2,  // MAIN.ASM:6965  Option_9_ball      "  6 balls"
    POWERUP_BALL_20      =  3,  // MAIN.ASM:6966  Option_20_ball     "  20 balls"
    POWERUP_DEATH        =  4,  // MAIN.ASM:6967  Option_death       " lost life"
    POWERUP_NEW_LIFE     =  5,  // MAIN.ASM:6968  Option_new_life    "  new life"
    POWERUP_SHOOT        =  6,  // MAIN.ASM:6969  Option_shoot       " big shoot"
    POWERUP_SLOW_BALL    =  7,  // MAIN.ASM:6970  Option_slow_ball   "    slow"
    POWERUP_FAST_BALL    =  8,  // MAIN.ASM:6971  Option_fast_ball   "    fast"
    POWERUP_IRON_BALL    =  9,  // MAIN.ASM:6972  Option_iron_ball   " iron ball"
    POWERUP_TELEPOD      = 10,  // MAIN.ASM:6973  Option_telepod     "  telepod"
    POWERUP_NIGHT        = 11,  // MAIN.ASM:6974  Option_night       "  point x2"
    POWERUP_SMALL_SHIP   = 12,  // MAIN.ASM:6975  Option_small_ship  " small ship"
    POWERUP_LARGE_SHIP   = 13,  // MAIN.ASM:6976  Option_large_ship  " large ship"
    POWERUP_REVERSE      = 14,  // MAIN.ASM:6977  Option_reverse     "  reverse"
    POWERUP_NEXT_LEVEL   = 15,  // MAIN.ASM:6978  Option_next_level  " next level"
    POWERUP_DEL_MONSTER  = 16,  // MAIN.ASM:6979  Option_del_monster "wrath of god"
    POWERUP_MAGNETIC     = 17,  // MAIN.ASM:6980  Option_magnetic    "  magnetic"
    POWERUP_ADD_MONSTER  = 18,  // MAIN.ASM:6981  Option_add_monster "summon beast"
    POWERUP_GHOST        = 19,  // MAIN.ASM:6982  Option_ghost       "   ghost"
    POWERUP_BONUS        = 20,  // MAIN.ASM:6983  Option_bonus       "  +100 pts" (sprite offset 21)
    POWERUP_MALUS        = 21,  // MAIN.ASM:6984  Option_malus       "  -100 pts" (sprite offset 20)
    POWERUP_MINI_SHOOT   = 22,  // MAIN.ASM:6985  Option_mini_shoot  "   shoot"
    POWERUP_COLLISION    = 23   // MAIN.ASM:6986  Option_collision   " collision"
} PowerupType;

// ============================================================================
// BRICK TYPE ENUM
// ============================================================================

typedef enum {
    BRICK_ABSENT         = 0x00,  // Blaster.inc:405  absente   Eempty cell
    BRICK_NORMAL         = 0x20,  // Blaster.inc:400  normale
    BRICK_TRANSPARENT    = 0x10,  // Blaster.inc:401  transparente
    BRICK_INDESTRUCTIBLE = 0x08,  // Blaster.inc:402  incassable
    BRICK_TELEPORTER     = 0x18,  // Blaster.inc:403  teleporteuse
    BRICK_INVALID        = 0xFF   // Blaster.inc:406  invalide
} BrickType;

// ============================================================================
// DIFFICULTY ENUM
// ============================================================================

typedef enum {
    DIFFICULTY_EASY   = 1,  // Blaster.inc:16  EASY
    DIFFICULTY_MEDIUM = 2,  // Blaster.inc:17  MEDIUM
    DIFFICULTY_HARD   = 4   // Blaster.inc:18  HARD
} Difficulty;

// ============================================================================
// CONTROL TYPE ENUM
// ============================================================================

typedef enum {
    CONTROL_COMPUTER = 8,   // Blaster.inc:19  COMPUTER
    CONTROL_KEYBOARD = 16,  // Blaster.inc:20  KEYBOARD
    CONTROL_JOYSTICK = 32   // Blaster.inc:21  JOYSTICK
} ControlType;

// ============================================================================
// PLAYER
// ============================================================================

#define PLAYER_ONE  1  // Blaster.inc:13
#define PLAYER_TWO  2  // Blaster.inc:14

// ============================================================================
// ============================================================================

