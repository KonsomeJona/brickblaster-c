#include "music_manager.h"
#include "assets.h"
#include <stdio.h>
#include <string.h>

// MAIN.ASM:48-58 music file names (wav conversions of original mod tracks)
static const char *music_paths[MUSIC_COUNT] = {
    ASSETS_BASE "music/Blaster.wav",
    ASSETS_BASE "music/Thelast.wav",
    ASSETS_BASE "music/Lode.wav",
    ASSETS_BASE "music/Credit.wav",
    ASSETS_BASE "music/Rain.wav"
};

/* Resolve a music path with extension fallback: prefer the .ogg variant when
 * one exists next to the canonical .wav. The web bundle converts the 82 MB of
 * WAV music to OGG at CI time to keep the download sane (raylib decodes OGG
 * natively via stb_vorbis); desktop distributions ship the WAVs unchanged, so
 * there the fallback is a no-op. Returns wav_path itself or `buf`. */
static const char *music_resolve_path(const char *wav_path,
                                      char *buf, size_t buf_len) {
    size_t len = strlen(wav_path);
    if (len < 4 || len >= buf_len) return wav_path;
    memcpy(buf, wav_path, len + 1);
    if (strcmp(buf + len - 4, ".wav") == 0) {
        memcpy(buf + len - 4, ".ogg", 4);   /* same length — NUL kept */
        if (FileExists(buf)) return buf;
    }
    return wav_path;
}

void music_manager_init(MusicManager *mgr) {
    if (!mgr) return;

    memset(mgr, 0, sizeof(MusicManager));

    for (int i = 0; i < MUSIC_COUNT; i++) {
        char pathbuf[512];
        const char *path = music_resolve_path(music_paths[i],
                                              pathbuf, sizeof(pathbuf));
        mgr->tracks[i] = LoadMusicStream(path);

        // Check if load succeeded
        if (mgr->tracks[i].ctxData != NULL) {
            mgr->tracks_loaded[i] = 1;
            SetMusicVolume(mgr->tracks[i], MUSIC_DEFAULT_VOLUME);
        } else {
            mgr->tracks_loaded[i] = 0;
            fprintf(stderr, "Warning: Failed to load %s\n", path);
        }
    }

    mgr->current = MUSIC_BLASTER;
    mgr->initialized = 1;
    mgr->music_enabled = 1;

    // Start with menu music if loaded
    if (mgr->tracks_loaded[MUSIC_BLASTER]) {
        PlayMusicStream(mgr->tracks[MUSIC_BLASTER]);
    }
}

void music_manager_set_enabled(MusicManager *mgr, int enabled) {
    if (!mgr || !mgr->initialized) return;
    mgr->music_enabled = enabled;
    float vol = enabled ? MUSIC_DEFAULT_VOLUME : 0.0f;
    for (int i = 0; i < MUSIC_COUNT; i++) {
        if (mgr->tracks_loaded[i]) {
            SetMusicVolume(mgr->tracks[i], vol);
        }
    }
}

void music_manager_switch(MusicManager *mgr, MusicTrack track) {
    if (!mgr || !mgr->initialized) return;
    if (track < 0 || track >= MUSIC_COUNT) return;
    if (track == mgr->current) return;

    // Stop current track if loaded
    if (mgr->tracks_loaded[mgr->current]) {
        StopMusicStream(mgr->tracks[mgr->current]);
    }

    // Play new track if loaded
    mgr->current = track;
    if (mgr->tracks_loaded[mgr->current]) {
        PlayMusicStream(mgr->tracks[mgr->current]);
    }
}

void music_manager_update(MusicManager *mgr) {
    if (!mgr || !mgr->initialized) return;
    if (mgr->tracks_loaded[mgr->current]) {
        UpdateMusicStream(mgr->tracks[mgr->current]);
    }
}

void music_manager_cleanup(MusicManager *mgr) {
    if (!mgr) return;

    for (int i = 0; i < MUSIC_COUNT; i++) {
        if (mgr->tracks_loaded[i]) {
            UnloadMusicStream(mgr->tracks[i]);
            mgr->tracks_loaded[i] = 0;
        }
    }
    mgr->initialized = 0;
}
