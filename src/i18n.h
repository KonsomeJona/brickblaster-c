#ifndef I18N_H
#define I18N_H

/* i18n.h — Lightweight internationalisation for BrickBlaster UI.
 *
 * Supports: EN, FR, DE, ES, IT, PT.
 * JA/KO fall back to EN (default font lacks CJK glyphs).
 *
 * Usage:  i18n_init();  // detect device language
 *         const char *s = i18n(STR_PLAY);
 *
 * All strings are plain ASCII — no accented characters — so they render
 * correctly with raylib's built-in default font.
 */

typedef enum {
    /* Main menu */
    STR_PLAY, STR_SCORES, STR_LEVEL_EDITOR, STR_SETTINGS, STR_SUPPORT,
    /* Settings labels */
    STR_MUSIC, STR_SFX, STR_ON, STR_OFF,
    STR_PAD_SPEED, STR_DRAG, STR_TILT, STR_TILT_SPEED,
    STR_VERY_LOW, STR_LOW, STR_MEDIUM, STR_HIGH, STR_VERY_HIGH,
    STR_BACK, STR_FULLSCREEN,
    /* Settings descriptions */
    STR_DESC_PAD_SPEED, STR_DESC_DRAG, STR_DESC_TILT, STR_DESC_TILT_SPEED,
    /* World / Difficulty */
    STR_SELECT_WORLD, STR_SPACE, STR_ECLIPSE,
    STR_SELECT_DIFFICULTY, STR_EASY, STR_NORMAL, STR_HARD,
    /* Game overlays */
    STR_READY, STR_GAME_PAUSED, STR_GAME_OVER, STR_DEMO_LABEL,
    STR_FIRE, STR_RESUME, STR_EXIT,
    /* Original-style 18-char padded overlay strings (rendered with FONTE).
     * Match Blaster_en.cfg option_text_* — pre-centred inside the 270 px
     * panel_info (18 chars × 15 px). */
    STR_OPT_READY, STR_OPT_PAUSED, STR_OPT_GAME_OVER, STR_OPT_DEMO,
    STR_OPT_PLAY_AGAIN,
    /* Dual-mode winner banners — ASM has no cfg entry (dual added in C).
     * 18 chars each, FONTE lowercase. */
    STR_OPT_P1_WINS, STR_OPT_P2_WINS, STR_OPT_DRAW,
    /* 24 powerup labels — Blaster_en.cfg:56-79 Option_*. 18 chars each.
     * Render pipeline wiring deferred (fix J owns `current_option`
     * mirror). */
    STR_OPT_POW_BALL_3, STR_OPT_POW_BALL_6, STR_OPT_POW_BALL_9,
    STR_OPT_POW_BALL_20, STR_OPT_POW_DEATH, STR_OPT_POW_NEW_LIFE,
    STR_OPT_POW_SHOOT, STR_OPT_POW_SLOW_BALL, STR_OPT_POW_FAST_BALL,
    STR_OPT_POW_IRON_BALL, STR_OPT_POW_TELEPOD, STR_OPT_POW_NIGHT,
    STR_OPT_POW_SMALL_SHIP, STR_OPT_POW_LARGE_SHIP, STR_OPT_POW_REVERSE,
    STR_OPT_POW_NEXT_LEVEL, STR_OPT_POW_DEL_MONSTER, STR_OPT_POW_MAGNETIC,
    STR_OPT_POW_ADD_MONSTER, STR_OPT_POW_GHOST, STR_OPT_POW_BONUS,
    STR_OPT_POW_MALUS, STR_OPT_POW_MINI_SHOOT, STR_OPT_POW_COLLISION,
    /* Hints */
    STR_TAP_TO_START, STR_PRESS_A_START,
    STR_P_ESC_RESUME, STR_START_A_RESUME_B_EXIT,
    /* Pause overlay */
    STR_PAUSE_DRAG_HINT, STR_PAUSE_TILT_HINT,
    /* Original-style menu labels (lowercase, rendered with FONTE bitmap font).
     * Match Blaster_en.cfg menu_text_1a..7d. */
    STR_M_PLAY, STR_M_DEMO, STR_M_MISC, STR_M_QUIT,
    STR_M_ONE_PLAYER, STR_M_COOP, STR_M_DUAL, STR_M_CANCEL,
    STR_M_COMPUTER, STR_M_KEYBOARD, STR_M_JOYSTICK,
    STR_M_SPACE, STR_M_ARCADE,
    STR_M_EASY, STR_M_MEDIUM, STR_M_HARD,
    STR_M_HISCORES, STR_M_HISCORES_COOP, STR_M_CREDITS, STR_M_EDITOR,
    STR_M_ANALOGIC, STR_M_NUMERIC, STR_M_BLANK,
    /* Titles for each of the 7 menu screens (context line). */
    STR_M_TITLE_1, STR_M_TITLE_2, STR_M_TITLE_3, STR_M_TITLE_4,
    STR_M_TITLE_5, STR_M_TITLE_6, STR_M_TITLE_7,
    /* Count — must be last */
    STR_COUNT
} StringId;

typedef enum {
    LANG_EN = 0, LANG_FR, LANG_DE, LANG_ES, LANG_IT, LANG_PT,
    LANG_COUNT
} Language;

/* Detect device language and set it. Call once at startup. */
void i18n_init(void);

/* Detect language on Android via JNI. Call after wear_input_init(). */
void i18n_detect_android(void *android_app_ptr);

/* Get / set current language. */
Language i18n_get_language(void);
void i18n_set_language(Language lang);

/* Get localised string. Never returns NULL. */
const char *i18n(StringId id);

/* Draw text auto-sized to fit within max_width pixels.
 * Returns the font size used. Tries max_fs first, shrinks to min_fs.
 * Text is centered horizontally at cx, vertically at cy. */
int i18n_draw_fit(const char *text, int cx, int cy, int max_width,
                  int max_fs, int min_fs, unsigned int color_rgba);

#endif /* I18N_H */
