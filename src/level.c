#include "level.h"
#include "assets.h"   /* ASSETS_BASE — same path convention as game_load_level */
#include <string.h>
#include <stdio.h>    /* snprintf (level_count path building) */
#include <stdlib.h>
#if defined(PLATFORM_ANDROID)
    #include "raylib.h"
#elif defined(_WIN32)
    #include <direct.h>
    #define PORTABLE_MKDIR(p) _mkdir(p)
#else
    #include <sys/stat.h>
    #define PORTABLE_MKDIR(p) mkdir((p), 0755)
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

/* Expected total file size — FILE.ASM:1205 "cmp ecx,31200" (LEVEL_FILE_SIZE,
 * level.h). */

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
    /* A short read would leave the level table half-filled with garbage and
     * silently change level_count(), so treat it as a load failure. */
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
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

static void fill_level(Level *lvl, const unsigned char *src) {
    int i;

    memcpy(lvl->bricks, src, BRICK_COUNT);

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
}

int level_load(Level *lvl, const char *path, int level_num) {
    int data_size = 0;
    unsigned char *data;

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
    fill_level(lvl, data + (long)(level_num - 1) * BRICK_COUNT);
    free_file_data(data);
    return 0;
}

/* -----------------------------------------------------------------------
 * Worlds and edited copies — see level.h.
 * ----------------------------------------------------------------------- */
static char s_user_dir[256] = "data/";
static int  s_count_cache[LEVEL_WORLDS] = { -1, -1, -1, -1 };

void level_count_invalidate(void) {
    int i;
    for (i = 0; i < LEVEL_WORLDS; i++) s_count_cache[i] = -1;
}

void level_set_user_dir(const char *dir) {
    snprintf(s_user_dir, sizeof(s_user_dir), "%s", dir ? dir : "");
    level_count_invalidate();
}

const char *level_user_dir(void) { return s_user_dir; }

void level_user_path(int world, char *out, int n) {
    snprintf(out, (size_t)n, "%scustom.lv%d", s_user_dir, world);
}

/* Whole file, or NULL when missing or not exactly LEVEL_FILE_SIZE bytes. */
static unsigned char *read_world_file(const char *path) {
    int size = 0;
    unsigned char *data = read_file_data(path, &size);
    if (data && size != LEVEL_FILE_SIZE) {
        free_file_data(data);
        data = NULL;
    }
    return data;
}

int level_read_world(int world, unsigned char *buf, int allow_user) {
    char path[300];
    unsigned char *data = NULL;

    if (!buf || world < 0 || world >= LEVEL_WORLDS) return -1;
    if (allow_user) {
        level_user_path(world, path, sizeof(path));
        data = read_world_file(path);
    }
    if (!data) {
        /* Capitalised filename first, lowercase fallback (blaster.lv2). */
        snprintf(path, sizeof(path), ASSETS_BASE "levels/Blaster.lv%d", world);
        data = read_world_file(path);
    }
    if (!data) {
        snprintf(path, sizeof(path), ASSETS_BASE "levels/blaster.lv%d", world);
        data = read_world_file(path);
    }
    if (!data) return -1;
    memcpy(buf, data, LEVEL_FILE_SIZE);
    free_file_data(data);
    return 0;
}

int level_write_user_world(int world, const unsigned char *buf) {
    char path[300];
    int rc = -1;

    if (!buf || world < 0 || world >= LEVEL_WORLDS) return -1;
    level_user_path(world, path, sizeof(path));
#if defined(PLATFORM_ANDROID)
    rc = SaveFileData(path, (void *)buf, LEVEL_FILE_SIZE) ? 0 : -1;
#else
    {
        /* The directory may not exist yet on a fresh install: without it
         * fopen fails and the edit is silently lost. */
        char dir[256];
        size_t len;
        FILE *f;
        snprintf(dir, sizeof(dir), "%s", s_user_dir);
        len = strlen(dir);
        if (len > 1 && (dir[len - 1] == '/' || dir[len - 1] == '\\')) dir[len - 1] = '\0';
        if (dir[0]) PORTABLE_MKDIR(dir);   /* already there: harmless EEXIST */
        f = fopen(path, "wb");
        if (f) {
            if (fwrite(buf, 1, LEVEL_FILE_SIZE, f) == LEVEL_FILE_SIZE) rc = 0;
            if (fclose(f) != 0) rc = -1;
        }
    }
#endif
    level_count_invalidate();
    return rc;
}

int level_remove_user_world(int world) {
    char path[300];
    FILE *f;

    if (world < 0 || world >= LEVEL_WORLDS) return -1;
    level_user_path(world, path, sizeof(path));
    level_count_invalidate();
#if defined(PLATFORM_ANDROID)
    /* Relative paths resolve inside internal storage only through raylib's
     * file wrappers; there is no remove wrapper. The caller keeps the copy. */
    (void)f;
    return -1;
#else
    f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return remove(path) == 0 ? 0 : -1;
#endif
}

int level_load_world(Level *lvl, int world, int level_num) {
    unsigned char *buf;

    if (!lvl || level_num < 1 || level_num > LEVELS_PER_FILE) return -1;
    buf = (unsigned char *)malloc(LEVEL_FILE_SIZE);
    if (!buf) return -1;
    if (level_read_world(world, buf, 1) != 0) {
        free(buf);
        return -1;
    }
    fill_level(lvl, buf + (long)(level_num - 1) * BRICK_COUNT);
    lvl->world = world;
    free(buf);
    return 0;
}

void level_trim_world(unsigned char *buf) {
    int n = level_count_buffer(buf);
    while (n > 1) {
        unsigned char *lv = buf + (n - 1) * BRICK_COUNT;
        int i;
        for (i = 0; i < BRICK_COUNT && lv[i] == 0x00; i++) {}
        if (i < BRICK_COUNT) break;
        memset(lv, 0xFF, BRICK_COUNT);
        n--;
    }
}

unsigned char level_brush_code(int brush, int color) {
    /* EDITOR.ASM:223-266 F1-F5 handlers (F2 = normale + 4 hits, P1-ASM-25). */
    static const unsigned char code[5] = { 0x21, 0x24, 0x08, 0x11, 0x18 };
    if (brush < 0 || brush > 4) brush = 0;
    return (unsigned char)(code[brush] | ((color & 3) << 6));
}

int level_world_bg_set(int world) {
    return (world == WORLD_ATOLL) ? 2 : (world & 1);
}

int level_world_palette(int world) {
    return (world == WORLD_ATOLL) ? 0 : (world & 1);
}

/* -----------------------------------------------------------------------
 * level_count — number of PLAYABLE levels in a world file.
 * MAIN.ASM:5025-5041  search_level_number:
 *     mov esi,level_adrs
 *     mov ecx,level_size
 *     mov level_number,0
 *   @@again:
 *     cmp B [esi],-1                      ; block starts with 0xFF ?
 *     je @@end                            ; yes → done
 *     inc level_number
 *     add esi,nbs_brique_x*nbs_brique_y   ; next 390-byte block
 *     sub ecx,nbs_brique_x*nbs_brique_y
 *     cmp ecx,0
 *     ja @@again
 *     jmp Error_File                      ; no sentinel found = corrupt file
 *
 * Counts 390-byte blocks until the first one whose FIRST byte is 0xFF
 * (invalide).  Measured on the 1999 files: each .lv? is 31,200 bytes with
 * the last 15,600 bytes all 0xFF — 40 playable levels + 40 empty slots.
 *
 * Divergence from ASM: a file with no 0xFF sentinel aborts the DOS game
 * (Error_File); here we return the full capacity instead — all levels are
 * valid, refusing to play them helps nobody.  Missing/short file → 0.
 * ----------------------------------------------------------------------- */
int level_count_buffer(const unsigned char *buf) {
    int n = 0, off;
    for (off = 0; off + BRICK_COUNT <= LEVEL_FILE_SIZE; off += BRICK_COUNT) {
        if (buf[off] == 0xFF) break;   /* cmp B [esi],-1 / je @@end */
        n++;
    }
    return n;
}

int level_count(int world) {
    unsigned char *buf;
    int n = 0;

    if (world < 0 || world >= LEVEL_WORLDS) return 0;
    if (s_count_cache[world] >= 0) return s_count_cache[world];

    /* Same resolution as game_load_level: edited copy, then the shipped
     * file (capitalised name, lowercase fallback). */
    buf = (unsigned char *)malloc(LEVEL_FILE_SIZE);
    if (!buf) return 0;
    if (level_read_world(world, buf, 1) == 0) n = level_count_buffer(buf);
    free(buf);
    s_count_cache[world] = n;
    return n;
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
