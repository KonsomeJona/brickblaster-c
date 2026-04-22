#pragma once
/* brick.h — BrickBlaster Brick system
 *
 * Brick handling derived from MAIN.ASM detect_brique (line 3947).
 *
 * MAIN.ASM:3947   detect_brique — collision / hit processing routine
 * MAIN.ASM:3977   cmp al,absente → skip empty cell
 * MAIN.ASM:3979   cmp al,invalide → skip invalid cell
 * MAIN.ASM:3981   cmp al,incassable → indestructible path (jae @@collision)
 * MAIN.ASM:3996   dec B [esi+ebx] — decrement raw HP byte by 1 (damage = 1)
 * MAIN.ASM:4004   cmp B [esi+ebx],absente → destroyed when byte hits 0 (absente=0)
 *
 * Brick byte encoding (Blaster.inc:390-406):
 *   Bits 7-6  Color   (COULEUR_DE_BRIQUE = 0xC0)
 *   Bits 5-3  Type    (TYPE_DE_BRIQUE    = 0x38)
 *   Bits 4-0  HP      (RESISTANCE_DE_BRIQUE = 0x1F)
 *
 * Grid coordinate formula (MAIN.ASM:4882-4883):
 *   x = BRICK_ORIGIN_X + col * BRICK_W
 *   y = BRICK_ORIGIN_Y + row * BRICK_H
 */

#include "constants.h"
#include "level.h"

typedef struct {
    unsigned char raw;   /* original encoded byte                      */
    int hp;              /* hit-points (bits 4-0 of raw)               */
    int color;           /* 0-3 decoded from bits 7-6                  */
    BrickType type;      /* BRICK_NORMAL / BRICK_INDESTRUCTIBLE / …    */
    int active;          /* 1 = exists on grid                         */
    int x, y;            /* pixel position of top-left corner          */
    int index;           /* 0-389 grid index                           */
    int last_hit_owner;  /* 0=P1, 1=P2 — stamped at damage time so the
                          * deferred transparent-brick sync loop can
                          * attribute score to the correct player.
                          * MAIN.ASM:4024-4026 sprite_player dispatch. */
    int reflet_timer;    /* Indestructible reflet animation countdown.
                          * Stamped to NBS_REFLET on ball impact
                          * (MAIN.ASM:4058-4061), decremented each frame
                          * (MAIN.ASM:6220-6240) back to 0 = base beton. */
} Brick;

/* Initialise a Brick from a grid index and its raw encoded byte.
 * MAIN.ASM:4882-4883  screen XY from col/row
 * Blaster.inc:390-406 byte layout */
void brick_init(Brick *b, int index, unsigned char raw);

/* Apply one hit to a brick.
 * MAIN.ASM:3981  cmp al,incassable  — INCASSABLE bricks: always returns 0,
 *                                      hp is left unchanged.
 * MAIN.ASM:3996  dec B [esi+ebx]    — NORMAL bricks: hp -= damage (damage=1
 *                                      from the dec instruction).
 * MAIN.ASM:4004  cmp B,absente      — returns 1 (destroyed) when hp reaches 0.
 * Returns 1 if the brick was destroyed, 0 otherwise. */
int brick_hit(Brick *b, int damage);
