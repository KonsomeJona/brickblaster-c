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

/* LEVELS_PER_FILE: 31200 / 390 = 80 — validated by FILE.ASM:1205 */
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
