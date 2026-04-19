#pragma once
/* letterbox.h — Shared letterbox coordinate conversion
 *
 * Converts between window/screen coordinates and the 640x480 game canvas.
 * Used by input polling, pause button hit-testing, menu clicks, mobile controls.
 */

#include "constants.h"
#include "raylib.h"

typedef struct {
    float scale;
    float offset_x;
    float offset_y;
} Letterbox;

/* Compute letterbox scale and offset for the current window size. */
static inline Letterbox letterbox_get(int win_w, int win_h) {
    float sx = (float)win_w / SCREEN_W;
    float sy = (float)win_h / SCREEN_H;
    float s  = (sx < sy) ? sx : sy;
    Letterbox lb;
    lb.scale    = s;
    lb.offset_x = (win_w - SCREEN_W * s) / 2.0f;
    lb.offset_y = (win_h - SCREEN_H * s) / 2.0f;
    return lb;
}

/* Convert window-space position to game canvas coordinates (640x480). */
static inline Vector2 letterbox_to_canvas(Letterbox lb, Vector2 win_pos) {
    return (Vector2){
        (win_pos.x - lb.offset_x) / lb.scale,
        (win_pos.y - lb.offset_y) / lb.scale
    };
}

/* Shortcut: compute letterbox from current window and convert a point. */
static inline Vector2 letterbox_screen_to_canvas(Vector2 win_pos) {
    Letterbox lb = letterbox_get(GetScreenWidth(), GetScreenHeight());
    return letterbox_to_canvas(lb, win_pos);
}
