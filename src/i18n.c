#include "i18n.h"
#include <raylib.h>
#include <string.h>
#include <stdlib.h>

static Language s_lang = LANG_EN;

/* =========================================================================
 * Translation table — all ASCII-safe (no accented chars).
 * Index: [language][string_id]
 * ========================================================================= */
static const char *s_strings[LANG_COUNT][STR_COUNT] = {

/* ---------- ENGLISH ---------- */
[LANG_EN] = {
    /* Main menu */
    [STR_PLAY]          = "PLAY",
    [STR_SCORES]        = "SCORES",
    [STR_LEVEL_EDITOR]  = "LEVEL EDITOR",
    [STR_SETTINGS]      = "SETTINGS",
    [STR_SUPPORT]       = "SUPPORT",
    /* Settings */
    [STR_MUSIC]         = "MUSIC",
    [STR_SFX]           = "SFX",
    [STR_ON]            = "ON",
    [STR_OFF]           = "OFF",
    [STR_PAD_SPEED]     = "PAD SPEED",
    [STR_DRAG]          = "DRAG",
    [STR_TILT]          = "TILT",
    [STR_TILT_SPEED]    = "TILT SPEED",
    [STR_VERY_LOW]      = "VERY LOW",
    [STR_LOW]           = "LOW",
    [STR_MEDIUM]        = "MEDIUM",
    [STR_HIGH]          = "HIGH",
    [STR_VERY_HIGH]     = "VERY HIGH",
    [STR_BACK]          = "BACK",
    [STR_FULLSCREEN]    = "FULLSCREEN",
    [STR_DESC_PAD_SPEED]  = "Left/right button responsiveness",
    [STR_DESC_DRAG]       = "Slide finger to move paddle",
    [STR_DESC_TILT]       = "Tilt device to move paddle",
    [STR_DESC_TILT_SPEED] = "Accelerometer sensitivity",
    /* World / Difficulty */
    [STR_SELECT_WORLD]  = "SELECT WORLD",
    [STR_SPACE]         = "SPACE",
    [STR_ECLIPSE]       = "ECLIPSE",
    [STR_SELECT_DIFFICULTY] = "SELECT DIFFICULTY",
    [STR_EASY]          = "EASY",
    [STR_NORMAL]        = "NORMAL",
    [STR_HARD]          = "HARD",
    /* Overlays */
    [STR_READY]         = "READY ?",
    [STR_GAME_PAUSED]   = "GAME PAUSED",
    [STR_GAME_OVER]     = "GAME OVER",
    [STR_DEMO_LABEL]    = "DEMO",
    [STR_FIRE]          = "FIRE",
    [STR_RESUME]        = "RESUME",
    [STR_EXIT]          = "EXIT",
    /* Hints */
    [STR_TAP_TO_START]  = "TAP TO START",
    [STR_PRESS_A_START] = "PRESS A TO START",
    [STR_P_ESC_RESUME]  = "P or ESC to resume",
    [STR_START_A_RESUME_B_EXIT] = "START or A to resume  /  B to exit",
    [STR_PAUSE_DRAG_HINT] = "Slide finger to move paddle",
    [STR_PAUSE_TILT_HINT] = "Tilt device to move paddle",
    /* Original-style lowercase menu labels (Blaster_en.cfg). */
    [STR_M_PLAY]          = "play",
    [STR_M_DEMO]          = "demo",
    [STR_M_MISC]          = "miscellaneous",
    [STR_M_QUIT]          = "quit",
    [STR_M_ONE_PLAYER]    = "one player",
    [STR_M_COOP]          = "two players coop",
    [STR_M_DUAL]          = "two players dual",
    [STR_M_CANCEL]        = "cancel",
    [STR_M_COMPUTER]      = "computer",
    [STR_M_KEYBOARD]      = "keyboard",
    [STR_M_JOYSTICK]      = "joystick",
    [STR_M_SPACE]         = "space",
    [STR_M_ARCADE]        = "arcade",
    [STR_M_EASY]          = "easy",
    [STR_M_MEDIUM]        = "medium",
    [STR_M_HARD]          = "hard",
    [STR_M_HISCORES]      = "high scores",
    [STR_M_HISCORES_COOP] = "high scores coop",
    [STR_M_CREDITS]       = "credits",
    [STR_M_EDITOR]        = "editor",
    [STR_M_ANALOGIC]      = "analogic",
    [STR_M_NUMERIC]       = "numeric",
    [STR_M_BLANK]         = "",
    [STR_M_TITLE_1]       = "main menu",
    [STR_M_TITLE_2]       = "players",
    [STR_M_TITLE_3]       = "ctrl player 2",
    [STR_M_TITLE_4]       = "select world",
    [STR_M_TITLE_5]       = "skill level",
    [STR_M_TITLE_6]       = "miscellaneous",
    [STR_M_TITLE_7]       = "joystick",
    /* Blaster_en.cfg option_text_* — 18 chars each, pre-centred. */
    [STR_OPT_READY]       = "     ready ?      ",
    [STR_OPT_PAUSED]      = "   game paused    ",
    [STR_OPT_GAME_OVER]   = "    game over     ",
    [STR_OPT_DEMO]        = "       demo       ",
    [STR_OPT_PLAY_AGAIN]  = "    play again    ",
    /* Dual-mode winner banners — 18 chars each. FONTE lowercase. */
    [STR_OPT_P1_WINS]     = "   player 1 wins  ",
    [STR_OPT_P2_WINS]     = "   player 2 wins  ",
    [STR_OPT_DRAW]        = "       draw       ",
    /* Blaster_en.cfg:56-79 Option_* — 24 powerup labels, 18 chars each. */
    [STR_OPT_POW_BALL_3]      = "      1 ball      ",
    [STR_OPT_POW_BALL_6]      = "     3 balls      ",
    [STR_OPT_POW_BALL_9]      = "     6 balls      ",
    [STR_OPT_POW_BALL_20]     = "     20 balls     ",
    [STR_OPT_POW_DEATH]       = "    lost life     ",
    [STR_OPT_POW_NEW_LIFE]    = "     new life     ",
    [STR_OPT_POW_SHOOT]       = "    big shoot     ",
    [STR_OPT_POW_SLOW_BALL]   = "       slow       ",
    [STR_OPT_POW_FAST_BALL]   = "       fast       ",
    [STR_OPT_POW_IRON_BALL]   = "    iron ball     ",
    [STR_OPT_POW_TELEPOD]     = "     telepod      ",
    [STR_OPT_POW_NIGHT]       = "     point x2     ",
    [STR_OPT_POW_SMALL_SHIP]  = "    small ship    ",
    [STR_OPT_POW_LARGE_SHIP]  = "    large ship    ",
    [STR_OPT_POW_REVERSE]     = "     reverse      ",
    [STR_OPT_POW_NEXT_LEVEL]  = "    next level    ",
    [STR_OPT_POW_DEL_MONSTER] = "   wrath of god   ",
    [STR_OPT_POW_MAGNETIC]    = "     magnetic     ",
    [STR_OPT_POW_ADD_MONSTER] = "   summon beast   ",
    [STR_OPT_POW_GHOST]       = "      ghost       ",
    [STR_OPT_POW_BONUS]       = "     +100 pts     ",
    [STR_OPT_POW_MALUS]       = "     -100 pts     ",
    [STR_OPT_POW_MINI_SHOOT]  = "      shoot       ",
    [STR_OPT_POW_COLLISION]   = "    collision     ",
},

/* ---------- FRENCH ---------- */
[LANG_FR] = {
    [STR_PLAY]          = "JOUER",
    [STR_SCORES]        = "SCORES",
    [STR_LEVEL_EDITOR]  = "EDITEUR",
    [STR_SETTINGS]      = "REGLAGES",
    [STR_SUPPORT]       = "SUPPORT",
    [STR_MUSIC]         = "MUSIQUE",
    [STR_SFX]           = "SFX",
    [STR_ON]            = "OUI",
    [STR_OFF]           = "NON",
    [STR_PAD_SPEED]     = "VITESSE PAD",
    [STR_DRAG]          = "GLISSER",
    [STR_TILT]          = "INCLINER",
    [STR_TILT_SPEED]    = "SENSIB. INCL.",
    [STR_VERY_LOW]      = "TRES LENT",
    [STR_LOW]           = "LENT",
    [STR_MEDIUM]        = "MOYEN",
    [STR_HIGH]          = "RAPIDE",
    [STR_VERY_HIGH]     = "TRES RAPIDE",
    [STR_BACK]          = "RETOUR",
    [STR_FULLSCREEN]    = "PLEIN ECRAN",
    [STR_DESC_PAD_SPEED]  = "Vitesse des boutons gauche/droite",
    [STR_DESC_DRAG]       = "Glisser le doigt pour deplacer",
    [STR_DESC_TILT]       = "Incliner pour deplacer la raquette",
    [STR_DESC_TILT_SPEED] = "Sensibilite de l'inclinaison",
    [STR_SELECT_WORLD]  = "CHOISIR MONDE",
    [STR_SPACE]         = "ESPACE",
    [STR_ECLIPSE]       = "ECLIPSE",
    [STR_SELECT_DIFFICULTY] = "DIFFICULTE",
    [STR_EASY]          = "FACILE",
    [STR_NORMAL]        = "NORMAL",
    [STR_HARD]          = "DIFFICILE",
    [STR_READY]         = "PRET ?",
    [STR_GAME_PAUSED]   = "PAUSE",
    [STR_GAME_OVER]     = "FIN DE PARTIE",
    [STR_DEMO_LABEL]    = "DEMO",
    [STR_FIRE]          = "TIR",
    [STR_RESUME]        = "REPRENDRE",
    [STR_EXIT]          = "QUITTER",
    [STR_TAP_TO_START]  = "APPUYER POUR JOUER",
    [STR_PRESS_A_START] = "APPUYER A POUR JOUER",
    [STR_P_ESC_RESUME]  = "P ou ECHAP pour reprendre",
    [STR_START_A_RESUME_B_EXIT] = "START ou A: reprendre / B: quitter",
    [STR_PAUSE_DRAG_HINT] = "Glisser le doigt pour deplacer",
    [STR_PAUSE_TILT_HINT] = "Incliner pour deplacer la raquette",
    /* Original-style lowercase menu labels (Blaster.cfg, ASCII only). */
    [STR_M_PLAY]          = "jouer",
    [STR_M_DEMO]          = "demo",
    [STR_M_MISC]          = "divers",
    [STR_M_QUIT]          = "quitter",
    [STR_M_ONE_PLAYER]    = "un joueur",
    /* Blaster.cfg:87-88 — byte-exact. */
    [STR_M_COOP]          = "2 joueurs cooperatifs",
    [STR_M_DUAL]          = "2 joueurs mode duel",
    [STR_M_CANCEL]        = "retour",
    [STR_M_COMPUTER]      = "ordinateur",
    [STR_M_KEYBOARD]      = "clavier",
    [STR_M_JOYSTICK]      = "joystick",
    [STR_M_SPACE]         = "espace",
    [STR_M_ARCADE]        = "arcade",
    [STR_M_EASY]          = "facile",
    [STR_M_MEDIUM]        = "moyen",
    [STR_M_HARD]          = "difficile",
    [STR_M_HISCORES]      = "meilleurs scores",
    /* Blaster.cfg:103 — byte-exact (trailing period). */
    [STR_M_HISCORES_COOP] = "meilleurs scores coop.",
    [STR_M_CREDITS]       = "credits",
    [STR_M_EDITOR]        = "editeur",
    [STR_M_ANALOGIC]      = "analogique",
    [STR_M_NUMERIC]       = "numerique",
    [STR_M_BLANK]         = "",
    [STR_M_TITLE_1]       = "menu principal",
    [STR_M_TITLE_2]       = "joueurs",
    /* Blaster.cfg:90 / :94 — byte-exact (spelled-out). */
    [STR_M_TITLE_3]       = "controle joueur 2",
    [STR_M_TITLE_4]       = "choix du monde",
    [STR_M_TITLE_5]       = "difficulte",
    [STR_M_TITLE_6]       = "divers",
    [STR_M_TITLE_7]       = "joystick",
    /* Blaster.cfg option_text_* (FR) — 18 chars each. */
    [STR_OPT_READY]       = "      pret ?      ",
    [STR_OPT_PAUSED]      = "       pause      ",
    [STR_OPT_GAME_OVER]   = "    game over     ",
    [STR_OPT_DEMO]        = "       demo       ",
    [STR_OPT_PLAY_AGAIN]  = "   joue encore    ",
    /* Dual-mode winner banners — FR (no ASM cfg entry, invented).
     * 18 chars each, FONTE lowercase. */
    [STR_OPT_P1_WINS]     = "   joueur 1 gagne ",
    [STR_OPT_P2_WINS]     = "   joueur 2 gagne ",
    [STR_OPT_DRAW]        = "      egalite     ",
    /* Blaster.cfg:56-79 Option_* — 24 powerup labels, 18 chars each. */
    [STR_OPT_POW_BALL_3]      = "      1 balle     ",
    [STR_OPT_POW_BALL_6]      = "     3 balles     ",
    [STR_OPT_POW_BALL_9]      = "     6 balles     ",
    [STR_OPT_POW_BALL_20]     = "     20 balles    ",
    [STR_OPT_POW_DEATH]       = "    vie perdue    ",
    [STR_OPT_POW_NEW_LIFE]    = "    vie en plus   ",
    [STR_OPT_POW_SHOOT]       = "      missile     ",
    [STR_OPT_POW_SLOW_BALL]   = "     plus lent    ",
    [STR_OPT_POW_FAST_BALL]   = "    plus rapide   ",
    [STR_OPT_POW_IRON_BALL]   = "  balle en metal  ",
    [STR_OPT_POW_TELEPOD]     = "    teleporteur   ",
    [STR_OPT_POW_NIGHT]       = "    points x2     ",
    [STR_OPT_POW_SMALL_SHIP]  = "  petit vaisseau  ",
    [STR_OPT_POW_LARGE_SHIP]  = "  grand vaisseau  ",
    [STR_OPT_POW_REVERSE]     = "     inversion    ",
    [STR_OPT_POW_NEXT_LEVEL]  = "  niveau suivant  ",
    [STR_OPT_POW_DEL_MONSTER] = "    apocalypse    ",
    [STR_OPT_POW_MAGNETIC]    = "      aimant      ",
    [STR_OPT_POW_ADD_MONSTER] = "invocation monstre",
    [STR_OPT_POW_GHOST]       = "     fantome      ",
    [STR_OPT_POW_BONUS]       = "     +100 pts     ",
    [STR_OPT_POW_MALUS]       = "     -100 pts     ",
    [STR_OPT_POW_MINI_SHOOT]  = "       tirs       ",
    [STR_OPT_POW_COLLISION]   = "    collision     ",
},

/* ---------- GERMAN ---------- */
[LANG_DE] = {
    [STR_PLAY]          = "SPIELEN",
    [STR_SCORES]        = "PUNKTE",
    [STR_LEVEL_EDITOR]  = "EDITOR",
    [STR_SETTINGS]      = "OPTIONEN",
    [STR_SUPPORT]       = "HILFE",
    [STR_MUSIC]         = "MUSIK",
    [STR_SFX]           = "SFX",
    [STR_ON]            = "AN",
    [STR_OFF]           = "AUS",
    [STR_PAD_SPEED]     = "TASTEN-TEMPO",
    [STR_DRAG]          = "WISCHEN",
    [STR_TILT]          = "NEIGEN",
    [STR_TILT_SPEED]    = "NEIG-TEMPO",
    [STR_VERY_LOW]      = "SEHR LANGSAM",
    [STR_LOW]           = "LANGSAM",
    [STR_MEDIUM]        = "MITTEL",
    [STR_HIGH]          = "SCHNELL",
    [STR_VERY_HIGH]     = "SEHR SCHNELL",
    [STR_BACK]          = "ZURUCK",
    [STR_FULLSCREEN]    = "VOLLBILD",
    [STR_DESC_PAD_SPEED]  = "Geschwindigkeit der L/R-Tasten",
    [STR_DESC_DRAG]       = "Mit dem Finger wischen",
    [STR_DESC_TILT]       = "Gerat neigen zum Bewegen",
    [STR_DESC_TILT_SPEED] = "Neigungsempfindlichkeit",
    [STR_SELECT_WORLD]  = "WELT WAHLEN",
    [STR_SPACE]         = "WELTRAUM",
    [STR_ECLIPSE]       = "ECLIPSE",
    [STR_SELECT_DIFFICULTY] = "SCHWIERIGKEIT",
    [STR_EASY]          = "LEICHT",
    [STR_NORMAL]        = "NORMAL",
    [STR_HARD]          = "SCHWER",
    [STR_READY]         = "BEREIT ?",
    [STR_GAME_PAUSED]   = "PAUSIERT",
    [STR_GAME_OVER]     = "GAME OVER",
    [STR_DEMO_LABEL]    = "DEMO",
    [STR_FIRE]          = "FEUER",
    [STR_RESUME]        = "WEITER",
    [STR_EXIT]          = "BEENDEN",
    [STR_TAP_TO_START]  = "ANTIPPEN ZUM STARTEN",
    [STR_PRESS_A_START] = "A DRUCKEN ZUM STARTEN",
    [STR_P_ESC_RESUME]  = "P oder ESC: weiter",
    [STR_START_A_RESUME_B_EXIT] = "START/A: weiter / B: beenden",
    [STR_PAUSE_DRAG_HINT] = "Finger wischen zum Bewegen",
    [STR_PAUSE_TILT_HINT] = "Gerat neigen zum Bewegen",
    /* Menu labels — DE. No original cfg, translations are best-effort.
     * FONTE charset: a-z 0-9 + - # ! ? : . & space (no umlauts). */
    [STR_M_PLAY]          = "spielen",
    [STR_M_DEMO]          = "demo",
    [STR_M_MISC]          = "sonstiges",
    [STR_M_QUIT]          = "beenden",
    [STR_M_ONE_PLAYER]    = "ein spieler",
    [STR_M_COOP]          = "2 spieler koop",
    [STR_M_DUAL]          = "2 spieler duell",
    [STR_M_CANCEL]        = "zuruck",
    [STR_M_COMPUTER]      = "computer",
    [STR_M_KEYBOARD]      = "tastatur",
    [STR_M_JOYSTICK]      = "joystick",
    [STR_M_SPACE]         = "weltraum",
    [STR_M_ARCADE]        = "arcade",
    [STR_M_EASY]          = "leicht",
    [STR_M_MEDIUM]        = "mittel",
    [STR_M_HARD]          = "schwer",
    [STR_M_HISCORES]      = "bestenliste",
    [STR_M_HISCORES_COOP] = "bestenliste koop",
    [STR_M_CREDITS]       = "credits",
    [STR_M_EDITOR]        = "editor",
    [STR_M_ANALOGIC]      = "analog",
    [STR_M_NUMERIC]       = "digital",
    [STR_M_BLANK]         = "",
    [STR_M_TITLE_1]       = "hauptmenu",
    [STR_M_TITLE_2]       = "spieler",
    [STR_M_TITLE_3]       = "steuerung spieler 2",
    [STR_M_TITLE_4]       = "welt wahlen",
    [STR_M_TITLE_5]       = "schwierigkeit",
    [STR_M_TITLE_6]       = "sonstiges",
    [STR_M_TITLE_7]       = "joystick",
    /* option_text_* (DE) — 18 chars each, FONTE lowercase. */
    [STR_OPT_READY]       = "      bereit      ",
    [STR_OPT_PAUSED]      = "     pausiert     ",
    [STR_OPT_GAME_OVER]   = "    game over     ",
    [STR_OPT_DEMO]        = "       demo       ",
    [STR_OPT_PLAY_AGAIN]  = "   noch einmal    ",
    /* Dual-mode winner banners — DE. 18 chars each. */
    [STR_OPT_P1_WINS]     = "   spieler 1 gew. ",
    [STR_OPT_P2_WINS]     = "   spieler 2 gew. ",
    [STR_OPT_DRAW]        = "   unentschieden  ",
    /* Powerup labels — DE falls back to EN (no ASM cfg). 18 chars each. */
    [STR_OPT_POW_BALL_3]      = "      1 ball      ",
    [STR_OPT_POW_BALL_6]      = "     3 balls      ",
    [STR_OPT_POW_BALL_9]      = "     6 balls      ",
    [STR_OPT_POW_BALL_20]     = "     20 balls     ",
    [STR_OPT_POW_DEATH]       = "    lost life     ",
    [STR_OPT_POW_NEW_LIFE]    = "     new life     ",
    [STR_OPT_POW_SHOOT]       = "    big shoot     ",
    [STR_OPT_POW_SLOW_BALL]   = "       slow       ",
    [STR_OPT_POW_FAST_BALL]   = "       fast       ",
    [STR_OPT_POW_IRON_BALL]   = "    iron ball     ",
    [STR_OPT_POW_TELEPOD]     = "     telepod      ",
    [STR_OPT_POW_NIGHT]       = "     point x2     ",
    [STR_OPT_POW_SMALL_SHIP]  = "    small ship    ",
    [STR_OPT_POW_LARGE_SHIP]  = "    large ship    ",
    [STR_OPT_POW_REVERSE]     = "     reverse      ",
    [STR_OPT_POW_NEXT_LEVEL]  = "    next level    ",
    [STR_OPT_POW_DEL_MONSTER] = "   wrath of god   ",
    [STR_OPT_POW_MAGNETIC]    = "     magnetic     ",
    [STR_OPT_POW_ADD_MONSTER] = "   summon beast   ",
    [STR_OPT_POW_GHOST]       = "      ghost       ",
    [STR_OPT_POW_BONUS]       = "     +100 pts     ",
    [STR_OPT_POW_MALUS]       = "     -100 pts     ",
    [STR_OPT_POW_MINI_SHOOT]  = "      shoot       ",
    [STR_OPT_POW_COLLISION]   = "    collision     ",
},

/* ---------- SPANISH ---------- */
[LANG_ES] = {
    [STR_PLAY]          = "JUGAR",
    [STR_SCORES]        = "PUNTOS",
    [STR_LEVEL_EDITOR]  = "EDITOR",
    [STR_SETTINGS]      = "AJUSTES",
    [STR_SUPPORT]       = "AYUDA",
    [STR_MUSIC]         = "MUSICA",
    [STR_SFX]           = "SFX",
    [STR_ON]            = "SI",
    [STR_OFF]           = "NO",
    [STR_PAD_SPEED]     = "VELOCIDAD PAD",
    [STR_DRAG]          = "ARRASTRAR",
    [STR_TILT]          = "INCLINAR",
    [STR_TILT_SPEED]    = "SENSIB. INCL.",
    [STR_VERY_LOW]      = "MUY BAJA",
    [STR_LOW]           = "BAJA",
    [STR_MEDIUM]        = "MEDIA",
    [STR_HIGH]          = "ALTA",
    [STR_VERY_HIGH]     = "MUY ALTA",
    [STR_BACK]          = "VOLVER",
    [STR_FULLSCREEN]    = "PANTALLA COMP.",
    [STR_DESC_PAD_SPEED]  = "Velocidad de los botones I/D",
    [STR_DESC_DRAG]       = "Deslizar dedo para mover",
    [STR_DESC_TILT]       = "Inclinar para mover la paleta",
    [STR_DESC_TILT_SPEED] = "Sensibilidad del acelerometro",
    [STR_SELECT_WORLD]  = "ELIGE MUNDO",
    [STR_SPACE]         = "ESPACIO",
    [STR_ECLIPSE]       = "ECLIPSE",
    [STR_SELECT_DIFFICULTY] = "DIFICULTAD",
    [STR_EASY]          = "FACIL",
    [STR_NORMAL]        = "NORMAL",
    [STR_HARD]          = "DIFICIL",
    [STR_READY]         = "LISTO ?",
    [STR_GAME_PAUSED]   = "PAUSA",
    [STR_GAME_OVER]     = "FIN DEL JUEGO",
    [STR_DEMO_LABEL]    = "DEMO",
    [STR_FIRE]          = "FUEGO",
    [STR_RESUME]        = "REANUDAR",
    [STR_EXIT]          = "SALIR",
    [STR_TAP_TO_START]  = "TOCA PARA EMPEZAR",
    [STR_PRESS_A_START] = "PULSA A PARA EMPEZAR",
    [STR_P_ESC_RESUME]  = "P o ESC para reanudar",
    [STR_START_A_RESUME_B_EXIT] = "START/A: reanudar / B: salir",
    [STR_PAUSE_DRAG_HINT] = "Deslizar dedo para mover",
    [STR_PAUSE_TILT_HINT] = "Inclinar para mover la paleta",
    /* Original-style lowercase menu labels (Blaster_es.cfg). */
    [STR_M_PLAY]          = "jugar",
    [STR_M_DEMO]          = "demo",
    [STR_M_MISC]          = "diverso",
    [STR_M_QUIT]          = "abandonar",
    [STR_M_ONE_PLAYER]    = "un jugador",
    /* Blaster_es.cfg:87-88 — byte-exact (drop final period for COOP, keep
     * spelling `duelo` per cfg). */
    [STR_M_COOP]          = "2 jugadores cooperado",
    [STR_M_DUAL]          = "2 jugadores modo duelo",
    [STR_M_CANCEL]        = "retorno",
    [STR_M_COMPUTER]      = "ordenador",
    [STR_M_KEYBOARD]      = "teclado",
    [STR_M_JOYSTICK]      = "joystick",
    [STR_M_SPACE]         = "espacio",
    [STR_M_ARCADE]        = "arcade",
    [STR_M_EASY]          = "facil",
    [STR_M_MEDIUM]        = "medio",
    [STR_M_HARD]          = "dificil",
    /* Blaster_es.cfg:102-103 — byte-exact (retains the odd ASM spelling
     * "puntacion" and the abbreviated "puntuaci."). */
    [STR_M_HISCORES]      = "mejores puntacion",
    [STR_M_HISCORES_COOP] = "mejores puntuaci. coop",
    [STR_M_CREDITS]       = "creditos",
    [STR_M_EDITOR]        = "editor",
    [STR_M_ANALOGIC]      = "analogico",
    [STR_M_NUMERIC]       = "numerico",
    [STR_M_BLANK]         = "",
    [STR_M_TITLE_1]       = "menu principal",
    [STR_M_TITLE_2]       = "jugadores",
    /* Blaster_es.cfg:90 / :94 — byte-exact (spelled-out titles). */
    [STR_M_TITLE_3]       = "controle del jugador 2",
    [STR_M_TITLE_4]       = "escoger el mundo",
    [STR_M_TITLE_5]       = "dificultad",
    [STR_M_TITLE_6]       = "diverso",
    [STR_M_TITLE_7]       = "joystick",
    /* Blaster_es.cfg option_text_* — 18 chars each, byte-exact (Blaster_es.cfg:48-53). */
    [STR_OPT_READY]       = "      listo       ",
    [STR_OPT_PAUSED]      = "       pausa      ",
    [STR_OPT_GAME_OVER]   = "    game over     ",
    [STR_OPT_DEMO]        = "       demo       ",
    [STR_OPT_PLAY_AGAIN]  = "  jugas otra vez  ",
    /* Dual-mode winner banners — ES (no ASM cfg entry, invented).
     * 18 chars each, FONTE lowercase. */
    [STR_OPT_P1_WINS]     = "  jugador 1 gana  ",
    [STR_OPT_P2_WINS]     = "  jugador 2 gana  ",
    [STR_OPT_DRAW]        = "      empate      ",
    /* Blaster_es.cfg:56-79 Option_* — 24 powerup labels, 18 chars each. */
    [STR_OPT_POW_BALL_3]      = "      1 bola      ",
    [STR_OPT_POW_BALL_6]      = "     3 bolas      ",
    [STR_OPT_POW_BALL_9]      = "     6 bolas      ",
    [STR_OPT_POW_BALL_20]     = "     20 bolas     ",
    [STR_OPT_POW_DEATH]       = "    vida perdida  ",
    [STR_OPT_POW_NEW_LIFE]    = "    nueva vida    ",
    [STR_OPT_POW_SHOOT]       = "      misil       ",
    [STR_OPT_POW_SLOW_BALL]   = "     mas lento    ",
    [STR_OPT_POW_FAST_BALL]   = "     mas rapido   ",
    [STR_OPT_POW_IRON_BALL]   = "  bola de metal   ",
    [STR_OPT_POW_TELEPOD]     = "      telepod     ",
    [STR_OPT_POW_NIGHT]       = "    puntos x2     ",
    [STR_OPT_POW_SMALL_SHIP]  = "    pequena nave  ",
    [STR_OPT_POW_LARGE_SHIP]  = "     gran nave    ",
    [STR_OPT_POW_REVERSE]     = "    al reverse    ",
    [STR_OPT_POW_NEXT_LEVEL]  = "  siguiente nivel ",
    [STR_OPT_POW_DEL_MONSTER] = "     apocalipsis  ",
    [STR_OPT_POW_MAGNETIC]    = "        iman      ",
    [STR_OPT_POW_ADD_MONSTER] = "     monstruo     ",
    [STR_OPT_POW_GHOST]       = "     fantasma     ",
    [STR_OPT_POW_BONUS]       = "     +100 pts     ",
    [STR_OPT_POW_MALUS]       = "     -100 pts     ",
    [STR_OPT_POW_MINI_SHOOT]  = "       tiro       ",
    [STR_OPT_POW_COLLISION]   = "     colision     ",
},

/* ---------- ITALIAN ---------- */
[LANG_IT] = {
    [STR_PLAY]          = "GIOCA",
    [STR_SCORES]        = "PUNTEGGI",
    [STR_LEVEL_EDITOR]  = "EDITOR",
    [STR_SETTINGS]      = "OPZIONI",
    [STR_SUPPORT]       = "AIUTO",
    [STR_MUSIC]         = "MUSICA",
    [STR_SFX]           = "SFX",
    [STR_ON]            = "SI",
    [STR_OFF]           = "NO",
    [STR_PAD_SPEED]     = "VELOCITA PAD",
    [STR_DRAG]          = "TRASCINA",
    [STR_TILT]          = "INCLINA",
    [STR_TILT_SPEED]    = "SENSIB. INCL.",
    [STR_VERY_LOW]      = "MOLTO BASSA",
    [STR_LOW]           = "BASSA",
    [STR_MEDIUM]        = "MEDIA",
    [STR_HIGH]          = "ALTA",
    [STR_VERY_HIGH]     = "MOLTO ALTA",
    [STR_BACK]          = "INDIETRO",
    [STR_FULLSCREEN]    = "SCHERMO INTERO",
    [STR_DESC_PAD_SPEED]  = "Velocita dei tasti S/D",
    [STR_DESC_DRAG]       = "Scorrere il dito per muovere",
    [STR_DESC_TILT]       = "Inclinare per muovere",
    [STR_DESC_TILT_SPEED] = "Sensibilita dell'inclinazione",
    [STR_SELECT_WORLD]  = "SCEGLI MONDO",
    [STR_SPACE]         = "SPAZIO",
    [STR_ECLIPSE]       = "ECLISSI",
    [STR_SELECT_DIFFICULTY] = "DIFFICOLTA",
    [STR_EASY]          = "FACILE",
    [STR_NORMAL]        = "NORMALE",
    [STR_HARD]          = "DIFFICILE",
    [STR_READY]         = "PRONTO ?",
    [STR_GAME_PAUSED]   = "PAUSA",
    [STR_GAME_OVER]     = "GAME OVER",
    [STR_DEMO_LABEL]    = "DEMO",
    [STR_FIRE]          = "FUOCO",
    [STR_RESUME]        = "RIPRENDI",
    [STR_EXIT]          = "ESCI",
    [STR_TAP_TO_START]  = "TOCCA PER INIZIARE",
    [STR_PRESS_A_START] = "PREMI A PER INIZIARE",
    [STR_P_ESC_RESUME]  = "P o ESC per riprendere",
    [STR_START_A_RESUME_B_EXIT] = "START/A: riprendi / B: esci",
    [STR_PAUSE_DRAG_HINT] = "Scorrere il dito per muovere",
    [STR_PAUSE_TILT_HINT] = "Inclinare per muovere",
    /* Menu labels — IT. No original cfg, best-effort translations.
     * FONTE charset: a-z 0-9 + - # ! ? : . & space (no accents). */
    [STR_M_PLAY]          = "gioca",
    [STR_M_DEMO]          = "demo",
    [STR_M_MISC]          = "varie",
    [STR_M_QUIT]          = "esci",
    [STR_M_ONE_PLAYER]    = "un giocatore",
    [STR_M_COOP]          = "2 giocatori coop",
    [STR_M_DUAL]          = "2 giocatori duello",
    [STR_M_CANCEL]        = "annulla",
    [STR_M_COMPUTER]      = "computer",
    [STR_M_KEYBOARD]      = "tastiera",
    [STR_M_JOYSTICK]      = "joystick",
    [STR_M_SPACE]         = "spazio",
    [STR_M_ARCADE]        = "arcade",
    [STR_M_EASY]          = "facile",
    [STR_M_MEDIUM]        = "medio",
    [STR_M_HARD]          = "difficile",
    [STR_M_HISCORES]      = "record",
    [STR_M_HISCORES_COOP] = "record coop",
    [STR_M_CREDITS]       = "crediti",
    [STR_M_EDITOR]        = "editor",
    [STR_M_ANALOGIC]      = "analogico",
    [STR_M_NUMERIC]       = "numerico",
    [STR_M_BLANK]         = "",
    [STR_M_TITLE_1]       = "menu principale",
    [STR_M_TITLE_2]       = "giocatori",
    [STR_M_TITLE_3]       = "controllo giocatore 2",
    [STR_M_TITLE_4]       = "scegli mondo",
    [STR_M_TITLE_5]       = "difficolta",
    [STR_M_TITLE_6]       = "varie",
    [STR_M_TITLE_7]       = "joystick",
    /* option_text_* (IT) — 18 chars each. */
    [STR_OPT_READY]       = "      pronto      ",
    [STR_OPT_PAUSED]      = "       pausa      ",
    [STR_OPT_GAME_OVER]   = "    game over     ",
    [STR_OPT_DEMO]        = "       demo       ",
    [STR_OPT_PLAY_AGAIN]  = "   ancora gioca   ",
    /* Dual-mode winner banners — IT. 18 chars each. */
    [STR_OPT_P1_WINS]     = " giocatore 1 vince",
    [STR_OPT_P2_WINS]     = " giocatore 2 vince",
    [STR_OPT_DRAW]        = "     pareggio     ",
    /* Powerup labels — IT falls back to EN (no ASM cfg). 18 chars each. */
    [STR_OPT_POW_BALL_3]      = "      1 ball      ",
    [STR_OPT_POW_BALL_6]      = "     3 balls      ",
    [STR_OPT_POW_BALL_9]      = "     6 balls      ",
    [STR_OPT_POW_BALL_20]     = "     20 balls     ",
    [STR_OPT_POW_DEATH]       = "    lost life     ",
    [STR_OPT_POW_NEW_LIFE]    = "     new life     ",
    [STR_OPT_POW_SHOOT]       = "    big shoot     ",
    [STR_OPT_POW_SLOW_BALL]   = "       slow       ",
    [STR_OPT_POW_FAST_BALL]   = "       fast       ",
    [STR_OPT_POW_IRON_BALL]   = "    iron ball     ",
    [STR_OPT_POW_TELEPOD]     = "     telepod      ",
    [STR_OPT_POW_NIGHT]       = "     point x2     ",
    [STR_OPT_POW_SMALL_SHIP]  = "    small ship    ",
    [STR_OPT_POW_LARGE_SHIP]  = "    large ship    ",
    [STR_OPT_POW_REVERSE]     = "     reverse      ",
    [STR_OPT_POW_NEXT_LEVEL]  = "    next level    ",
    [STR_OPT_POW_DEL_MONSTER] = "   wrath of god   ",
    [STR_OPT_POW_MAGNETIC]    = "     magnetic     ",
    [STR_OPT_POW_ADD_MONSTER] = "   summon beast   ",
    [STR_OPT_POW_GHOST]       = "      ghost       ",
    [STR_OPT_POW_BONUS]       = "     +100 pts     ",
    [STR_OPT_POW_MALUS]       = "     -100 pts     ",
    [STR_OPT_POW_MINI_SHOOT]  = "      shoot       ",
    [STR_OPT_POW_COLLISION]   = "    collision     ",
},

/* ---------- PORTUGUESE ---------- */
[LANG_PT] = {
    [STR_PLAY]          = "JOGAR",
    [STR_SCORES]        = "PONTOS",
    [STR_LEVEL_EDITOR]  = "EDITOR",
    [STR_SETTINGS]      = "OPCOES",
    [STR_SUPPORT]       = "AJUDA",
    [STR_MUSIC]         = "MUSICA",
    [STR_SFX]           = "SFX",
    [STR_ON]            = "SIM",
    [STR_OFF]           = "NAO",
    [STR_PAD_SPEED]     = "VELOCIDADE PAD",
    [STR_DRAG]          = "ARRASTAR",
    [STR_TILT]          = "INCLINAR",
    [STR_TILT_SPEED]    = "SENSIB. INCL.",
    [STR_VERY_LOW]      = "MUITO BAIXA",
    [STR_LOW]           = "BAIXA",
    [STR_MEDIUM]        = "MEDIA",
    [STR_HIGH]          = "ALTA",
    [STR_VERY_HIGH]     = "MUITO ALTA",
    [STR_BACK]          = "VOLTAR",
    [STR_FULLSCREEN]    = "TELA CHEIA",
    [STR_DESC_PAD_SPEED]  = "Velocidade dos botoes E/D",
    [STR_DESC_DRAG]       = "Deslizar o dedo para mover",
    [STR_DESC_TILT]       = "Inclinar para mover a raquete",
    [STR_DESC_TILT_SPEED] = "Sensibilidade do acelerometro",
    [STR_SELECT_WORLD]  = "ESCOLHA MUNDO",
    [STR_SPACE]         = "ESPACO",
    [STR_ECLIPSE]       = "ECLIPSE",
    [STR_SELECT_DIFFICULTY] = "DIFICULDADE",
    [STR_EASY]          = "FACIL",
    [STR_NORMAL]        = "NORMAL",
    [STR_HARD]          = "DIFICIL",
    [STR_READY]         = "PRONTO ?",
    [STR_GAME_PAUSED]   = "PAUSA",
    [STR_GAME_OVER]     = "FIM DE JOGO",
    [STR_DEMO_LABEL]    = "DEMO",
    [STR_FIRE]          = "FOGO",
    [STR_RESUME]        = "RETOMAR",
    [STR_EXIT]          = "SAIR",
    [STR_TAP_TO_START]  = "TOQUE PARA INICIAR",
    [STR_PRESS_A_START] = "PRESSIONE A P/ INICIAR",
    [STR_P_ESC_RESUME]  = "P ou ESC para retomar",
    [STR_START_A_RESUME_B_EXIT] = "START/A: retomar / B: sair",
    [STR_PAUSE_DRAG_HINT] = "Deslizar o dedo para mover",
    [STR_PAUSE_TILT_HINT] = "Inclinar para mover a raquete",
    /* Menu labels — PT. No original cfg, best-effort translations.
     * FONTE charset: a-z 0-9 + - # ! ? : . & space (no accents). */
    [STR_M_PLAY]          = "jogar",
    [STR_M_DEMO]          = "demo",
    [STR_M_MISC]          = "diversos",
    [STR_M_QUIT]          = "sair",
    [STR_M_ONE_PLAYER]    = "um jogador",
    [STR_M_COOP]          = "2 jogadores coop",
    [STR_M_DUAL]          = "2 jogadores duelo",
    [STR_M_CANCEL]        = "voltar",
    [STR_M_COMPUTER]      = "computador",
    [STR_M_KEYBOARD]      = "teclado",
    [STR_M_JOYSTICK]      = "joystick",
    [STR_M_SPACE]         = "espaco",
    [STR_M_ARCADE]        = "arcade",
    [STR_M_EASY]          = "facil",
    [STR_M_MEDIUM]        = "medio",
    [STR_M_HARD]          = "dificil",
    [STR_M_HISCORES]      = "melhores pontos",
    [STR_M_HISCORES_COOP] = "melhores pontos coop",
    [STR_M_CREDITS]       = "creditos",
    [STR_M_EDITOR]        = "editor",
    [STR_M_ANALOGIC]      = "analogico",
    [STR_M_NUMERIC]       = "numerico",
    [STR_M_BLANK]         = "",
    [STR_M_TITLE_1]       = "menu principal",
    [STR_M_TITLE_2]       = "jogadores",
    [STR_M_TITLE_3]       = "controle jogador 2",
    [STR_M_TITLE_4]       = "escolher mundo",
    [STR_M_TITLE_5]       = "dificuldade",
    [STR_M_TITLE_6]       = "diversos",
    [STR_M_TITLE_7]       = "joystick",
    /* option_text_* (PT) — 18 chars each. */
    [STR_OPT_READY]       = "      pronto      ",
    [STR_OPT_PAUSED]      = "       pausa      ",
    [STR_OPT_GAME_OVER]   = "    game over     ",
    [STR_OPT_DEMO]        = "       demo       ",
    [STR_OPT_PLAY_AGAIN]  = "    jogar mais    ",
    /* Dual-mode winner banners — PT. 18 chars each. */
    [STR_OPT_P1_WINS]     = "  jogador 1 vence ",
    [STR_OPT_P2_WINS]     = "  jogador 2 vence ",
    [STR_OPT_DRAW]        = "      empate      ",
    /* Powerup labels — PT falls back to EN (no ASM cfg). 18 chars each. */
    [STR_OPT_POW_BALL_3]      = "      1 ball      ",
    [STR_OPT_POW_BALL_6]      = "     3 balls      ",
    [STR_OPT_POW_BALL_9]      = "     6 balls      ",
    [STR_OPT_POW_BALL_20]     = "     20 balls     ",
    [STR_OPT_POW_DEATH]       = "    lost life     ",
    [STR_OPT_POW_NEW_LIFE]    = "     new life     ",
    [STR_OPT_POW_SHOOT]       = "    big shoot     ",
    [STR_OPT_POW_SLOW_BALL]   = "       slow       ",
    [STR_OPT_POW_FAST_BALL]   = "       fast       ",
    [STR_OPT_POW_IRON_BALL]   = "    iron ball     ",
    [STR_OPT_POW_TELEPOD]     = "     telepod      ",
    [STR_OPT_POW_NIGHT]       = "     point x2     ",
    [STR_OPT_POW_SMALL_SHIP]  = "    small ship    ",
    [STR_OPT_POW_LARGE_SHIP]  = "    large ship    ",
    [STR_OPT_POW_REVERSE]     = "     reverse      ",
    [STR_OPT_POW_NEXT_LEVEL]  = "    next level    ",
    [STR_OPT_POW_DEL_MONSTER] = "   wrath of god   ",
    [STR_OPT_POW_MAGNETIC]    = "     magnetic     ",
    [STR_OPT_POW_ADD_MONSTER] = "   summon beast   ",
    [STR_OPT_POW_GHOST]       = "      ghost       ",
    [STR_OPT_POW_BONUS]       = "     +100 pts     ",
    [STR_OPT_POW_MALUS]       = "     -100 pts     ",
    [STR_OPT_POW_MINI_SHOOT]  = "      shoot       ",
    [STR_OPT_POW_COLLISION]   = "    collision     ",
},

}; /* end s_strings */


/* =========================================================================
 * Public API
 * ========================================================================= */

void i18n_init(void) {
    s_lang = LANG_EN;
#if !defined(PLATFORM_ANDROID)
    /* Desktop/Web: check LANG environment variable */
    const char *env = getenv("LANG");
    if (!env) env = getenv("LANGUAGE");
    if (env) {
        if      (env[0]=='f' && env[1]=='r') s_lang = LANG_FR;
        else if (env[0]=='d' && env[1]=='e') s_lang = LANG_DE;
        else if (env[0]=='e' && env[1]=='s') s_lang = LANG_ES;
        else if (env[0]=='i' && env[1]=='t') s_lang = LANG_IT;
        else if (env[0]=='p' && env[1]=='t') s_lang = LANG_PT;
    }
#endif
}

Language i18n_get_language(void) { return s_lang; }
void i18n_set_language(Language lang) {
    if (lang >= 0 && lang < LANG_COUNT) s_lang = lang;
}

const char *i18n(StringId id) {
    if (id < 0 || id >= STR_COUNT) return "?";
    const char *s = s_strings[s_lang][id];
    if (s) return s;
    /* Fallback to English */
    s = s_strings[LANG_EN][id];
    return s ? s : "?";
}

int i18n_draw_fit(const char *text, int cx, int cy, int max_width,
                  int max_fs, int min_fs, unsigned int color_rgba) {
    Color c = *(Color *)&color_rgba;
    int fs = max_fs;
    int tw = MeasureText(text, fs);
    while (tw > max_width && fs > min_fs) {
        fs--;
        tw = MeasureText(text, fs);
    }
    DrawText(text, cx - tw / 2, cy - fs / 2, fs, c);
    return fs;
}

/* =========================================================================
 * Android language detection via JNI
 * ========================================================================= */
#if defined(PLATFORM_ANDROID)
#include <jni.h>
#include <android/log.h>

void i18n_detect_android(void *android_app_ptr) {
    struct {
        void *userData;
        void *onAppCmd;
        void *onInputEvent;
        void *activity;
    } *app = android_app_ptr;

    if (!app || !app->activity) return;

    typedef struct {
        void   *callbacks;
        JavaVM *vm;
        JNIEnv *env;
        jobject clazz;
    } NativeActivityCompat;

    NativeActivityCompat *activity = (NativeActivityCompat *)app->activity;
    JNIEnv *env = NULL;
    (*activity->vm)->AttachCurrentThread(activity->vm, &env, NULL);
    if (!env) return;

    /* java.util.Locale.getDefault().getLanguage() */
    jclass locale_cls = (*env)->FindClass(env, "java/util/Locale");
    if (!locale_cls) return;
    jmethodID get_default = (*env)->GetStaticMethodID(env, locale_cls,
        "getDefault", "()Ljava/util/Locale;");
    jmethodID get_lang = (*env)->GetMethodID(env, locale_cls,
        "getLanguage", "()Ljava/lang/String;");
    if (!get_default || !get_lang) return;

    jobject locale = (*env)->CallStaticObjectMethod(env, locale_cls, get_default);
    if (!locale) return;
    jstring jlang = (jstring)(*env)->CallObjectMethod(env, locale, get_lang);
    if (!jlang) return;

    const char *lang = (*env)->GetStringUTFChars(env, jlang, NULL);
    if (lang) {
        if      (lang[0]=='f' && lang[1]=='r') s_lang = LANG_FR;
        else if (lang[0]=='d' && lang[1]=='e') s_lang = LANG_DE;
        else if (lang[0]=='e' && lang[1]=='s') s_lang = LANG_ES;
        else if (lang[0]=='i' && lang[1]=='t') s_lang = LANG_IT;
        else if (lang[0]=='p' && lang[1]=='t') s_lang = LANG_PT;
        /* ja, ko → stay LANG_EN (default font lacks CJK glyphs) */
        __android_log_print(ANDROID_LOG_INFO, "BrickBlaster",
                            "i18n: device language=%s → lang_id=%d", lang, s_lang);
        (*env)->ReleaseStringUTFChars(env, jlang, lang);
    }
}
#else
void i18n_detect_android(void *p) { (void)p; }
#endif
