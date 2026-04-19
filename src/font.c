/*
 * font.c - Bitmap font implementation translated from FONTE.ASM
 *
 * ASM reference: BrickBlaster/work400/Blaster/FONTE.ASM
 *
 * Key translation notes:
 *  - The original ASM scans the Fonte[] byte array with "repne scasb" to find
 *    the character index, then multiplies by Fonte_next_char (16) and adds
 *    Fonte_Adrs (801) to get the byte offset into the raw 800-wide bitmap.
 *  - In C/raylib we reproduce this as: frame = index; src_x = 1 + index * 16.
 *  - The raw bitmap offset of 801 = 1 + (800 * 1) means the first character
 *    starts at pixel (x=1, y=1) in the sprite sheet.
 *  - All drawing is done via DrawTextureRec(), which is the raylib equivalent
 *    of the rep movsb / transparence-mode loops in the ASM.
 */

#include "font.h"
#include <string.h>  /* strlen */

/*
 * FONTE.ASM:418 - The exact character table:
 *   Fonte db 'abcdefghijklmnopqrstuvwxyz0123456789+-#!?:.& '
 *
 * Index 0 = 'a', index 25 = 'z', index 26 = '0', index 35 = '9',
 * index 36 = '+', index 37 = '-', index 38 = '#', index 39 = '!',
 * index 40 = '?', index 41 = ':', index 42 = '.', index 43 = '&',
 * index 44 = ' '  (space, always the last / fallback character)
 *
 * FONTE.ASM:419 - Fonte_Size = $-Fonte  (= 45)
 */
static const char s_charset[] =
    "abcdefghijklmnopqrstuvwxyz"  /* FONTE.ASM:418 - indices 0-25  */
    "0123456789"                   /* FONTE.ASM:418 - indices 26-35 */
    "+-#!?:.& ";                   /* FONTE.ASM:418 - indices 36-44 */

/* Compile-time guard: charset must be exactly FONT_CHARSET_SIZE characters. */
/* (sizeof includes NUL terminator, so subtract 1)                           */
typedef char _charset_size_check[
    (sizeof(s_charset) - 1 == FONT_CHARSET_SIZE) ? 1 : -1
];


/* ------------------------------------------------------------------
 * font_init
 * ------------------------------------------------------------------ */
void font_init(BitmapFont *f, Texture2D *sheet)
{
    f->sheet  = sheet;
    f->char_w = FONT_CHAR_W;   /* FONTE.ASM:413 - Fonte_size_x = 15 */
    f->char_h = FONT_CHAR_H;   /* FONTE.ASM:414 - Fonte_size_y = 22 */
    f->cols   = FONT_CHARSET_SIZE;
}


/* ------------------------------------------------------------------
 * font_char_to_frame  (pure function, no raylib)
 *
 * Mirrors the ASM logic at FONTE.ASM:320-329:
 *
 *   lea  edi, Fonte          ; point edi at charset table
 *   mov  ecx, Fonte_size     ; length = 45
 *   repne scasb              ; scan for AL (the character)
 *   dec  edi                 ; point back at the found byte
 *   lea  esi, Fonte
 *   sub  edi, esi            ; index = edi - Fonte_base
 *
 * Then:
 *   mov  esi, [Fonte_Tbl + edi*4]   ; lookup table entry
 *   add  esi, Fonte_Buffer           ; absolute byte ptr in raw bitmap
 *
 * In C we just return the index; the caller converts it to pixel coords.
 * ------------------------------------------------------------------ */
int font_char_to_frame(char c)
{
    int i;

    /* FONTE.ASM:318 - lodsb; the ASM loads the raw byte from the text string.
     * Uppercase is handled here: the ASM charset is all lowercase, but game
     * text may be uppercased.  The Godot port (bitmap_font.gd:27) confirms
     * "var lower := c.to_lower()" before searching.                          */
    if (c >= 'A' && c <= 'Z') {
        c = (char)(c + ('a' - 'A'));
    }

    /* FONTE.ASM:320-329 - repne scasb loop: find c in Fonte[] */
    for (i = 0; i < FONT_CHARSET_SIZE; i++) {
        if (s_charset[i] == c) {
            return i;   /* FONTE.ASM:326 - edi - Fonte_base = index */
        }
    }

    return -1;  /* character not in charset */
}


/* ------------------------------------------------------------------
 * font_draw_char
 *
 * Computes the source rectangle in FONTE.png from the frame index and
 * calls DrawTextureRec().
 *
 * FONTE.ASM:411 - Fonte_Adrs = 001 + (fonte_screen_x * 001)
 *   This is the byte offset of the first character in the raw 800-wide
 *   bitmap: x = 1 (Fonte_Adrs % 800 = 1), y = 1 (Fonte_Adrs / 800 = 1).
 *
 * FONTE.ASM:415 - Fonte_next_char = 16
 *   Each successive character is 16 pixels further right in the sheet.
 *
 * So character at index i has:
 *   src_x = 1 + i * 16     (FONTE.ASM:411,415)
 *   src_y = 1               (FONTE.ASM:411)
 *   width = 15              (FONTE.ASM:413 - Fonte_size_x)
 *   height = 22             (FONTE.ASM:414 - Fonte_size_y)
 *
 * ------------------------------------------------------------------ */
void font_draw_char(BitmapFont *f, char c, int x, int y, Color tint)
{
    Rectangle src;
    Vector2   dst;
    int frame;

    if (!f || !f->sheet) return;

    frame = font_char_to_frame(c);
    if (frame < 0) return;  /* not in charset - silently skip */

    /* FONTE.ASM:411 - x offset = 1 + (frame * Fonte_next_char) */
    src.x      = (float)(1 + frame * FONT_STRIDE);   /* FONTE.ASM:411,415 */
    src.y      = 1.0f;                                /* FONTE.ASM:411     */
    src.width  = (float)FONT_CHAR_W;                  /* FONTE.ASM:413     */
    src.height = (float)FONT_CHAR_H;                  /* FONTE.ASM:414     */

    dst.x = (float)x;
    dst.y = (float)y;

    DrawTextureRec(*f->sheet, src, dst, tint);
}


/* ------------------------------------------------------------------
 * font_draw_string
 *
 * Draws each character in s at successive x positions.
 *
 * FONTE.ASM:357-358:
 *   pop  ebx          ; restore x position
 *   add  ebx, edx    ; ebx += Fonte_size_x  (advance by char width)
 *
 * Each character advances the x cursor by FONT_CHAR_W (15) pixels.
 * Note: the ASM uses Fonte_size_x (15) for advance, NOT Fonte_next_char (16).
 * The stride of 16 is only the sprite-sheet spacing; drawn characters are 15px.
 * ------------------------------------------------------------------ */
void font_draw_string(BitmapFont *f, const char *s, int x, int y, Color tint)
{
    if (!f || !f->sheet || !s) return;

    while (*s) {
        font_draw_char(f, *s, x, y, tint);
        x += f->char_w;   /* FONTE.ASM:357-358 - advance by Fonte_size_x = 15 */
        s++;
    }
}


/* ------------------------------------------------------------------
 * font_string_width
 * ------------------------------------------------------------------ */
int font_string_width(const BitmapFont *f, const char *s)
{
    if (!f || !s) return 0;
    return (int)strlen(s) * f->char_w;  /* FONTE.ASM:413 - Fonte_size_x = 15 */
}
