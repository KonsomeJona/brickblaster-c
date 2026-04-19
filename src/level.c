#include "level.h"
#include <string.h>
#if defined(PLATFORM_ANDROID)
    #include "raylib.h"
#else
    #include <stdio.h>
    #include <stdlib.h>
#endif

/* level.c - Level file loading from FILE.ASM
 *
 * FILE.ASM:1184  Read_Level:
 *   1. Build filename from world byte: 'Blaster.lv0', 'Blaster.lv1', 'blaster.lv2'
 *   2. Load entire file into memory with Load_External_File_Handle
 *   3. Validate: cmp ecx,31200  — file MUST be exactly 31,200 bytes
 *   4. Store level_adrs, level_size, level_handle
 *
 * MAIN.ASM:1711-1714  Level N address in the loaded file:
 *   mov eax,current_level
 *   dec eax
 *   imul eax,nbs_brique_x*nbs_brique_y    ; = BRICK_COUNT = 390
 *   add eax,level_adrs
 *
 * No header bytes.  Levels are stored row-major, 390 bytes each, sequentially.
 * Brick byte layout: [CC|TTT|HHHHH]  (Blaster.inc:390-406)
 *   Bits 7-6: Color   (mask 0xC0)
 *   Bits 5-3: Type    (mask 0x38)  0=absent, 0x20=normale, 0x10=transparent,
 *                                   0x08=incassable, 0x18=teleporteuse
 *   Bits 4-0: HP      (mask 0x1F)
 *   0x00 = ABSENTE (empty), 0xFF = INVALIDE
 */

/* Expected total file size — FILE.ASM:1205 "cmp ecx,31200" */
#define LEVEL_FILE_SIZE  31200

/* -----------------------------------------------------------------------
 * level_load
 * Opens `path`, seeks to (level_num-1)*BRICK_COUNT bytes, reads 390 bytes.
 * Returns 0 on success, -1 on error.
 * ----------------------------------------------------------------------- */
/* Platform-specific file read: load entire file into a malloc'd buffer.
 * On Android, assets are inside the APK and only accessible via AAssetManager
 * (raylib's LoadFileData wraps this). On other platforms, use fopen. */
static unsigned char *read_file_data(const char *path, int *out_size) {
#if defined(PLATFORM_ANDROID)
    return LoadFileData(path, out_size);
#else
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long sz;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)sz, f);
    fclose(f);
    *out_size = (int)sz;
    return buf;
#endif
}

static void free_file_data(unsigned char *data) {
#if defined(PLATFORM_ANDROID)
    UnloadFileData(data);
#else
    free(data);
#endif
}

int level_load(Level *lvl, const char *path, int level_num) {
    int data_size = 0;
    unsigned char *data;
    long offset;
    int i;

    if (!lvl || !path) return -1;
    if (level_num < 1 || level_num > LEVELS_PER_FILE) return -1;

    /* FILE.ASM:1205 — file MUST be exactly 31200 bytes. */
    data = read_file_data(path, &data_size);
    if (!data) return -1;

    if (data_size != LEVEL_FILE_SIZE) {
        free_file_data(data);
        return -1;
    }

    /* Seek to level N: offset = (N-1) * BRICK_COUNT  (MAIN.ASM:1711-1714) */
    offset = (long)(level_num - 1) * BRICK_COUNT;
    memcpy(lvl->bricks, data + offset, BRICK_COUNT);
    free_file_data(data);

    /* Fill metadata */
    lvl->cols  = BRICK_COLS;
    lvl->rows  = BRICK_ROWS;
    lvl->world = -1; /* caller may set; path encodes world number */

    /* Count non-empty, non-invalid bricks
     * ABSENTE=0x00, INVALIDE=0xFF — both treated as no brick */
    lvl->brick_count = 0;
    for (i = 0; i < BRICK_COUNT; i++) {
        unsigned char b = lvl->bricks[i];
        if (b != 0x00 && b != 0xFF) {
            lvl->brick_count++;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------
 * Grid helpers
 * MAIN.ASM:1700-1715 and MAIN.ASM:4880-4898
 * ----------------------------------------------------------------------- */

/* Column of brick at linear index.
 * Equivalent to MAIN.ASM: x_offset / brique_size_x (shr by 5) then used
 * as col; here derived directly as index % BRICK_COLS. */
int level_brick_col(int index) {
    return index % BRICK_COLS;
}

/* Row of brick at linear index.
 * Equivalent to MAIN.ASM: y_offset / brique_size_y (shr by 4) then *13. */
int level_brick_row(int index) {
    return index / BRICK_COLS;
}

/* Screen X of brick at linear index.
 * MAIN.ASM:4882  "mov ebx,bord_x"  then add col*brique_size_x each step. */
int level_brick_x(int index) {
    return BRICK_ORIGIN_X + level_brick_col(index) * BRICK_W;
}

/* Screen Y of brick at linear index.
 * MAIN.ASM:4883  "mov edi,bord_y"  then add brique_size_y each row. */
int level_brick_y(int index) {
    return BRICK_ORIGIN_Y + level_brick_row(index) * BRICK_H;
}

/* -----------------------------------------------------------------------
 * Brick byte decode
 * Blaster.inc:390-406
 * ----------------------------------------------------------------------- */

/* Color index 0-3 from bits 7-6.
 * Blaster.inc:393  couleur_de_brique = 0C0h
 * Shift right 6 to get 0=green, 1=blue, 2=violet, 3=orange */
int level_brick_color(unsigned char b) {
    return (b & 0xC0) >> 6;
}

/* Non-zero if type bits 5-3 are non-zero (brick has a type, i.e. is present).
 * Blaster.inc:399  type_de_brique = 038h */
int level_brick_is_special(unsigned char b) {
    return (b & 0x38) != 0 ? 1 : 0;
}

/* Hit-points from bits 4-0.
 * Blaster.inc:391  resistance_de_brique = 01Fh */
int level_brick_hp(unsigned char b) {
    return b & 0x1F;
}
