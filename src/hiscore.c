/* hiscore.c — BrickBlaster high score system
 * Translated from HISCORE.ASM and FILE.ASM (score section).
 * Pure logic — no raylib, no display.
 *
 * HISCORE.ASM:391-521  Load_Score, Save_Score, Code_Score, detect_new_score,
 *                       struc_score data, NBS_SCORE, Score_Size
 * FILE.ASM:394          extension '.scr' used by Init_File in Load_Score/Save_Score
 */

#include "hiscore.h"

#include <stdio.h>
#include <string.h>

/* Compile-time size check — struc_score must be exactly 32 bytes.
 * HISCORE.ASM:479-485  4+8+4+15+1 = 32
 * The __attribute__((packed)) on the struct ensures no padding. */
typedef char assert_hiscore_entry_size[
    (sizeof(HiscoreEntry) == HISCORE_ENTRY_SIZE) ? 1 : -1
];

/* ---------------------------------------------------------------------------
 * hiscore_xor_encode — HISCORE.ASM:454-466  Code_Score
 *
 * Original ASM:
 *   mov ecx, score_size   ; ecx = 960 (total buffer size)
 *   lea edi, score_1
 *   mov esi, edi
 * @@decod:
 *   lodsb                 ; al = *esi++
 *   xor al, cl            ; XOR with low byte of ecx
 *   stosb                 ; *edi++ = al
 *   loop @@decod          ; ecx-- and jump if ecx != 0
 *
 * ecx starts at 960 and decrements AFTER the XOR. So on the first iteration
 * cl = 960 & 0xFF = 0xC0. On the last (960th) iteration cl = 1 & 0xFF = 1.
 * Formula: buf[i] ^= (len - i) & 0xFF,  for i in [0, len).
 *
 * The operation is its own inverse (XOR property), so encode == decode.
 * Applied once before write; applied again after read to restore plain data.
 * HISCORE.ASM:409     call Code_Score  (after load, to decode)
 * HISCORE.ASM:418     call Code_Score  (before write, to encode)
 * HISCORE.ASM:436     call Code_Score  (after write, to restore in memory)
 * --------------------------------------------------------------------------- */
void hiscore_xor_encode(unsigned char *buf, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        /* cl = low 8 bits of ecx; ecx starts at len, is len-i at iteration i */
        unsigned char counter = (unsigned char)((len - i) & 0xFF); /* HISCORE.ASM:461 xor al,cl */
        buf[i] ^= counter;
    }
}

/* ---------------------------------------------------------------------------
 * hiscore_init_defaults — HISCORE.ASM:487-517
 *
 * Initialise table to match the ASM data-section defaults:
 *   score_value  = entry_number (1-based)
 *   score_text   = "000000  "  (8 chars, 6 digits + 2 spaces)
 *   score_level  = "00  "      (4 chars, 2 digits + 2 spaces)
 *   score_name   = "???????????????"  (15 question marks)
 *   score_end    = 13 for entries 1-14, 0 for entry 15 (last)
 *
 * HISCORE.ASM:487  score_1  struc_score <1, '000000  ', '00  ', '???????????????', 13>
 * HISCORE.ASM:501  score_15 struc_score <15,'000000  ', '00  ', '???????????????',  0>
 * Same pattern repeated for coop_score_1..coop_score_15 (HISCORE.ASM:503-517).
 * --------------------------------------------------------------------------- */
void hiscore_init_defaults(Hiscores *hs)
{
    int m, e;
    for (m = 0; m < HISCORE_MODES; m++) {
        for (e = 0; e < HISCORE_ENTRIES; e++) {
            HiscoreEntry *en = &hs->entries[m][e];
            en->value = e + 1;                           /* 1-based initial values */
            memcpy(en->score_text, "000000  ", 8);       /* HISCORE.ASM:481 */
            memcpy(en->level_text, "00  ",     4);       /* HISCORE.ASM:482 */
            memset(en->name, '?', HISCORE_NAME_LEN);     /* HISCORE.ASM:483 '???????????????' */
            /* score_end: 13 for entries 0-13 (1-based 1-14), 0 for entry 14 (1-based 15) */
            en->end = (e < HISCORE_ENTRIES - 1) ? 13 : 0; /* HISCORE.ASM:487-501 */
        }
    }
}

/* ---------------------------------------------------------------------------
 * hiscore_load — HISCORE.ASM:391-411  Load_Score
 *
 * Original ASM:
 *   mov eax, '.scr'          ; HISCORE.ASM:394
 *   call Init_File            ; build filename from exe path + '.scr'
 *   mov ax, 3d00h             ; DOS open for reading
 *   _int 21h
 *   jc @@end                  ; if error, skip (keep defaults)
 *   mov bx, ax                ; file handle
 *   lea edx, score_1
 *   mov ecx, score_size       ; 960 bytes
 *   mov ax, 3f00h             ; DOS read
 *   _int 21h
 *   mov ax, 3e00h             ; DOS close
 *   _int 21h
 *   call Code_Score            ; XOR-decode in place
 * @@end:
 *   ret
 *
 * Returns 0 on success, -1 if file cannot be opened/read.
 * --------------------------------------------------------------------------- */
int hiscore_load(Hiscores *hs, const char *path)
{
    FILE *f;
    size_t n;
    /* Read into temp buffer so a partial read (truncated / interrupted file)
     * cannot corrupt the in-memory defaults that the caller populated.
     * Only commit to hs->entries once the full 960 bytes are present. */
    unsigned char tmp[HISCORE_SAVE_SIZE];

    f = fopen(path, "rb");
    if (!f) {
        return -1;  /* HISCORE.ASM:397 jc @@end — file not found is non-fatal */
    }

    n = fread(tmp, 1, HISCORE_SAVE_SIZE, f);
    fclose(f);

    if ((int)n != HISCORE_SAVE_SIZE) {
        /* Partial read — keep in-memory defaults intact. */
        return -1;
    }

    memcpy(hs->entries, tmp, HISCORE_SAVE_SIZE);
    /* HISCORE.ASM:409  call Code_Score — decode after read */
    hiscore_xor_encode((unsigned char *)hs->entries, HISCORE_SAVE_SIZE);

    return 0;
}

/* ---------------------------------------------------------------------------
 * hiscore_save — HISCORE.ASM:415-451  Save_Score
 *
 * Original ASM:
 *   call Code_Score            ; XOR-encode in place  (HISCORE.ASM:418)
 *   mov eax, '.scr'
 *   call Init_File
 *   mov ax, 3d02h              ; open read/write
 *   _int 21h
 *   jc @@create                ; if not exists, create it
 * @@write:
 *   mov bx, ax
 *   lea edx, score_1
 *   mov ecx, score_size
 *   mov ax, 4000h              ; DOS write
 *   _int 21h
 *   mov ax, 3e00h              ; close
 *   _int 21h
 *   call Code_Score            ; XOR-decode back (HISCORE.ASM:436)
 *   ret
 * @@create:
 *   mov eax, '.scr'
 *   call Init_File
 *   mov ecx, 0
 *   mov ax, 3c00h              ; create file
 *   _int 21h
 *   jnc @@write
 *   call Code_Score            ; restore on failure (HISCORE.ASM:448)
 *   ret
 *
 * We encode a private copy to avoid mutating caller's data — equivalent to
 * the ASM encode+write+decode triple at HISCORE.ASM:418, 436.
 * Returns 0 on success, -1 on error.
 * --------------------------------------------------------------------------- */
int hiscore_save(const Hiscores *hs, const char *path)
{
    FILE *f;
    size_t n;
    /* Working copy so we can XOR-encode without modifying caller's data.
     * This mirrors the ASM: encode in-place, write, decode in-place. */
    Hiscores tmp;
    memcpy(&tmp, hs, sizeof(Hiscores));

    /* HISCORE.ASM:418  call Code_Score — encode before write */
    hiscore_xor_encode((unsigned char *)tmp.entries, HISCORE_SAVE_SIZE);

    /* HISCORE.ASM:421  try open for writing; HISCORE.ASM:440 create if missing */
    f = fopen(path, "wb");
    if (!f) {
        return -1;
    }

    n = fwrite(tmp.entries, 1, HISCORE_SAVE_SIZE, f);
    fclose(f);

    /* tmp is stack-allocated; no need to XOR-decode it back. */

    return ((int)n == HISCORE_SAVE_SIZE) ? 0 : -1;
}

/* ---------------------------------------------------------------------------
 * hiscore_qualifies — HISCORE.ASM:216-274  detect_new_score
 *
 * Mirrors the ASM scan-loop first-match semantics: qualifies on the first
 * entry where player_score >= entry_score. Default table values are 1..15
 * (see hiscore_init_defaults) so a first-time score >= 1 qualifies at the
 * top rank, and a score >= 10 qualifies at rank 10 — matching the ASM.
 *
 * Checking only the last entry (value 15) locked first-time players out
 * entirely, because any score below 15 failed the test even though the
 * ASM path would have inserted it near the top.
 *
 * HISCORE.ASM:228-234 @@again loop; jae → insert
 * mode 0 = solo (score_1..score_15), mode 1 = coop (coop_score_1..coop_score_15).
 * --------------------------------------------------------------------------- */
int hiscore_qualifies(const Hiscores *hs, int mode, int score)
{
    int i;
    if (mode < 0 || mode >= HISCORE_MODES) return 0;
    for (i = 0; i < HISCORE_ENTRIES; i++) {
        if (score >= hs->entries[mode][i].value) return 1;  /* HISCORE.ASM:230 jae */
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * hiscore_insert — HISCORE.ASM:216-274  detect_new_score + @@insert_new_score
 *
 * Scan from top; on first entry where player_score >= entry_score, shift the
 * tail down by one slot (last entry is lost), then write the new entry.
 *
 * ASM logic — detect_new_score (HISCORE.ASM:216-274):
 *   mov eax, player_1.player_counter_score
 *   lea esi, score_1.score_value            ; point at first entry
 *   mov ecx, nbs_score                      ; 15
 * @@again:
 *   cmp eax, D[esi]           ; player_score >= entry_score?
 *   jae @@insert_new_score    ; yes → insert here
 *   add esi, size struc_score ; advance to next entry
 *   loop @@again
 *   ; fall through → no slot found
 *
 * @@insert_new_score:
 *   push esi
 *   dec ecx                   ; remaining entries below insert point
 *   jecxz @@end               ; inserting at last slot → no shift needed
 *   mov eax, ecx
 *   imul eax, size struc_score
 *   mov ecx, eax              ; byte count to shift
 *   mov edi, esi
 *   add edi, size struc_score ; destination = one slot below
 * @@insert:
 *   mov al, B[esi+ecx-1]     ; copy bytes backwards (memmove)
 *   mov B[edi+ecx-1], al
 *   loop @@insert
 *   ; zero out sentinel of last entry (now bumped out)
 *   mov score_15.score_end, 0
 * @@end:
 *   pop esi
 *   ; write new entry at esi
 *   push player_1.player_counter_score
 *   pop [esi.score_value]
 *   mov ax, W player_1.player_score_txt+1 ; 6-digit score text (3 words)
 *   mov W [esi.score_text+0], ax
 *   ... (3 word copies for score_text)
 *   mov ax, W level_txt+1                 ; level text (1 word = 2 chars)
 *   mov W [esi.score_level], ax
 *   lea eax, [esi.score_name]
 *   mov name_adrs, eax                    ; name filled in by get_name later
 *
 * In our C translation we format score_text and level_text from integers,
 * and copy the name directly (no interactive entry in this pure function).
 *
 * Returns 0-based rank of inserted entry, or -1 if score does not qualify.
 *
 * HISCORE.ASM:229  @@again scan
 * HISCORE.ASM:239  @@insert_new_score shift
 * HISCORE.ASM:258  write new entry fields
 * --------------------------------------------------------------------------- */
int hiscore_insert(Hiscores *hs, int mode, const char *name, int score, int level)
{
    int i, rank;
    HiscoreEntry *table;
    HiscoreEntry *slot;

    if (mode < 0 || mode >= HISCORE_MODES) return -1;

    table = hs->entries[mode];
    rank  = -1;

    /* HISCORE.ASM:228-234  scan for insertion point */
    for (i = 0; i < HISCORE_ENTRIES; i++) {
        if (score >= table[i].value) {  /* HISCORE.ASM:230 cmp eax,D[esi]; jae */
            rank = i;
            break;
        }
    }

    if (rank == -1) return -1;  /* score does not make the table */

    /* HISCORE.ASM:239-255  shift entries below rank down by one slot.
     * ASM copies bytes backwards (high to low index within the moved block)
     * to avoid overwriting source before it's copied — equivalent to memmove
     * when source < destination.  Last entry (index 14) is overwritten. */
    if (rank < HISCORE_ENTRIES - 1) {
        /* Number of entries to shift: (HISCORE_ENTRIES-1 - rank)  entries */
        int entries_to_shift = HISCORE_ENTRIES - 1 - rank;
        /* memmove: shift entries[rank .. rank+entries_to_shift-1] down by one */
        memmove(&table[rank + 1], &table[rank],
                entries_to_shift * sizeof(HiscoreEntry)); /* HISCORE.ASM:249-252 @@insert loop */
        /* HISCORE.ASM:253  mov score_15.score_end, 0 — last entry sentinel cleared.
         * (After memmove the old entry 14 data is now at 14+1 which is out of
         * bounds, so we clear the sentinel of the new last entry [14].) */
        table[HISCORE_ENTRIES - 1].end = 0;
    }

    /* HISCORE.ASM:256-274  write new entry at rank */
    slot = &table[rank];

    slot->value = score;  /* HISCORE.ASM:258 push/pop score_value */

    /* Format score_text: 6-digit zero-padded score + 2 trailing spaces.
     * HISCORE.ASM:261-266  copies 3 words from player_1.player_score_txt.
     * player_score_txt is maintained as a formatted ASCII string in the ASM;
     * we replicate the format here. */
    {
        char buf[9];
        int s = score;
        if (s < 0) s = 0;
        if (s > 999999) s = 999999;
        /* "NNNNNN  " — 6 digits, 2 spaces, no NUL in the field */
        buf[0] = '0' + (s / 100000) % 10;
        buf[1] = '0' + (s /  10000) % 10;
        buf[2] = '0' + (s /   1000) % 10;
        buf[3] = '0' + (s /    100) % 10;
        buf[4] = '0' + (s /     10) % 10;
        buf[5] = '0' + (s         ) % 10;
        buf[6] = ' ';
        buf[7] = ' ';
        memcpy(slot->score_text, buf, 8);  /* HISCORE.ASM:261-266 */
    }

    /* Format level_text: 2-digit zero-padded level + 2 trailing spaces.
     * HISCORE.ASM:267-268  mov ax,W level_txt+1; mov W[esi.score_level],ax
     * level_txt is a 4-char ASCII field; only 2 chars (one word) are copied. */
    {
        char buf[5];
        int lv = level;
        if (lv < 0)  lv = 0;
        if (lv > 99) lv = 99;
        buf[0] = '0' + (lv / 10) % 10;
        buf[1] = '0' + (lv     ) % 10;
        buf[2] = ' ';
        buf[3] = ' ';
        memcpy(slot->level_text, buf, 4);  /* HISCORE.ASM:267-268 */
    }

    /* HISCORE.ASM:270-272  lea eax,[esi.score_name]; mov name_adrs,eax
     * In the ASM, name entry is done interactively by get_name() after insert.
     * We accept the name as a parameter and copy it directly.
     * name_size = 15 (HISCORE.ASM:477); pad with spaces like the get_name loop. */
    {
        int n;
        int len = (int)strlen(name);
        if (len > HISCORE_NAME_LEN) len = HISCORE_NAME_LEN;
        for (n = 0; n < HISCORE_NAME_LEN; n++) {
            slot->name[n] = (n < len) ? name[n] : ' ';
        }
    }

    /* Preserve the sentinel byte: all entries except the last keep 13 (CR).
     * The last entry (index 14) keeps 0.
     * HISCORE.ASM:487-501  initial values; HISCORE.ASM:253 clears last entry. */
    slot->end = (rank < HISCORE_ENTRIES - 1) ? 13 : 0;

    return rank;
}
