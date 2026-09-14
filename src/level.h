#pragma once
/* level.h - Level file loading and grid helpers
 *
 * FILE.ASM: Read_Level (line 1184) loads the entire .lv? file at once.
 * The file must be exactly 31,200 bytes (FILE.ASM:1205 "cmp ecx,31200").
 * Each level is 390 raw bytes (BRICK_COLS*BRICK_ROWS = 13*30) stored
 * sequentially with no header.  Level N starts at byte offset:
 *   (N-1) * BRICK_COUNT
 * (MAIN.ASM:1711-1714  "dec eax; imul eax,nbs_brique_x*nbs_brique_y")
 *
 * Three world files: Blaster.lv0 (space), Blaster.lv1, blaster.lv2
 * FILE.ASM:1195-1196  "mov eax,'.lv?'; mov al,world"
 *
 * Brick byte encoding (Blaster.inc:390-406):
 *   Bits 7-6: Color (COULEUR_DE_BRIQUE = 0xC0)
 *   Bits 5-3: Type  (TYPE_DE_BRIQUE    = 0x38)
 *   Bits 2-0: HP    (RESISTANCE_DE_BRIQUE = 0x07) -- only low 3 bits; full mask 0x1F
 *   0x00 = empty (ABSENTE), 0xFF = invalid (INVALIDE)
 */

#include "constants.h"

/* LEVELS_PER_FILE: 31200 / 390 = 80 — FILE storage CAPACITY (FILE.ASM:1205
 * "cmp ecx,31200" validates the file size, nothing more).  This is NOT the
 * number of playable levels: the 1999 .lv files hold 40 real levels followed
 * by 40 empty 0xFF slots.  Use level_count() for the playable count. */
#define LEVELS_PER_FILE  80

/* One level loaded from a .lv0/.lv1/.lv2 file
 * FILE.ASM: Read_Level — level data section */
typedef struct {
    unsigned char bricks[BRICK_COUNT]; /* 390 raw encoded bytes, row-major */
    int cols;                          /* always BRICK_COLS = 13 */
    int rows;                          /* always BRICK_ROWS = 30 */
    int brick_count;                   /* number of non-empty, non-invalid slots */
    int world;                         /* 0 = Blaster.lv0, 1 = Blaster.lv1, 2 = blaster.lv2 */
} Level;

/* Load level by number (1-based) from the appropriate .lv# file.
 * FILE.ASM:1184  Read_Level
 *   - Opens the world file matching `world` byte ('0','1','2')
 *   - Verifies file size == 31200
 *   - Level N offset: (N-1) * BRICK_COUNT  (MAIN.ASM:1711-1714)
 * Returns 0 on success, -1 on error. */
int level_load(Level *lvl, const char *path, int level_num);

/* Number of PLAYABLE levels in the given world's .lv file (0/1/2).
 * MAIN.ASM:5025-5041  search_level_number:
 *     mov level_number,0
 *   @@again:
 *     cmp B [esi],-1          ; first byte of the 390-byte block == 0xFF ?
 *     je @@end                ; yes → stop, level_number holds the count
 *     inc level_number
 *     add esi,nbs_brique_x*nbs_brique_y
 * Counts 390-byte level blocks until the first one starting with 0xFF
 * (invalide).  Measured on the 1999 data files: each .lv? is 31,200 bytes
 * whose last 15,600 bytes are all 0xFF — 40 playable levels + 40 empty
 * slots per world.  Returns 0 if the file is missing or has a bad size.
 * Result is cached per world, and honours the player's edited copy of the
 * world when there is one (see level_read_world). */
int level_count(int world);

/* -----------------------------------------------------------------------
 * Worlds and the player's edited copies (port extension, not in the ASM)
 *
 * World files: Blaster.lv0 (space), Blaster.lv1 (arcade), blaster.lv2 (the
 * third world that MAIN.ASM:495-501 @@coin_coin leaves commented out, never
 * selectable) and Blaster.lv3 (atoll, added by the port).
 *
 * The in-game editor never touches the shipped files. It saves a complete
 * 31,200-byte copy, <user dir>/custom.lv<world>, which then REPLACES the
 * shipped world everywhere the game reads it: campaign, level count, demo.
 * The copy is byte-compatible with the standalone editor and with the 1999
 * executable's own .lv files.
 * ----------------------------------------------------------------------- */
#define LEVEL_FILE_SIZE  31200
#define LEVEL_WORLDS     4
#define WORLD_ATOLL      3

/* Directory of the edited copies, with its trailing slash. Default "data/"
 * (next to blaster.scr); the web build points it at its IndexedDB mount. */
void level_set_user_dir(const char *dir);
const char *level_user_dir(void);

/* Path of the edited copy for `world`, whether or not it exists. */
void level_user_path(int world, char *out, int n);

/* Read a whole world (LEVEL_FILE_SIZE bytes) into buf. With allow_user, the
 * edited copy wins when it exists and has the right size. Returns 0 on
 * success, -1 when neither file is readable. */
int level_read_world(int world, unsigned char *buf, int allow_user);

/* Write / delete the edited copy, then drop the level_count cache. 0 on
 * success. Deleting a copy that does not exist also succeeds. */
int level_write_user_world(int world, const unsigned char *buf);
int level_remove_user_world(int world);

/* Load level N of `world` (edited copy first). Same contract as level_load. */
int level_load_world(Level *lvl, int world, int level_num);

/* search_level_number on an in-memory world buffer. */
int level_count_buffer(const unsigned char *buf);

/* Forget every cached level_count (after an import or an edit). */
void level_count_invalidate(void);

/* An emptied last level is not a level: turn trailing all-empty levels back
 * into 0xFF slots (at least one level is kept). */
void level_trim_world(unsigned char *buf);

/* EDITOR.ASM brush bytes: brush 0 normal (21), 1 multi (24), 2 indestructible
 * (08), 3 transparent (11), 4 teleporter (18); colour 0..3 in bits 7-6. */
unsigned char level_brush_code(int brush, int color);

/* Background set (sprites/0S_NN.png) and sprite palette of a world. */
int level_world_bg_set(int world);
int level_world_palette(int world);

/* -----------------------------------------------------------------------
 * Grid helper — pure functions, testable without file I/O
 * MAIN.ASM:1703-1709  brick index → row/col via shr/imul
 * ----------------------------------------------------------------------- */

/* Column of brick at linear index.
 * MAIN.ASM:1708  "shr ebx,5"  (divide x offset by brique_size_x=32 → col) */
int level_brick_col(int index);   /* index % BRICK_COLS */

/* Row of brick at linear index.
 * MAIN.ASM:1702  "shr eax,4"  (divide y offset by brique_size_y=16 → row) */
int level_brick_row(int index);   /* index / BRICK_COLS */

/* Screen X of brick at linear index.
 * MAIN.ASM:4882  "mov ebx,bord_x"  origin + col*brique_size_x */
int level_brick_x(int index);     /* BRICK_ORIGIN_X + col * BRICK_W */

/* Screen Y of brick at linear index.
 * MAIN.ASM:4883  "mov edi,bord_y"  origin + row*brique_size_y */
int level_brick_y(int index);     /* BRICK_ORIGIN_Y + row * BRICK_H */

/* -----------------------------------------------------------------------
 * Brick byte decode — mirrors BRICK_COLOR/BRICK_TYPE/BRICK_HP macros from
 * constants.h but as functions so tests can call them via forward-declare.
 * Blaster.inc:390-406
 * ----------------------------------------------------------------------- */

/* Color index 0-3 from bits 7-6 of brick byte.
 * Blaster.inc:393  couleur_de_brique = 0C0h  → shift right 6 */
int level_brick_color(unsigned char b);

/* Non-zero if brick byte encodes any type (bits 5-3 non-zero).
 * Blaster.inc:399  type_de_brique = 038h */
int level_brick_is_special(unsigned char b);

/* Hit-points from bits 4-0 of brick byte.
 * Blaster.inc:391  resistance_de_brique = 01Fh */
int level_brick_hp(unsigned char b);
