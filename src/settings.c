/* settings.c — Blaster.cfg parser + Blaster.usr persistence.
 *
 * Mirrors FILE.ASM:925-1013 (Read_File_Config) and FILE.ASM:794-832
 * (Read_Config_User / Write_Config_User).
 */

#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* The 24 Freq_Option_* keys in ASM spawn order (MAIN.ASM:7057+ struc_options).
 * Matches PowerupType enum 0..23 in powerup.h. */
static const char *FREQ_KEYS[24] = {
    "freq_option_3_ball",
    "freq_option_6_ball",
    "freq_option_9_ball",
    "freq_option_20_ball",
    "freq_option_death",
    "freq_option_new_life",
    "freq_option_shoot",
    "freq_option_slow_ball",
    "freq_option_fast_ball",
    "freq_option_iron_ball",
    "freq_option_telepod",
    "freq_option_night",
    "freq_option_small_ship",
    "freq_option_large_ship",
    "freq_option_reverse",
    "freq_option_next_level",
    "freq_option_del_monster",
    "freq_option_magnetic",
    "freq_option_add_monster",
    "freq_option_ghost",
    "freq_option_bonus",
    "freq_option_malus",
    "freq_option_mini_shoot",
    "freq_option_collision",
};

/* ASCII lowercase — reused in key compare. */
static int ci_equal(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (char)tolower((unsigned char)*a);
        char cb = (char)tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

/* Trim trailing whitespace in-place. */
static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[--n] = 0;
    }
}

/* Parse the value-list payload inside `(...)`. Fills out[] (up to max entries)
 * and returns the count actually parsed. Accepts comma or whitespace separators.
 * FILE.ASM:915-922 Get_Expression skips until '(' — we do the same. */
static int parse_values(const char *src, int *out, int max) {
    const char *p = src;
    /* find '(' */
    while (*p && *p != '(') p++;
    if (*p != '(') return 0;
    p++;
    int count = 0;
    while (*p && *p != ')' && count < max) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == ')' || *p == 0) break;
        /* read signed int */
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p) { p++; continue; }
        out[count++] = (int)v;
        p = end;
    }
    return count;
}

void settings_defaults(GameConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    /* FILE.ASM:1127-1143 compiled-in defaults. */
    cfg->bonus_extra_life            = 10000;
    cfg->mouse_speed                 = 0;
    cfg->nbs_ball_start              = 2;     /* FILE.ASM:1137 nbs_ball_start dd 2 */
    cfg->start_level                 = 1;
    cfg->speed_delai                 = 1500;

    cfg->delai_between_option[0]     = 60;
    cfg->delai_between_option[1]     = 60;
    cfg->delai_between_option[2]     = 60;

    cfg->monster_delai[0]            = 600;
    cfg->monster_delai[1]            = 500;
    cfg->monster_delai[2]            = 300;

    cfg->speed_start[0]              = 2;
    cfg->speed_start[1]              = 3;
    cfg->speed_start[2]              = 4;

    cfg->change_speed_level[0]       = 3;
    cfg->change_speed_level[1]       = 4;
    cfg->change_speed_level[2]       = 4;

    /* Freq_Option_* defaults mirror Blaster.cfg:16-39 shipping values. */
    static const int DEFAULTS[24][3] = {
        { 2, 3, 4}, { 3, 4, 5}, { 4, 5, 6}, { 8, 9,10}, /* 3/6/9/20 ball */
        { 2, 3, 4}, { 8, 9,10},                          /* death, new_life */
        { 1, 3, 4}, { 2, 3, 4}, { 2, 3, 4}, { 5, 5, 6}, /* shoot, slow, fast, iron */
        { 2, 3, 4}, { 5, 6, 7},                          /* telepod, night */
        { 2, 3, 4}, { 2, 3, 4}, { 2, 3, 4}, { 9,10,11}, /* small, large, reverse, next */
        { 2, 3, 4}, { 2, 3, 4}, { 2, 3, 4}, { 3, 4, 5}, /* del_monster, magnetic, add_monster, ghost */
        { 2, 3, 4}, { 2, 3, 4}, { 2, 3, 4}, { 2, 2, 2}, /* bonus, malus, mini_shoot, collision */
    };
    for (int i = 0; i < 24; i++) {
        cfg->freq_option[i][0] = DEFAULTS[i][0];
        cfg->freq_option[i][1] = DEFAULTS[i][1];
        cfg->freq_option[i][2] = DEFAULTS[i][2];
    }
}

/* Return 1 and split `line` into key/payload if it looks like
 *   "<key> : (<...>)"   Anything before ':' is the key (trimmed).
 *   Anything after is the payload including the `(...)`.
 * Comments (';' onwards) are stripped first. */
static int split_kv(char *line, char **out_key, char **out_val) {
    /* Strip comments */
    char *semi = strchr(line, ';');
    if (semi) *semi = 0;

    char *colon = strchr(line, ':');
    if (!colon) return 0;
    *colon = 0;
    char *key = line;
    while (*key && isspace((unsigned char)*key)) key++;
    rtrim(key);
    if (*key == 0) return 0;

    char *val = colon + 1;
    while (*val && isspace((unsigned char)*val)) val++;
    if (*val != '(') return 0;  /* must start with paren */

    *out_key = key;
    *out_val = val;
    return 1;
}

int settings_load_cfg(GameConfig *cfg, const char *path) {
    if (!cfg || !path) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *key = NULL, *val = NULL;
        if (!split_kv(line, &key, &val)) continue;

        int vals[3] = {0, 0, 0};
        int n = parse_values(val, vals, 3);
        if (n <= 0) continue;

        /* Single-value globals */
        if (ci_equal(key, "extra_life"))             { cfg->bonus_extra_life = vals[0]; continue; }
        if (ci_equal(key, "mouse_speed"))            { cfg->mouse_speed      = vals[0]; continue; }
        /* ASM dec edx after read (FILE.ASM:950): stored value = Nbs_Life - 1. */
        if (ci_equal(key, "nbs_life"))               { cfg->nbs_ball_start   = vals[0] - 1; continue; }
        if (ci_equal(key, "start_level"))            { cfg->start_level      = vals[0]; continue; }
        if (ci_equal(key, "freq_change_speed"))      { cfg->speed_delai      = vals[0]; continue; }

        /* 3-value per-difficulty tuples */
        if (ci_equal(key, "time_between_option")) {
            if (n >= 3) {
                cfg->delai_between_option[0] = vals[0];
                cfg->delai_between_option[1] = vals[1];
                cfg->delai_between_option[2] = vals[2];
            }
            continue;
        }
        if (ci_equal(key, "freq_create_monster")) {
            if (n >= 3) {
                cfg->monster_delai[0] = vals[0];
                cfg->monster_delai[1] = vals[1];
                cfg->monster_delai[2] = vals[2];
            }
            continue;
        }
        if (ci_equal(key, "start_speed")) {
            if (n >= 3) {
                cfg->speed_start[0] = vals[0];
                cfg->speed_start[1] = vals[1];
                cfg->speed_start[2] = vals[2];
            }
            continue;
        }
        if (ci_equal(key, "nbs_change_speed_level")) {
            if (n >= 3) {
                cfg->change_speed_level[0] = vals[0];
                cfg->change_speed_level[1] = vals[1];
                cfg->change_speed_level[2] = vals[2];
            }
            continue;
        }

        /* Freq_Option_* */
        for (int i = 0; i < 24; i++) {
            if (ci_equal(key, FREQ_KEYS[i])) {
                if (n >= 3) {
                    cfg->freq_option[i][0] = vals[0];
                    cfg->freq_option[i][1] = vals[1];
                    cfg->freq_option[i][2] = vals[2];
                }
                break;
            }
        }
    }

    fclose(f);
    return 1;
}

/* Iter 2 fix #11: .usr file is a 2-BYTE blob in ASM, not 2 floats.
 * FILE.ASM:813-817:
 *     user_cfg        db ?
 *     User_Volume     db 32      ; range [0, 64]
 *     User_Volume_Sfx db 64      ; range [0, 64]
 *     user_cfg_len    = $ - user_cfg
 * Each byte is a volume level 0..64. We expose the value as a float in
 * [0,1] to callers by dividing by 64 and multiplying back on write. */
int settings_load_usr(float *volume, float *volume_sfx, const char *path) {
    if (!volume || !volume_sfx || !path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char bytes[2] = { 0, 0 };
    size_t n = fread(bytes, sizeof(unsigned char), 2, f);
    fclose(f);
    if (n != 2) return 0;
    /* Clamp to [0,64] (ASM range). */
    unsigned char b0 = bytes[0]; if (b0 > 64) b0 = 64;
    unsigned char b1 = bytes[1]; if (b1 > 64) b1 = 64;
    *volume     = (float)b0 / 64.0f;
    *volume_sfx = (float)b1 / 64.0f;
    return 1;
}

int settings_save_usr(float volume, float volume_sfx, const char *path) {
    if (!path) return 0;
    /* Convert [0,1] float → [0,64] byte. Clamp on the way in. */
    if (volume     < 0.0f) volume     = 0.0f;
    if (volume     > 1.0f) volume     = 1.0f;
    if (volume_sfx < 0.0f) volume_sfx = 0.0f;
    if (volume_sfx > 1.0f) volume_sfx = 1.0f;
    unsigned char bytes[2] = {
        (unsigned char)(volume     * 64.0f),
        (unsigned char)(volume_sfx * 64.0f)
    };
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t n = fwrite(bytes, sizeof(unsigned char), 2, f);
    fclose(f);
    return (n == 2) ? 1 : 0;
}
