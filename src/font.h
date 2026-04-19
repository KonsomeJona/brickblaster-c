#pragma once
/*
 * font.h - Bitmap font from FONTE.ASM
 *
 * The original FONTE.ASM defines a single-row sprite sheet (FONTE.png)
 * containing all characters at:
 *   x = 1 + (char_index * 16),  y = 1
 *   each character is 15x22 pixels
 *
 * Character set (order matches FONTE.ASM Fonte label, line 418):
 *   a-z (26), 0-9 (10), + - # ! ? : . & <space> (9)  => 45 total
 *
 * FONTE.ASM constants (lines 413-419):
 *   Fonte_size_x    = 15   (character width)
 *   Fonte_size_y    = 22   (character height)
 *   Fonte_next_char = 16   (horizontal stride in sprite sheet)
 *   Fonte_Next_Line = 30   (vertical stride for newline, in screen pixels)
 *   fonte_screen_x  = 800  (sprite sheet / raw bitmap width)
 *   Fonte_Adrs      = 001 + (fonte_screen_x * 001)  = first char at (1, 1)
 */

#include "raylib.h"

/* FONTE.ASM:413 - Fonte_size_x */
#define FONT_CHAR_W     15
/* FONTE.ASM:414 - Fonte_size_y */
#define FONT_CHAR_H     22
/* FONTE.ASM:415 - Fonte_next_char (horizontal stride between chars in sheet) */
#define FONT_STRIDE     16
/* FONTE.ASM:416 - Fonte_Next_Line (vertical stride for newline in screen) */
#define FONT_NEXT_LINE  30

/* Number of characters in the charset (FONTE.ASM:419, $-Fonte) */
#define FONT_CHARSET_SIZE  45

typedef struct {
    Texture2D *sheet;   /* pointer to loaded FONTE texture (owned by Assets) */
    int char_w;         /* character width  = FONT_CHAR_W  (15 px) */
    int char_h;         /* character height = FONT_CHAR_H  (22 px) */
    int cols;           /* number of characters across the sheet (= FONT_CHARSET_SIZE) */
} BitmapFont;

/*
 * font_init - Initialise a BitmapFont from a pre-loaded texture pointer.
 * 'sheet' must remain valid for the lifetime of BitmapFont.
 */
void font_init(BitmapFont *f, Texture2D *sheet);

/*
 * font_draw_char - Draw a single character at (x, y) with given tint.
 * Silently skips characters not in the charset.
 */
void font_draw_char(BitmapFont *f, char c, int x, int y, Color tint);

/*
 * font_draw_string - Draw a NUL-terminated string starting at (x, y).
 * Characters are separated by FONT_CHAR_W pixels.
 * Unknown characters are treated as space (drawn blank).
 */
void font_draw_string(BitmapFont *f, const char *s, int x, int y, Color tint);

/*
 * font_char_to_frame - Pure function: return the sprite frame index (0-based)
 * for character c, or -1 if the character is not in the charset.
 *
 * Uppercase letters are mapped to lowercase (the ASM charset is lowercase-only).
 * Space always returns the last frame index (FONT_CHARSET_SIZE - 1 = 44).
 *
 * This function has no side-effects and does NOT call any raylib APIs,
 * so it is safe to call in headless tests.
 *
 * FONTE.ASM:320-329 - the repne scasb loop that searches Fonte[] for the char,
 * then computes the frame index as (edi - Fonte_base).
 */
int font_char_to_frame(char c);

/*
 * font_string_width - Return the total pixel width of string s
 * (number of characters * FONT_CHAR_W).
 */
int font_string_width(const BitmapFont *f, const char *s);
