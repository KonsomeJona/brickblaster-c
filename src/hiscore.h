#pragma once
/* hiscore.h  EBrickBlaster high score system
 * Translated from HISCORE.ASM and FILE.ASM (score section).
 * HISCORE.ASM:391-521  struc_score, score tables, Load/Save/Code_Score
 * FILE.ASM:394         Load_Score / Save_Score use extension '.scr'
 */

#include "constants.h"
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * struc_score layout  EHISCORE.ASM:479-485
 *
 *   struc_score  struc
 *     score_value   dd ?            ; 4 bytes  Ebinary integer score
 *     score_text    db '000000  '   ; 8 bytes  EASCII score "000000  "
 *     score_level   db '00  '       ; 4 bytes  EASCII level "00  "
 *     score_name    db '???????????????'  ; 15 bytes  Eplayer name
 *     score_end     db ?            ; 1 byte   Esentinel (13 or 0)
 *   struc_score  ends
 *
 * Total: 4 + 8 + 4 + 15 + 1 = 32 bytes.
 * score_end is 13 (CR) for entries 1-14, 0 for entry 15 (last).
 * HISCORE.ASM:487-501  score_1..score_15 (solo mode)
 * HISCORE.ASM:503-517  coop_score_1..coop_score_15 (coop mode)
 * --------------------------------------------------------------------------- */
#if defined(_MSC_VER)
#pragma pack(push,1)
#endif
typedef struct {
    int   value;                        /* score_value:  4 bytes, binary score  */
    char  score_text[8];                /* score_text:   8 bytes, "000000  "    */
    char  level_text[4];                /* score_level:  4 bytes, "00  "        */
    char  name[HISCORE_NAME_LEN];       /* score_name:  15 bytes, player name   */
    char  end;                          /* score_end:    1 byte,  13 or 0       */
#if defined(__GNUC__)
} __attribute__((packed)) HiscoreEntry;
#else
} HiscoreEntry;
#pragma pack(pop)
#endif

/* Compile-time size assertion will be enforced in hiscore.c */

/* ---------------------------------------------------------------------------
 * Full hiscore table: 2 modes ÁE15 entries
 * HISCORE.ASM:519  Score_Size = size struc_score * nbs_score * 2
 *                            = 32 * 15 * 2 = 960 bytes
 * --------------------------------------------------------------------------- */
typedef struct {
    HiscoreEntry entries[HISCORE_MODES][HISCORE_ENTRIES];
} Hiscores;

/* ---------------------------------------------------------------------------
 * XOR encode/decode  EHISCORE.ASM:454-466  Code_Score
 *
 * The same function is used for both encoding and decoding (XOR is its own
 * inverse).  Applied to the entire flat buffer of score_size (960) bytes.
 *
 *   mov ecx, score_size       ; ecx = 960
 * @@decod:
 *   lodsb                     ; al = buf[i]
 *   xor al, cl                ; cl = low byte of ecx = (score_size - i) & 0xFF
 *   stosb
 *   loop @@decod              ; ecx--
 *
 * So:  buf[i] ^= (score_size - i) & 0xFF;
 * --------------------------------------------------------------------------- */
void hiscore_xor_encode(unsigned char *buf, int len);  /* in-place encode/decode */

/* ---------------------------------------------------------------------------
 * File I/O  EHISCORE.ASM:391-451  Load_Score / Save_Score
 * File extension: '.scr'  (HISCORE.ASM:394  mov eax,'.scr')
 *
 * hiscore_load: open path, read HISCORE_SAVE_SIZE bytes, XOR-decode.
 *               If file missing, leaves *hs unchanged (defaults persist).
 *               Returns 0 on success, -1 on error.
 *
 * hiscore_save: XOR-encode a copy, write HISCORE_SAVE_SIZE bytes to path,
 *               XOR-decode the copy back so caller's data stays readable.
 *               Returns 0 on success, -1 on error.
 * --------------------------------------------------------------------------- */
int hiscore_load(Hiscores *hs, const char *path);
int hiscore_save(const Hiscores *hs, const char *path);

/* ---------------------------------------------------------------------------
 * Score management  EHISCORE.ASM:216-274  detect_new_score / insert_new_score
 *
 * hiscore_qualifies: returns 1 if score beats the last (lowest) entry in the
 *   table for the given mode.  mode 0 = solo, mode 1 = coop.
 *
 * hiscore_insert: insert a new entry at the correct rank (descending order),
 *   shifting lower entries down.  Entry 15 (index 14) is always overwritten.
 *   name must be a NUL-terminated string of up to HISCORE_NAME_LEN chars.
 *   score_text and level_text are formatted from score/level integers.
 *   Returns rank (0-based index) of inserted entry, or -1 if not inserted.
 *
 *   ASM insert logic (detect_new_score + @@insert_new_score):
 *     scan from top; when player_score >= entry_score, shift tail down
 *     by one slot (memmove-style byte loop), then write new entry at that slot.
 *   HISCORE.ASM:229-274
 * --------------------------------------------------------------------------- */
int hiscore_qualifies(const Hiscores *hs, int mode, int score);
int hiscore_insert(Hiscores *hs, int mode, const char *name, int score, int level);

/* ---------------------------------------------------------------------------
 * Default table initialisation  Ematches ASM data section defaults
 * HISCORE.ASM:487-517  score_1..15 initialised with value=1..15,
 *   score_text='000000  ', score_level='00  ', name='???????????????',
 *   score_end=13 (entries 1-14) or 0 (entry 15)
 * --------------------------------------------------------------------------- */
void hiscore_init_defaults(Hiscores *hs);
