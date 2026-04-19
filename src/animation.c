#include "animation.h"
#include <stdio.h>
#include <stdlib.h>

// MAIN.ASM doesn't specify FLC FPS, but standard is 18
// FILE.ASM:54-116 (intro.flc), FILE.ASM:118-183 (Blaster.flc)

Animation animation_load(const char *dir, int count, float fps) {
    Animation anim = {0};
    anim.frames = malloc(sizeof(Texture2D) * count);
    if (!anim.frames) {
        return anim;  // Failed allocation
    }

    anim.frame_count = count;
    anim.fps = fps;
    anim.frame_interval = (fps > 0.0f) ? (1.0f / fps) : 0.0f;
    anim.current_frame = 0;
    anim.frame_timer = 0;
    anim.loop = 0;

    for (int i = 0; i < count; i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/frame_%04d.png", dir, i + 1);
        anim.frames[i] = LoadTexture(path);

        // Check if texture loaded successfully (id == 0 means failure)
        if (anim.frames[i].id == 0) {
            // Clean up previously loaded textures
            for (int j = 0; j < i; j++) {
                UnloadTexture(anim.frames[j]);
            }
            free(anim.frames);
            anim.frames = NULL;
            anim.frame_count = 0;
            return anim;
        }
    }

    return anim;
}

void animation_update(Animation *anim) {
    if (!anim || !anim->frames) {
        return;
    }

    if (anim->current_frame >= anim->frame_count && !anim->loop) {
        return;  // Finished
    }

    anim->frame_timer += (1.0f / 60.0f);  /* fixed 60fps — GetFrameTime stale with custom frame control */
    if (anim->frame_timer >= anim->frame_interval) {
        anim->current_frame++;
        anim->frame_timer = 0;

        if (anim->loop && anim->current_frame >= anim->frame_count) {
            anim->current_frame = 0;
        }
    }
}

void animation_draw(Animation *anim, int x, int y) {
    if (!anim || !anim->frames) {
        return;
    }

    if (anim->current_frame < anim->frame_count) {
        DrawTexture(anim->frames[anim->current_frame], x, y, WHITE);
    }
}

int animation_is_finished(Animation *anim) {
    if (!anim || !anim->frames) {
        return 1;  // Treat NULL as finished
    }
    return anim->current_frame >= anim->frame_count;
}

void animation_unload(Animation *anim) {
    if (!anim || !anim->frames) {
        return;
    }

    for (int i = 0; i < anim->frame_count; i++) {
        UnloadTexture(anim->frames[i]);
    }
    free(anim->frames);
    anim->frames = NULL;
    anim->frame_count = 0;
}

void animation_reset(Animation *anim) {
    if (!anim) {
        return;
    }

    anim->current_frame = 0;
    anim->frame_timer = 0;
}
