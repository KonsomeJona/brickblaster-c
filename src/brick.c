#include "brick.h"

/* brick.c — BrickBlaster Brick system
 *
 * Ground truth: MAIN.ASM detect_brique (line 3947).
 * Every magic number is referenced to its ASM source.
 */

/* brick_init — decode raw byte into a Brick struct.
 *
 * Grid → pixel coordinate formula from MAIN.ASM:4882-4883:
 *   bord_x  = 112  (BRICK_ORIGIN_X, Blaster.inc:445)
 *   bord_y  = 0    (BRICK_ORIGIN_Y, Blaster.inc:446)
 *   x = bord_x + col * brique_size_x   (brique_size_x=32, Blaster.inc:352)
 *   y = bord_y + row * brique_size_y   (brique_size_y=16, Blaster.inc:353)
 *
 * Row/col from index:
 *   col = index % BRICK_COLS   (MAIN.ASM:3962  shr ebx,5  after offset / BRICK_W)
 *   row = index / BRICK_COLS   (MAIN.ASM:3969  shr eax,4  after offset / BRICK_H)
 *
 * Byte decode (Blaster.inc:390-406):
 *   color = (raw & 0xC0) >> 6    (couleur_de_brique mask)
 *   type  = raw & 0x38           (type_de_brique mask)
 *   hp    = raw & 0x1F           (resistance_de_brique mask)
 *
 * active: 0x00 (absente) or 0xFF (invalide) → not active.
 */
void brick_init(Brick *b, int index, unsigned char raw)
{
    int col, row;

    b->raw   = raw;
    b->index = index;
    b->last_hit_owner = 0;  /* default P1 — overwritten at damage time */

    /* Grid position */
    col  = index % BRICK_COLS;   /* Blaster.inc:453  nbs_brique_x = 13 */
    row  = index / BRICK_COLS;   /* Blaster.inc:454  nbs_brique_y = 30 */

    /* Pixel position — MAIN.ASM:4882-4883 */
    b->x = BRICK_ORIGIN_X + col * BRICK_W;  /* bord_x + col*brique_size_x */
    b->y = BRICK_ORIGIN_Y + row * BRICK_H;  /* bord_y + row*brique_size_y */

    /* Decode byte — Blaster.inc:390-406 */
    b->color = (raw & COULEUR_DE_BRIQUE) >> 6;   /* bits 7-6 → index 0-3  */
    b->type  = (BrickType)(raw & TYPE_DE_BRIQUE); /* bits 5-3 */
    b->hp    = (int)(raw & RESISTANCE_DE_BRIQUE); /* bits 4-0 */

    /* TRANSPARENTE type bit (0x10) overlaps with the HP mask (0x1F bit 4).
     * Raw byte 0x10 → hp = 0x10 & 0x1F = 16, but intended HP is the lower nibble.
     * Strip the type bit: use bits 3-0 only, floor at 1 so the brick is present.
     * Blaster.inc:401  transparente = 0x10 (bit 4 = type) */
    if (b->type == BRICK_TRANSPARENT) {
        b->hp = (int)(raw & 0x0F);  /* lower nibble, excludes type bit 4 */
        if (b->hp < 1) b->hp = 1;
    }

    /* Active: empty (0x00/absente) and invalid (0xFF/invalide) are not active.
     * MAIN.ASM:3977  cmp al,absente  je @@end
     * MAIN.ASM:3979  cmp al,invalide je @@end */
    if (raw == ABSENTE || raw == INVALIDE) {
        b->active = 0;
    } else {
        b->active = 1;
    }
}

/* brick_hit — apply damage to a brick.
 *
 * MAIN.ASM:3981  cmp al,incassable
 *                jae @@collision
 *   The incassable (0x08) test uses JAE (jump if above-or-equal), which means
 *   both BRICK_INDESTRUCTIBLE (0x08) and BRICK_TELEPORTER (0x18) hit this path.
 *   For our purposes we treat INCASSABLE as never destructible.
 *
 * MAIN.ASM:3996  dec B [esi+ebx]    — damage is always 1 (dec = subtract 1)
 * MAIN.ASM:4004  cmp B [esi+ebx],absente (0x00)
 *                jne @@redraw_brique — if not zero, brick just damaged, not done
 *                → stc + ret         — if zero, destroyed (carry flag set = true)
 *
 * Returns 1 if destroyed, 0 otherwise.
 */
int brick_hit(Brick *b, int damage)
{
    /* Indestructible and teleporter bricks never take damage.
     * MAIN.ASM:3981  cmp al,incassable; jae @@collision
     * JAE catches both BRICK_INDESTRUCTIBLE (0x08) and BRICK_TELEPORTER (0x18). */
    if (b->type == BRICK_INDESTRUCTIBLE || b->type == BRICK_TELEPORTER) {
        return 0;
    }

    /* Non-active bricks cannot be hit */
    if (!b->active) {
        return 0;
    }

    /* Apply damage — MAIN.ASM:3996 dec B [esi+ebx] (damage=1 from dec) */
    b->hp -= damage;

    /* Clamp to zero — raw byte cannot go below absente (0) */
    if (b->hp < 0) {
        b->hp = 0;
    }

    /* Destroyed when hp reaches zero — MAIN.ASM:4004 cmp B [esi+ebx],absente */
    if (b->hp == 0) {
        b->active = 0;
        return 1;
    }

    return 0;
}
