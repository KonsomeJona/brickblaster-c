#include "music_manager.h"
#include "assets.h"
#include <stdio.h>
#include <string.h>

/* The two decoders raylib 5.0 streams WAV and OGG music with. Declarations
 * only: the implementations are compiled into raylib (raudio.c), which keeps
 * them with external linkage. See "Seamless module loops" below for why the
 * port drives them itself. */
#include "external/dr_wav.h"
#define STB_VORBIS_HEADER_ONLY
#include "external/stb_vorbis.c"

// MAIN.ASM:48-58 music file names (wav conversions of original mod tracks)
static const char *music_paths[MUSIC_COUNT] = {
    ASSETS_BASE "music/Blaster.wav",
    ASSETS_BASE "music/Thelast.wav",
    ASSETS_BASE "music/Lode.wav",
    ASSETS_BASE "music/Credit.wav",
    ASSETS_BASE "music/Rain.wav"
};

/* ---------------------------------------------------------------------------
 * Seamless module loops
 *
 * The 1999 game played the original .mod files (FILE.ASM:11-16) through a
 * tracker player, so a song never "restarted": when it reached its last row
 * the player simply followed the module's own jump and kept going. Thelast,
 * Blaster and Rain end on a Bxx position jump into the middle of the song
 * (order 2, 1 and 5), Lode and Credit wrap to the start.
 *
 * The port ships one-pass renders of those modules instead (libopenmpt
 * 0.7.3, --stereo 200 --filter 1: identical to these files within +-2 LSB),
 * and raylib can only loop a stream back to frame 0. That replayed the intro
 * of Thelast/Blaster/Rain on every lap, and even where the jump target IS the
 * start, it dropped the notes still ringing from the last row.
 *
 * So the port feeds the stream itself:
 *   - play the file up to loop_end, the frame where the module jumps;
 *   - play the <name>_seam clip: what the module player outputs right after
 *     that jump (rendered from the same .mod with the same settings), i.e. the
 *     jump target's rows WITH the previous notes still ringing;
 *   - continue in the file at loop_start + seam_frames, where the second pass
 *     of the module has become sample-identical to the first.
 * Built that way, the stream matches libopenmpt playing the .mod for four
 * passes within +-4 LSB, intro and every lap. Credit needs no clip: its jump
 * lands 4 frames into the file and nothing rings across it.
 *
 * All values are frames of the 44.1 kHz files. The web/Android builds convert
 * every music/*.wav (the clips included) to OGG, which keeps the length.
 * ------------------------------------------------------------------------- */
typedef struct {
    unsigned int loop_end;     /* first frame NOT played before the jump        */
    unsigned int loop_start;   /* frame of the file the jump lands on           */
    unsigned int seam_frames;  /* length of the seam clip, 0 = none             */
    const char  *seam_path;
} MusicLoop;

static const MusicLoop music_loops[MUSIC_COUNT] = {
    /* Blaster: B01 at order 6 row 63 -> order 1 */
    { 2370816u,  338688u, 179970u, ASSETS_BASE "music/Blaster_seam.wav" },
    /* Thelast: B02 at order 24 row 63 -> order 2 */
    { 5785920u,  451584u, 109407u, ASSETS_BASE "music/Thelast_seam.wav" },
    /* Lode: song end -> order 0 */
    { 2906496u,       0u,   1426u, ASSETS_BASE "music/Lode_seam.wav" },
    /* Credit: song end -> order 0 */
    { 5904900u,       4u,      0u, NULL },
    /* Rain: B05 at order 11 row 59 -> order 5 */
    { 4262584u, 1878240u,  43380u, ASSETS_BASE "music/Rain_seam.wav" },
};

/* raylib's internal music context ids (raudio.c MUSIC_AUDIO_WAV/OGG, 5.0). */
#define CTX_WAV 1
#define CTX_OGG 2

typedef struct {
    int           fed;          /* 1 = this stream is filled by music_fill()   */
    int           ctx_type;
    void         *ctx;          /* drwav* or stb_vorbis*, owned by the Music   */
    int           channels;
    unsigned int  pos;          /* next frame of the file the decoder returns  */
    Wave          seam;         /* the clip, converted to the stream's format  */
    unsigned int  seam_frames;  /* frames of it that are played                */
    unsigned int  seam_pos;
    int           in_seam;
} TrackFeed;

/* Touched by the audio thread (through the callbacks) only while that track's
 * stream plays; the main thread only rewinds a stream it has stopped. */
static TrackFeed s_feed[MUSIC_COUNT];

static void feed_seek(TrackFeed *f, unsigned int frame) {
    if (f->ctx_type == CTX_WAV) drwav_seek_to_pcm_frame((drwav *)f->ctx, frame);
    /* stb_vorbis_seek, not _seek_frame: the latter stops at the start of the
     * Vorbis block holding the sample, hundreds of frames early. */
    else                        stb_vorbis_seek((stb_vorbis *)f->ctx, frame);
    f->pos = frame;
}

static unsigned int feed_read(TrackFeed *f, short *out, unsigned int frames) {
    if (f->ctx_type == CTX_WAV)
        return (unsigned int)drwav_read_pcm_frames_s16((drwav *)f->ctx, frames, out);
    {
        int n = stb_vorbis_get_samples_short_interleaved((stb_vorbis *)f->ctx, f->channels,
                                                         out, (int)frames * f->channels);
        return (n > 0) ? (unsigned int)n : 0u;
    }
}

/* Fill `frames` frames of interleaved s16 for track t (audio thread). */
static void music_fill(int t, short *out, unsigned int frames) {
    TrackFeed       *f = &s_feed[t];
    const MusicLoop *L = &music_loops[t];
    int stalls = 0;

    while (frames > 0) {
        unsigned int n;
        if (f->in_seam) {
            n = f->seam_frames - f->seam_pos;
            if (n > frames) n = frames;
            memcpy(out, (const short *)f->seam.data + (size_t)f->seam_pos * f->channels,
                   (size_t)n * f->channels * sizeof(short));
            f->seam_pos += n;
            if (f->seam_pos >= f->seam_frames) {
                f->in_seam = 0;
                feed_seek(f, L->loop_start + f->seam_frames);
            }
        } else {
            unsigned int want = (f->pos < L->loop_end) ? L->loop_end - f->pos : 0;
            if (want > frames) want = frames;
            n = want ? feed_read(f, out, want) : 0;
            f->pos += n;
            if (f->pos >= L->loop_end || n < want || want == 0) {
                /* The module jumps here (or the file ended early): */
                if (f->seam_frames > 0) { f->in_seam = 1; f->seam_pos = 0; }
                else                    feed_seek(f, L->loop_start);
                if (n == 0 && ++stalls > 2) {   /* decoder returns nothing */
                    memset(out, 0, (size_t)frames * f->channels * sizeof(short));
                    return;
                }
            }
        }
        out    += (size_t)n * f->channels;
        frames -= n;
    }
}

/* AudioCallback carries no user pointer: one trampoline per track. */
static void cb_track0(void *b, unsigned int n) { music_fill(0, (short *)b, n); }
static void cb_track1(void *b, unsigned int n) { music_fill(1, (short *)b, n); }
static void cb_track2(void *b, unsigned int n) { music_fill(2, (short *)b, n); }
static void cb_track3(void *b, unsigned int n) { music_fill(3, (short *)b, n); }
static void cb_track4(void *b, unsigned int n) { music_fill(4, (short *)b, n); }
static const AudioCallback s_callbacks[MUSIC_COUNT] = {
    cb_track0, cb_track1, cb_track2, cb_track3, cb_track4
};

/* Same .ogg-first rule as the music itself (see music_load_any). */
static Wave seam_load_any(const char *wav_path) {
    size_t len = strlen(wav_path);
    char buf[512];
    Wave w = { 0 };
    if (len >= 4 && len < sizeof(buf) && strcmp(wav_path + len - 4, ".wav") == 0) {
        memcpy(buf, wav_path, len + 1);
        memcpy(buf + len - 4, ".ogg", 4);
        w = LoadWave(buf);
        if (w.data != NULL) return w;
    }
    return LoadWave(wav_path);
}

/* Take over the stream of track i when its decoder is one we can drive. */
static void feed_setup(MusicManager *mgr, int i) {
    TrackFeed       *f = &s_feed[i];
    const MusicLoop *L = &music_loops[i];
    Music            m = mgr->tracks[i];

    memset(f, 0, sizeof(*f));
    if ((m.ctxType != CTX_WAV && m.ctxType != CTX_OGG) || m.ctxData == NULL) return;
    if (m.stream.sampleSize != 16) return;
    if (m.frameCount != 0 && L->loop_end > m.frameCount) return;  /* not the file we expect */

    f->ctx_type = m.ctxType;
    f->ctx      = m.ctxData;
    f->channels = (int)m.stream.channels;

    if (L->seam_frames > 0 && L->seam_path) {
        Wave w = seam_load_any(L->seam_path);
        if (w.data != NULL) {
            WaveFormat(&w, (int)m.stream.sampleRate, 16, (int)m.stream.channels);
            f->seam        = w;
            f->seam_frames = (w.frameCount < L->seam_frames) ? w.frameCount : L->seam_frames;
        } else {
            fprintf(stderr, "Warning: %s missing — that track loops without it\n", L->seam_path);
        }
    }

    feed_seek(f, 0);
    f->fed = 1;
    SetAudioStreamCallback(m.stream, s_callbacks[i]);
}

/* Restart a STOPPED track from its first frame. */
static void track_rewind(MusicManager *mgr, int i) {
    if (s_feed[i].fed) {
        s_feed[i].in_seam = 0;
        feed_seek(&s_feed[i], 0);
    } else {
        SeekMusicStream(mgr->tracks[i], 0.0f);
    }
}

static void track_play(MusicManager *mgr, int i) {
    if (s_feed[i].fed) PlayAudioStream(mgr->tracks[i].stream);
    else               PlayMusicStream(mgr->tracks[i]);
}

static void track_stop(MusicManager *mgr, int i) {
    /* StopMusicStream would also rewind the decoder from this thread while the
     * audio thread may still be inside music_fill; stop the stream only and
     * rewind when the track is started again. */
    if (s_feed[i].fed) StopAudioStream(mgr->tracks[i].stream);
    else               StopMusicStream(mgr->tracks[i]);
}

/* Load a music track, preferring the .ogg variant of the canonical .wav path.
 * The web and Android builds convert the 82 MB of WAV music to OGG at CI time
 * to keep the download sane (raylib decodes OGG natively via stb_vorbis);
 * desktop distributions ship the WAVs unchanged, so there the .ogg attempt
 * simply fails and we fall back.
 *
 * We ATTEMPT THE LOAD rather than probing with FileExists(): that helper is an
 * access() call, which is blind to assets packed inside an Android APK — it
 * would report the .ogg missing and send us to a .wav the conversion removed,
 * leaving the game silent. LoadMusicStream goes through raylib's asset layer
 * (android_fopen / the Emscripten MEMFS), so it sees what actually ships. */
static Music music_load_any(const char *wav_path) {
    size_t len = strlen(wav_path);
    char buf[512];

    if (len >= 4 && len < sizeof(buf) && strcmp(wav_path + len - 4, ".wav") == 0) {
        memcpy(buf, wav_path, len + 1);
        memcpy(buf + len - 4, ".ogg", 4);   /* same length — NUL kept */
        Music ogg = LoadMusicStream(buf);
        if (ogg.ctxData != NULL) return ogg;
    }
    return LoadMusicStream(wav_path);
}

void music_manager_init(MusicManager *mgr) {
    if (!mgr) return;

    memset(mgr, 0, sizeof(MusicManager));

    for (int i = 0; i < MUSIC_COUNT; i++) {
        mgr->tracks[i] = music_load_any(music_paths[i]);

        // Check if load succeeded
        if (mgr->tracks[i].ctxData != NULL) {
            mgr->tracks_loaded[i] = 1;
            SetMusicVolume(mgr->tracks[i], 32.0f / 64.0f);  /* FILE.ASM:815 default */
            feed_setup(mgr, i);
        } else {
            mgr->tracks_loaded[i] = 0;
            fprintf(stderr, "Warning: Failed to load %s (nor its .ogg variant)\n",
                    music_paths[i]);
        }
    }

    mgr->current = MUSIC_BLASTER;
    mgr->initialized = 1;
    mgr->music_volume = 32;   /* FILE.ASM:815 default */

    // Start with menu music if loaded
    if (mgr->tracks_loaded[MUSIC_BLASTER]) {
        track_play(mgr, MUSIC_BLASTER);
    }
}

void music_manager_set_volume(MusicManager *mgr, int volume) {
    if (!mgr || !mgr->initialized) return;
    if (volume < 0)  volume = 0;
    if (volume > 64) volume = 64;
    mgr->music_volume = volume;
    /* MAIN.ASM:676-677  mov user_volume,al / mov master_volume,al — the level
     * is continuous, not a mute toggle. */
    float vol = (float)volume / 64.0f;
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
        track_stop(mgr, mgr->current);
    }

    // Play new track from its start, as Load_Music does (MAIN.ASM:120-143)
    mgr->current = track;
    if (mgr->tracks_loaded[mgr->current]) {
        track_rewind(mgr, mgr->current);
        track_play(mgr, mgr->current);
    }
}

void music_manager_update(MusicManager *mgr) {
    if (!mgr || !mgr->initialized) return;
    /* Fed tracks are filled from the audio callback; calling UpdateMusicStream
     * on them would read the same decoder from this thread. */
    if (mgr->tracks_loaded[mgr->current] && !s_feed[mgr->current].fed) {
        UpdateMusicStream(mgr->tracks[mgr->current]);
    }
}

void music_manager_cleanup(MusicManager *mgr) {
    if (!mgr) return;

    for (int i = 0; i < MUSIC_COUNT; i++) {
        if (mgr->tracks_loaded[i]) {
            /* UnloadMusicStream unloads the stream (taking it out of the mixer
             * under raylib's lock) before closing the decoder it fed from. */
            UnloadMusicStream(mgr->tracks[i]);
            mgr->tracks_loaded[i] = 0;
        }
        if (s_feed[i].seam.data) UnloadWave(s_feed[i].seam);
        memset(&s_feed[i], 0, sizeof(s_feed[i]));
    }
    mgr->initialized = 0;
}
