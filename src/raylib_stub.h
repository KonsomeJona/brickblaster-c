#ifndef RAYLIB_STUB_H
#define RAYLIB_STUB_H
/* Minimal raylib stub for test builds - provides only what's needed */

/* Color type used by font.h - only needed for header inclusion */
typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

/* Font structure - only needed for header inclusion */
typedef struct {
    int baseSize;
    int glyphCount;
    int glyphPadding;
} Font;

#endif
