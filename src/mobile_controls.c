#include "mobile_controls.h"
#if defined(BRICKBLASTER_MOBILE)

#include "constants.h"
#include <raylib.h>
#include <string.h>

#define TAKOHI_ORANGE (Color){232, 115, 74, 255}

/* Per-frame button state */
static int s_left_down      = 0;
static int s_right_down     = 0;
static int s_fire_down      = 0;
static int s_game_tap_down  = 0;  /* any touch in the play area (not on buttons) */
/* Previous frame — for edge detection */
static int s_left_prev      = 0;
static int s_right_prev     = 0;
static int s_fire_prev      = 0;
static int s_game_tap_prev  = 0;

static const Rectangle s_left_rect  = { MC_LEFT_X,  MC_LEFT_Y,  MC_LEFT_W,  MC_LEFT_H  };
static const Rectangle s_right_rect = { MC_RIGHT_X, MC_RIGHT_Y, MC_RIGHT_W, MC_RIGHT_H };

/* Cached letterbox scale — recomputed only when screen size changes */
static int   s_cached_ww = 0, s_cached_wh = 0;
static float s_scale = 1.0f, s_ox = 0.0f, s_oy = 0.0f;

static void update_scale_cache(void) {
    int ww = GetScreenWidth();
    int wh = GetScreenHeight();
    if (ww == s_cached_ww && wh == s_cached_wh) return;
    s_cached_ww = ww;
    s_cached_wh = wh;
    float sx = (float)ww / SCREEN_W;
    float sy = (float)wh / SCREEN_H;
    s_scale = (sx < sy) ? sx : sy;
    s_ox = (ww - SCREEN_W * s_scale) / 2.0f;
    s_oy = (wh - SCREEN_H * s_scale) / 2.0f;
}

static Vector2 to_canvas(Vector2 win) {
    return (Vector2){ (win.x - s_ox) / s_scale, (win.y - s_oy) / s_scale };
}

void mobile_controls_update(void) {
    update_scale_cache();

    s_left_prev     = s_left_down;
    s_right_prev    = s_right_down;
    s_fire_prev     = s_fire_down;
    s_game_tap_prev = s_game_tap_down;

    s_left_down     = 0;
    s_right_down    = 0;
    s_fire_down     = 0;
    s_game_tap_down = 0;

    int count = GetTouchPointCount();
    for (int i = 0; i < count; i++) {
        Vector2 cp = to_canvas(GetTouchPosition(i));
        int on_left  = CheckCollisionPointRec(cp, s_left_rect);
        int on_right = CheckCollisionPointRec(cp, s_right_rect);
        /* Extend hit area to letterbox margins: touches outside the canvas
         * (negative X = left margin, X > SCREEN_W = right margin) count as
         * button presses so the entire screen edge is tappable. */
        if (!on_left && cp.x < 0 && cp.y >= MC_LEFT_Y)
            on_left = 1;
        if (!on_right && cp.x >= SCREEN_W && cp.y >= MC_RIGHT_Y)
            on_right = 1;
        float dx = cp.x - MC_FIRE_CX;
        float dy = cp.y - MC_FIRE_CY;
        int on_fire  = (dx*dx + dy*dy <= (float)(MC_FIRE_R * MC_FIRE_R));

        if (on_left)  s_left_down  = 1;
        if (on_right) s_right_down = 1;
        if (on_fire)  s_fire_down  = 1;
        if (!on_left && !on_right && !on_fire) s_game_tap_down = 1;
    }
}

int mobile_controls_is_control_area(Vector2 canvas_pos) {
    if (CheckCollisionPointRec(canvas_pos, s_left_rect))  return 1;
    if (CheckCollisionPointRec(canvas_pos, s_right_rect)) return 1;
    float dx = canvas_pos.x - MC_FIRE_CX;
    float dy = canvas_pos.y - MC_FIRE_CY;
    if (dx*dx + dy*dy <= (float)(MC_FIRE_R * MC_FIRE_R)) return 1;
    return 0;
}

int mobile_left_down(void)       { return s_left_down;  }
int mobile_right_down(void)      { return s_right_down; }
int mobile_fire_pressed(void)    { return s_fire_down     && !s_fire_prev;     }
int mobile_left_pressed(void)    { return s_left_down     && !s_left_prev;     }
int mobile_right_pressed(void)   { return s_right_down    && !s_right_prev;    }
int mobile_game_tap_pressed(void){ return s_game_tap_down && !s_game_tap_prev; }

/* Cached label widths for the three buttons (avoid MeasureText every frame) */
static char s_cached_label[16] = "";
static int  s_cached_label_w   = 0;
static int  s_cached_label_fs  = 0;

/* Draw a rectangular button (left / right). */
static void draw_rect_btn(Rectangle r, const char *label, int pressed) {
    Color bg     = pressed ? (Color){232, 115, 74, 170} : (Color){30, 30, 44, 140};
    Color border = pressed ? (Color){232, 115, 74, 255} : (Color){110, 110, 135, 180};
    DrawRectangleRounded(r, 0.22f, 6, bg);
    DrawRectangleLinesEx(r, 2.0f, border);
    int fs = 28;
    int tw = MeasureText(label, fs);
    DrawText(label,
             (int)(r.x + (r.width  - tw) / 2),
             (int)(r.y + (r.height - fs) / 2),
             fs,
             pressed ? WHITE : (Color){200, 200, 215, 200});
}

/* Draw the circular fire button. */
static void draw_fire_btn(const char *label, int pressed, int enabled) {
    int cx = MC_FIRE_CX, cy = MC_FIRE_CY, r = MC_FIRE_R;

    Color bg, border, text_col;
    if (!enabled) {
        bg       = (Color){30, 30, 40, 150};
        border   = (Color){80, 80, 95, 160};
        text_col = (Color){100, 100, 110, 180};
    } else if (pressed) {
        bg       = (Color){232, 115, 74, 220};
        border   = (Color){232, 115, 74, 255};
        text_col = WHITE;
    } else {
        bg       = (Color){60, 35, 80, 210};
        border   = (Color){232, 115, 74, 230};
        text_col = (Color){230, 190, 255, 245};
    }

    DrawCircle(cx, cy, (float)r, bg);
    DrawCircleLines(cx, cy, (float)r, border);
    /* Second ring for a bolder outline */
    DrawCircleLines(cx, cy, (float)(r - 2), (Color){border.r, border.g, border.b, border.a / 2});

    /* Label — cache measure to avoid re-computing every frame */
    int fs = 16;
    if (s_cached_label_fs != fs || strcmp(label, s_cached_label) != 0) {
        int max = (int)(sizeof(s_cached_label) - 1);
        int li;
        for (li = 0; li < max && label[li]; li++) s_cached_label[li] = label[li];
        s_cached_label[li] = '\0';
        s_cached_label_fs = fs;
        s_cached_label_w  = MeasureText(label, fs);
    }
    DrawText(label, cx - s_cached_label_w / 2, cy - fs / 2, fs, text_col);
}

void mobile_controls_draw(const char *fire_label, int fire_enabled) {
    draw_rect_btn(s_left_rect,  "<", s_left_down);
    draw_rect_btn(s_right_rect, ">", s_right_down);
    draw_fire_btn(fire_label ? fire_label : "FIRE", s_fire_down, fire_enabled);
}

#endif /* BRICKBLASTER_MOBILE */
