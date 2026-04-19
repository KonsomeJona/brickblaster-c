#pragma once
/* settings.h — Runtime-loaded game configuration.
 *
 * Mirrors the ASM `Read_File_Config` parser (FILE.ASM:925-1013) and the
 * `Read/Write_Config_User` persistence pair (FILE.ASM:794-832).
 *
 *  - blaster_asm.cfg: text file with `<key> : (<value[,value,value]>)` lines.
 *                     Single values apply globally; 3-value tuples are
 *                     easy/medium/hard per-difficulty overrides.
 *                     The ASM parser is whitespace-tolerant and skips any
 *                     `;` comment suffix.  We use `blaster_asm.cfg` to
 *                     keep the namespace distinct from our BBCF
 *                     `data/blaster.cfg` binary settings file.
 *
 *  - blaster.usr:     2-byte binary blob (FILE.ASM:813-817).
 *                     Each byte is a 0..64 volume level. We expose [0..1]
 *                     floats to callers and round-trip through bytes at
 *                     the fread/fwrite boundary (iter 2 fix #11).
 *
 * All functions return 1 on success, 0 on parse/read failure.  On failure
 * the target struct/floats are left untouched so callers can keep the
 * compiled-in defaults.
 */

/* ============================================================================
 * GameConfig — populated by settings_load_cfg().
 *
 * Mirrors the ASM data section at FILE.ASM:1127-1143.  Per-difficulty
 * arrays are indexed 0=easy, 1=medium, 2=hard (matches ASM _easy/_medium/_hard).
 * ============================================================================ */
typedef struct {
    int bonus_extra_life;           /* Extra_Life.            FILE.ASM:942, 1143 */
    int mouse_speed;                /* Mouse_Speed.           FILE.ASM:946, 1142 */
    int nbs_ball_start;             /* Nbs_Life - 1 (ASM dec). FILE.ASM:951, 1137 */
    int start_level;                /* Start_Level.           FILE.ASM:955, 1138 */
    int speed_delai;                /* Freq_Change_Speed.     FILE.ASM:959, 1133 */

    int delai_between_option[3];    /* Time_Between_Option.   FILE.ASM:977-981, 1139-1141 */
    int freq_option[24][3];         /* Freq_Option_* × 24.    FILE.ASM:962-973 */

    int monster_delai[3];           /* Freq_Create_Monster.   FILE.ASM:985-989, 1130-1132 */
    int speed_start[3];             /* Start_Speed.           FILE.ASM:993-997, 1127-1129 */
    int change_speed_level[3];      /* Nbs_Change_Speed_Level. FILE.ASM:1001-1005, 1134-1136 */
} GameConfig;

/* Fill `cfg` with compiled-in defaults matching FILE.ASM:1127-1143. */
void settings_defaults(GameConfig *cfg);

/* Parse Blaster.cfg at `path`. On error the struct is left unchanged.
 * Accepts lines of the form:
 *    key : (v)            ; comment
 *    key : (v1,v2,v3)     ; comment
 * Whitespace-tolerant, case-insensitive on keys.
 * Returns 1 on success, 0 if file missing or unreadable. */
int settings_load_cfg(GameConfig *cfg, const char *path);

/* Load persisted volume from .usr (2 floats, little-endian).
 * Returns 1 on success, 0 on missing/bad file. */
int settings_load_usr(float *volume, float *volume_sfx, const char *path);

/* Save current volume to .usr (2 floats).  Returns 1 on success. */
int settings_save_usr(float volume, float volume_sfx, const char *path);
