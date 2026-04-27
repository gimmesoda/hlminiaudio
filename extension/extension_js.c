/*
 * extension_js.c — Emscripten build of the miniaudio HL extension for haxe/js target.
 *
 * Compile with emcc, e.g.:
 *
 *   emcc extension_js.c \
 *     -I path/to/miniaudio \
 *     -I path/to/miniaudio/extras/decoders/libopus \
 *     -I path/to/miniaudio/extras/decoders/libvorbis \
 *     -s WASM=1 \
 *     -s EXPORTED_FUNCTIONS="['_ma_js_init','_ma_js_uninit','_ma_js_describe_last_error', \
 *       '_ma_js_buffer_dispose','_ma_js_buffer_from_bytes', \
 *       '_ma_js_buffer_from_pcm_float','_ma_js_buffer_from_pcm_s16', \
 *       '_ma_js_buffer_get_length_samples','_ma_js_buffer_get_duration','_ma_js_buffer_get_duration_seconds', \
 *       '_ma_js_decode_pcm_float','_ma_js_decode_pcm_s16', \
 *       '_ma_js_decoded_channels','_ma_js_decoded_sample_rate','_ma_js_decoded_samples', \
 *       '_ma_js_sound_group_init','_ma_js_sound_group_dispose', \
 *       '_ma_js_sound_group_start','_ma_js_sound_group_stop', \
 *       '_ma_js_sound_group_get_volume','_ma_js_sound_group_set_volume', \
 *       '_ma_js_sound_group_get_pan','_ma_js_sound_group_set_pan', \
 *       '_ma_js_sound_group_get_pan_mode','_ma_js_sound_group_set_pan_mode', \
 *       '_ma_js_sound_group_get_pitch','_ma_js_sound_group_set_pitch', \
 *       '_ma_js_sound_group_get_spatialization_enabled','_ma_js_sound_group_set_spatialization_enabled', \
 *       '_ma_js_sound_init','_ma_js_sound_dispose', \
 *       '_ma_js_sound_start','_ma_js_sound_stop', \
 *       '_ma_js_sound_seek_samples','_ma_js_sound_seek_seconds','_ma_js_sound_seek_milliseconds', \
 *       '_ma_js_sound_get_cursor_samples', \
 *       '_ma_js_sound_is_playing', \
 *       '_ma_js_sound_get_volume','_ma_js_sound_set_volume', \
 *       '_ma_js_sound_get_pan','_ma_js_sound_set_pan', \
 *       '_ma_js_sound_get_pan_mode','_ma_js_sound_set_pan_mode', \
 *       '_ma_js_sound_get_pitch','_ma_js_sound_set_pitch', \
 *       '_ma_js_sound_get_spatialization_enabled','_ma_js_sound_set_spatialization_enabled', \
 *       '_ma_js_sound_get_time','_ma_js_sound_set_time', \
 *       '_ma_js_sound_get_time_seconds','_ma_js_sound_set_time_seconds', \
 *       '_ma_js_sound_get_duration','_ma_js_sound_get_duration_seconds', \
 *       '_ma_js_sound_get_length_samples', \
 *       '_ma_js_sound_set_end_callback_js','_ma_js_sound_clear_end_callback', \
 *       '_ma_js_update', \
 *       '_malloc','_free']" \
 *     -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap','getValue','UTF8ToString','HEAPF32','HEAP16','HEAP32']" \
 *     -s ALLOW_MEMORY_GROWTH=1 \
 *     -s MODULARIZE=1 \
 *     -s EXPORT_NAME="MiniaudioModule" \
 *     -o miniaudio.js
 *
 * NOTE: vclosure callbacks are not portable to JS; sound_set_end_callback uses a JS
 * callback table instead (see ma_js_sound_set_end_callback_js below).
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define MA_JS_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define MA_JS_EXPORT
#endif

#ifdef _GUID
#undef _GUID
#endif

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_VORBIS
#include <miniaudio.h>

#include "extras/decoders/libopus/miniaudio_libopus.h"
#include "extras/decoders/libvorbis/miniaudio_libvorbis.h"

 /* ---- global state ---- */

static ma_engine engine;
static ma_result lastResult;
static int lastDecodedChannels, lastDecodedSampleRate, lastDecodedSamples;

/* ---- JS end-callback table ---- */
/*
 * In JS we cannot pass C function pointers as closures.
 * Instead we store integer "callback IDs" and call back into JS via
 * EM_ASM / emscripten_run_script when a sound ends.
 */

typedef struct sound_js_cb_entry {
    ma_sound* sound;
    int callbackId;          /* 0 = no callback */
    volatile int pending;
    struct sound_js_cb_entry* next;
} sound_js_cb_entry;

static sound_js_cb_entry* js_callbacks = NULL;

/* ---- memory stream (same as original) ---- */

typedef struct {
    const unsigned char* data;
    size_t size;
    size_t cursor;
} memory_stream;

static ma_result memory_stream_read(void* ud, void* out, size_t n, size_t* read)
{
    memory_stream* s = (memory_stream*)ud;
    if (read) *read = 0;
    if (!s || !out) return MA_INVALID_ARGS;
    size_t rem = s->size - s->cursor;
    size_t copy = n < rem ? n : rem;
    if (copy) { memcpy(out, s->data + s->cursor, copy); s->cursor += copy; }
    if (read) *read = copy;
    return copy == 0 ? MA_AT_END : MA_SUCCESS;
}

static ma_result memory_stream_seek(void* ud, ma_int64 off, ma_seek_origin origin)
{
    memory_stream* s = (memory_stream*)ud;
    if (!s) return MA_INVALID_ARGS;
    size_t base = 0;
    switch (origin) {
    case ma_seek_origin_start:   base = 0; break;
    case ma_seek_origin_current: base = s->cursor; break;
    case ma_seek_origin_end:     base = s->size; break;
    default: return MA_INVALID_ARGS;
    }
    ma_int64 t = (ma_int64)base + off;
    if (t < 0 || (ma_uint64)t > s->size) return MA_INVALID_ARGS;
    s->cursor = (size_t)t;
    return MA_SUCCESS;
}

static ma_result memory_stream_tell(void* ud, ma_int64* cursor)
{
    memory_stream* s = (memory_stream*)ud;
    if (!s || !cursor) return MA_INVALID_ARGS;
    *cursor = (ma_int64)s->cursor;
    return MA_SUCCESS;
}

/* ---- helpers ---- */

static void set_last_decoded_format(ma_uint32 channels, ma_uint32 sampleRate, ma_uint64 frameCount)
{
    lastDecodedChannels = (int)channels;
    lastDecodedSampleRate = (int)sampleRate;
    lastDecodedSamples = (int)frameCount;
}

static int is_ogg_stream(const unsigned char* b, size_t sz)
{
    return sz >= 4 && memcmp(b, "OggS", 4) == 0;
}

static int read_ogg_packet_signature(const unsigned char* b, size_t sz, const char* sig, size_t sigSz)
{
    if (!is_ogg_stream(b, sz) || sz < 27) return 0;
    size_t segSz = b[26];
    size_t pktOff = 27 + segSz;
    if (pktOff + sigSz > sz) return 0;
    return memcmp(b + pktOff, sig, sigSz) == 0;
}

static int is_opus_stream(const unsigned char* b, size_t sz)
{
    return read_ogg_packet_signature(b, sz, "OpusHead", 8);
}

static int is_vorbis_stream(const unsigned char* b, size_t sz)
{
    static const unsigned char sig[] = { 0x01, 'v', 'o', 'r', 'b', 'i', 's' };
    return read_ogg_packet_signature(b, sz, (const char*)sig, sizeof(sig));
}

/* Returns a malloc'd float* buffer of frameCount*channels floats decoded from vorbis. */
static float* decode_vorbis_float(const unsigned char* bytes, size_t size,
    ma_uint32* out_ch, ma_uint32* out_sr, ma_uint64* out_frames)
{
    memory_stream stream = { bytes, size, 0 };
    ma_libvorbis dec;
    ma_decoding_backend_config cfg = ma_decoding_backend_config_init(ma_format_f32, 0);
    lastResult = ma_libvorbis_init(memory_stream_read, memory_stream_seek, memory_stream_tell,
        &stream, &cfg, NULL, &dec);
    if (lastResult != MA_SUCCESS) return NULL;

    ma_uint32 ch = 0, sr = 0;
    ma_uint64 frames = 0;
    lastResult = ma_libvorbis_get_data_format(&dec, NULL, &ch, &sr, NULL, 0);
    if (lastResult != MA_SUCCESS) { ma_libvorbis_uninit(&dec, NULL); return NULL; }
    lastResult = ma_libvorbis_get_length_in_pcm_frames(&dec, &frames);
    if (lastResult != MA_SUCCESS || frames == 0) {
        ma_libvorbis_uninit(&dec, NULL); lastResult = MA_INVALID_FILE; return NULL;
    }

    float* data = (float*)ma_malloc((size_t)(frames * ch * sizeof(float)), NULL);
    if (!data) { ma_libvorbis_uninit(&dec, NULL); lastResult = MA_OUT_OF_MEMORY; return NULL; }

    ma_uint64 read = 0;
    lastResult = ma_libvorbis_read_pcm_frames(&dec, data, frames, &read);
    ma_libvorbis_uninit(&dec, NULL);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || read == 0) {
        ma_free(data, NULL); return NULL;
    }

    *out_ch = ch; *out_sr = sr; *out_frames = read;
    return data;
}

static float* decode_opus_float(const unsigned char* bytes, size_t size,
    ma_uint32* out_ch, ma_uint32* out_sr, ma_uint64* out_frames)
{
    memory_stream stream = { bytes, size, 0 };
    ma_libopus dec;
    ma_decoding_backend_config cfg = ma_decoding_backend_config_init(ma_format_f32, 0);
    lastResult = ma_libopus_init(memory_stream_read, memory_stream_seek, memory_stream_tell,
        &stream, &cfg, NULL, &dec);
    if (lastResult != MA_SUCCESS) return NULL;

    ma_uint32 ch = 0, sr = 0;
    ma_uint64 frames = 0;
    lastResult = ma_libopus_get_data_format(&dec, NULL, &ch, &sr, NULL, 0);
    if (lastResult != MA_SUCCESS) { ma_libopus_uninit(&dec, NULL); return NULL; }
    lastResult = ma_libopus_get_length_in_pcm_frames(&dec, &frames);
    if (lastResult != MA_SUCCESS || frames == 0) {
        ma_libopus_uninit(&dec, NULL); lastResult = MA_INVALID_FILE; return NULL;
    }

    float* data = (float*)ma_malloc((size_t)(frames * ch * sizeof(float)), NULL);
    if (!data) { ma_libopus_uninit(&dec, NULL); lastResult = MA_OUT_OF_MEMORY; return NULL; }

    ma_uint64 read = 0;
    lastResult = ma_libopus_read_pcm_frames(&dec, data, frames, &read);
    ma_libopus_uninit(&dec, NULL);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || read == 0) {
        ma_free(data, NULL); return NULL;
    }

    *out_ch = ch; *out_sr = sr; *out_frames = read;
    return data;
}

/* ---- JS callback helpers ---- */

static sound_js_cb_entry* js_cb_get(ma_sound* sound, int create)
{
    sound_js_cb_entry* e = js_callbacks;
    while (e) { if (e->sound == sound) return e; e = e->next; }
    if (!create) return NULL;
    e = (sound_js_cb_entry*)calloc(1, sizeof(sound_js_cb_entry));
    e->sound = sound;
    e->next = js_callbacks;
    js_callbacks = e;
    return e;
}

static void js_sound_end_cb(void* ud, ma_sound* snd)
{
    sound_js_cb_entry* e = (sound_js_cb_entry*)ud;
    if (e && e->sound == snd && e->callbackId != 0)
        e->pending = 1;
}

/* ============================================================
 * PUBLIC API — all functions exported with MA_JS_EXPORT
 * These are thin wrappers that replace HL primitives.
 * Pointer types become int (WASM linear memory address).
 * Strings are returned as const char* (UTF-8 in WASM memory).
 * ============================================================ */

 /* ----- engine ----- */

MA_JS_EXPORT int ma_js_init()
{
    lastResult = ma_engine_init(NULL, &engine);
    return lastResult == MA_SUCCESS ? 1 : 0;
}

MA_JS_EXPORT void ma_js_uninit()
{
    ma_engine_uninit(&engine);
}

MA_JS_EXPORT const char* ma_js_describe_last_error()
{
    return ma_result_description(lastResult);
}

/* ----- decode raw PCM (returns pointer into WASM heap; caller must free) ----- */

/*
 * Decodes audio bytes to interleaved float32 PCM.
 * Returns a pointer to a malloc'd buffer (size = decoded_samples * decoded_channels * 4 bytes).
 * Returns 0 on failure.
 * After call: ma_js_decoded_channels / _sample_rate / _samples give format info.
 */
MA_JS_EXPORT int ma_js_decode_pcm_float(const unsigned char* bytes, int size)
{
    if (!bytes || size <= 0) { lastResult = MA_INVALID_ARGS; return 0; }

    ma_uint32 ch = 0, sr = 0;
    ma_uint64 frames = 0;
    float* data = NULL;

    if (is_opus_stream(bytes, (size_t)size)) {
        data = decode_opus_float(bytes, (size_t)size, &ch, &sr, &frames);
    }
    else if (is_vorbis_stream(bytes, (size_t)size)) {
        data = decode_vorbis_float(bytes, (size_t)size, &ch, &sr, &frames);
    }
    else {
        ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
        ma_decoder dec;
        lastResult = ma_decoder_init_memory(bytes, (size_t)size, &cfg, &dec);
        if (lastResult != MA_SUCCESS) return 0;
        ch = dec.outputChannels;
        sr = dec.outputSampleRate;
        ma_uint64 frameCount = 0;
        lastResult = ma_decoder_get_length_in_pcm_frames(&dec, &frameCount);
        if (lastResult != MA_SUCCESS || frameCount == 0) {
            ma_decoder_uninit(&dec); lastResult = MA_INVALID_FILE; return 0;
        }
        data = (float*)ma_malloc((size_t)(frameCount * ch * sizeof(float)), NULL);
        if (!data) { ma_decoder_uninit(&dec); lastResult = MA_OUT_OF_MEMORY; return 0; }
        lastResult = ma_decoder_read_pcm_frames(&dec, data, frameCount, &frames);
        ma_decoder_uninit(&dec);
        if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || frames == 0) {
            ma_free(data, NULL); return 0;
        }
    }

    if (!data) return 0;
    set_last_decoded_format(ch, sr, frames);
    return (int)(intptr_t)data;
}

/*
 * Decodes audio bytes to interleaved int16 PCM.
 * Returns pointer to malloc'd buffer (size = decoded_samples * decoded_channels * 2 bytes).
 */
MA_JS_EXPORT int ma_js_decode_pcm_s16(const unsigned char* bytes, int size)
{
    if (!bytes || size <= 0) { lastResult = MA_INVALID_ARGS; return 0; }

    /* For opus/vorbis decode as float then convert */
    ma_uint32 ch = 0, sr = 0;
    ma_uint64 frames = 0;
    ma_int16* out = NULL;

    if (is_opus_stream(bytes, (size_t)size) || is_vorbis_stream(bytes, (size_t)size)) {
        float* fdata = is_opus_stream(bytes, (size_t)size)
            ? decode_opus_float(bytes, (size_t)size, &ch, &sr, &frames)
            : decode_vorbis_float(bytes, (size_t)size, &ch, &sr, &frames);
        if (!fdata) return 0;
        size_t n = (size_t)(frames * ch);
        out = (ma_int16*)ma_malloc(n * sizeof(ma_int16), NULL);
        if (!out) { ma_free(fdata, NULL); lastResult = MA_OUT_OF_MEMORY; return 0; }
        ma_pcm_f32_to_s16(out, fdata, n, ma_dither_mode_none);
        ma_free(fdata, NULL);
    }
    else {
        ma_decoder_config cfg = ma_decoder_config_init(ma_format_s16, 0, 0);
        ma_decoder dec;
        lastResult = ma_decoder_init_memory(bytes, (size_t)size, &cfg, &dec);
        if (lastResult != MA_SUCCESS) return 0;
        ch = dec.outputChannels;
        sr = dec.outputSampleRate;
        ma_uint64 frameCount = 0;
        lastResult = ma_decoder_get_length_in_pcm_frames(&dec, &frameCount);
        if (lastResult != MA_SUCCESS || frameCount == 0) {
            ma_decoder_uninit(&dec); lastResult = MA_INVALID_FILE; return 0;
        }
        out = (ma_int16*)ma_malloc((size_t)(frameCount * ch * sizeof(ma_int16)), NULL);
        if (!out) { ma_decoder_uninit(&dec); lastResult = MA_OUT_OF_MEMORY; return 0; }
        lastResult = ma_decoder_read_pcm_frames(&dec, out, frameCount, &frames);
        ma_decoder_uninit(&dec);
        if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || frames == 0) {
            ma_free(out, NULL); return 0;
        }
    }

    set_last_decoded_format(ch, sr, frames);
    return (int)(intptr_t)out;
}

MA_JS_EXPORT int ma_js_decoded_channels() { return lastDecodedChannels; }
MA_JS_EXPORT int ma_js_decoded_sample_rate() { return lastDecodedSampleRate; }
MA_JS_EXPORT int ma_js_decoded_samples() { return lastDecodedSamples; }

/* ----- buffer ----- */

MA_JS_EXPORT void ma_js_buffer_dispose(ma_audio_buffer* buf)
{
    ma_audio_buffer_uninit_and_free(buf);
}

/*
 * Creates a buffer from raw audio bytes (any supported format).
 * Returns pointer to ma_audio_buffer, or 0 on failure.
 */
MA_JS_EXPORT int ma_js_buffer_from_bytes(const unsigned char* bytes, int size)
{
    if (!bytes || size <= 0) { lastResult = MA_INVALID_ARGS; return 0; }

    ma_uint32 ch = 0, sr = 0;
    ma_uint64 frames = 0;
    float* data = NULL;

    if (is_opus_stream(bytes, (size_t)size)) {
        data = decode_opus_float(bytes, (size_t)size, &ch, &sr, &frames);
    }
    else if (is_vorbis_stream(bytes, (size_t)size)) {
        data = decode_vorbis_float(bytes, (size_t)size, &ch, &sr, &frames);
    }
    else {
        ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
        ma_decoder dec;
        lastResult = ma_decoder_init_memory(bytes, (size_t)size, &cfg, &dec);
        if (lastResult != MA_SUCCESS) return 0;
        ch = dec.outputChannels;
        sr = dec.outputSampleRate;
        ma_uint64 frameCount = 0;
        lastResult = ma_decoder_get_length_in_pcm_frames(&dec, &frameCount);
        if (lastResult != MA_SUCCESS || frameCount == 0) {
            ma_decoder_uninit(&dec); lastResult = MA_INVALID_FILE; return 0;
        }
        data = (float*)ma_malloc((size_t)(frameCount * ch * sizeof(float)), NULL);
        if (!data) { ma_decoder_uninit(&dec); lastResult = MA_OUT_OF_MEMORY; return 0; }
        lastResult = ma_decoder_read_pcm_frames(&dec, data, frameCount, &frames);
        ma_decoder_uninit(&dec);
        if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || frames == 0) {
            ma_free(data, NULL); return 0;
        }
    }

    if (!data) return 0;
    set_last_decoded_format(ch, sr, frames);

    ma_audio_buffer_config bcfg = ma_audio_buffer_config_init(ma_format_f32, ch, frames, data, NULL);
    ma_audio_buffer* buf = NULL;
    lastResult = ma_audio_buffer_alloc_and_init(&bcfg, &buf);
    if (lastResult == MA_SUCCESS) {
        buf->ref.sampleRate = sr;
        return (int)(intptr_t)buf;
    }
    ma_free(data, NULL);
    return 0;
}

MA_JS_EXPORT int ma_js_buffer_from_pcm_float(const float* data, int size, int channels, int sampleRate)
{
    if (!data || size <= 0 || channels <= 0 || sampleRate <= 0) {
        lastResult = MA_INVALID_ARGS; return 0;
    }
    int frameSize = channels * (int)sizeof(float);
    if ((size % frameSize) != 0) { lastResult = MA_INVALID_ARGS; return 0; }
    ma_uint64 frameCount = (ma_uint64)(size / frameSize);

    float* copy = (float*)ma_malloc((size_t)size, NULL);
    if (!copy) { lastResult = MA_OUT_OF_MEMORY; return 0; }
    memcpy(copy, data, (size_t)size);

    ma_audio_buffer_config bcfg = ma_audio_buffer_config_init(ma_format_f32, (ma_uint32)channels, frameCount, copy, NULL);
    ma_audio_buffer* buf = NULL;
    lastResult = ma_audio_buffer_alloc_and_init(&bcfg, &buf);
    if (lastResult == MA_SUCCESS) {
        buf->ref.sampleRate = (ma_uint32)sampleRate;
        return (int)(intptr_t)buf;
    }
    ma_free(copy, NULL);
    return 0;
}

MA_JS_EXPORT int ma_js_buffer_from_pcm_s16(const ma_int16* data, int size, int channels, int sampleRate)
{
    if (!data || size <= 0 || channels <= 0 || sampleRate <= 0) {
        lastResult = MA_INVALID_ARGS; return 0;
    }
    int frameSize = channels * (int)sizeof(ma_int16);
    if ((size % frameSize) != 0) { lastResult = MA_INVALID_ARGS; return 0; }
    ma_uint64 frameCount = (ma_uint64)(size / frameSize);

    ma_int16* copy = (ma_int16*)ma_malloc((size_t)size, NULL);
    if (!copy) { lastResult = MA_OUT_OF_MEMORY; return 0; }
    memcpy(copy, data, (size_t)size);

    ma_audio_buffer_config bcfg = ma_audio_buffer_config_init(ma_format_s16, (ma_uint32)channels, frameCount, copy, NULL);
    ma_audio_buffer* buf = NULL;
    lastResult = ma_audio_buffer_alloc_and_init(&bcfg, &buf);
    if (lastResult == MA_SUCCESS) {
        buf->ref.sampleRate = (ma_uint32)sampleRate;
        return (int)(intptr_t)buf;
    }
    ma_free(copy, NULL);
    return 0;
}

MA_JS_EXPORT int ma_js_buffer_get_length_samples(ma_audio_buffer* buf)
{
    ma_uint64 len = 0;
    lastResult = ma_audio_buffer_get_length_in_pcm_frames(buf, &len);
    return lastResult == MA_SUCCESS ? (int)len : 0;
}

MA_JS_EXPORT double ma_js_buffer_get_duration(ma_audio_buffer* buf)
{
    ma_uint64 len = 0;
    lastResult = ma_audio_buffer_get_length_in_pcm_frames(buf, &len);
    if (lastResult != MA_SUCCESS || buf->ref.sampleRate == 0) return 0;
    return ((double)len * 1000.0) / (double)buf->ref.sampleRate;
}

MA_JS_EXPORT double ma_js_buffer_get_duration_seconds(ma_audio_buffer* buf)
{
    ma_uint64 len = 0;
    lastResult = ma_audio_buffer_get_length_in_pcm_frames(buf, &len);
    if (lastResult != MA_SUCCESS || buf->ref.sampleRate == 0) return 0;
    return (double)len / (double)buf->ref.sampleRate;
}

/* ----- sound group ----- */

MA_JS_EXPORT int ma_js_sound_group_init(ma_sound_group* parent)
{
    ma_sound_group* g = (ma_sound_group*)ma_malloc(sizeof(ma_sound_group), NULL);
    lastResult = ma_sound_group_init(&engine, 0, parent, g);
    if (lastResult == MA_SUCCESS) return (int)(intptr_t)g;
    ma_sound_group_uninit(g);
    ma_free(g, NULL);
    return 0;
}

MA_JS_EXPORT void ma_js_sound_group_dispose(ma_sound_group* g)
{
    ma_sound_group_uninit(g);
    ma_free(g, NULL);
}

MA_JS_EXPORT int    ma_js_sound_group_start(ma_sound_group* g) { lastResult = ma_sound_group_start(g); return lastResult == MA_SUCCESS; }
MA_JS_EXPORT int    ma_js_sound_group_stop(ma_sound_group* g) { lastResult = ma_sound_group_stop(g);  return lastResult == MA_SUCCESS; }
MA_JS_EXPORT double ma_js_sound_group_get_volume(ma_sound_group* g) { return ma_sound_group_get_volume(g); }
MA_JS_EXPORT double ma_js_sound_group_set_volume(ma_sound_group* g, double v) { ma_sound_group_set_volume(g, (float)v); return v; }
MA_JS_EXPORT double ma_js_sound_group_get_pan(ma_sound_group* g) { return ma_sound_group_get_pan(g); }
MA_JS_EXPORT double ma_js_sound_group_set_pan(ma_sound_group* g, double v) { ma_sound_group_set_pan(g, (float)v); return v; }
MA_JS_EXPORT int    ma_js_sound_group_get_pan_mode(ma_sound_group* g) { return (int)ma_sound_group_get_pan_mode(g); }
MA_JS_EXPORT int    ma_js_sound_group_set_pan_mode(ma_sound_group* g, int m) { ma_sound_group_set_pan_mode(g, (ma_pan_mode)m); return m; }
MA_JS_EXPORT double ma_js_sound_group_get_pitch(ma_sound_group* g) { return ma_sound_group_get_pitch(g); }
MA_JS_EXPORT double ma_js_sound_group_set_pitch(ma_sound_group* g, double v) { ma_sound_group_set_pitch(g, (float)v); return v; }
MA_JS_EXPORT int    ma_js_sound_group_get_spatialization_enabled(ma_sound_group* g) { return ma_sound_group_is_spatialization_enabled(g) == MA_TRUE; }
MA_JS_EXPORT int    ma_js_sound_group_set_spatialization_enabled(ma_sound_group* g, int e) { ma_sound_group_set_spatialization_enabled(g, e ? MA_TRUE : MA_FALSE); return e; }

/* ----- sound ----- */

MA_JS_EXPORT int ma_js_sound_init(ma_audio_buffer* buf, ma_sound_group* parent)
{
    ma_sound* snd = (ma_sound*)ma_malloc(sizeof(ma_sound), NULL);
    lastResult = ma_sound_init_from_data_source(&engine, buf, 0, parent, snd);
    if (lastResult == MA_SUCCESS) return (int)(intptr_t)snd;
    ma_sound_uninit(snd);
    ma_free(snd, NULL);
    return 0;
}

MA_JS_EXPORT void ma_js_sound_dispose(ma_sound* snd)
{
    /* remove from callback list */
    sound_js_cb_entry** ep = &js_callbacks;
    while (*ep) {
        if ((*ep)->sound == snd) {
            sound_js_cb_entry* cur = *ep;
            *ep = cur->next;
            free(cur);
            break;
        }
        ep = &(*ep)->next;
    }
    ma_sound_set_end_callback(snd, NULL, NULL);
    ma_sound_uninit(snd);
    ma_free(snd, NULL);
}

MA_JS_EXPORT int    ma_js_sound_start(ma_sound* snd) { lastResult = ma_sound_start(snd);  return lastResult == MA_SUCCESS; }
MA_JS_EXPORT int    ma_js_sound_stop(ma_sound* snd) { lastResult = ma_sound_stop(snd);   return lastResult == MA_SUCCESS; }
MA_JS_EXPORT int    ma_js_sound_is_playing(ma_sound* snd) { return ma_sound_is_playing(snd) == MA_TRUE; }

MA_JS_EXPORT int ma_js_sound_seek_samples(ma_sound* snd, int sample)
{
    if (sample < 0) sample = 0;
    lastResult = ma_sound_seek_to_pcm_frame(snd, (ma_uint64)sample);
    return lastResult == MA_SUCCESS ? sample : -1;
}

MA_JS_EXPORT double ma_js_sound_seek_seconds(ma_sound* snd, double s)
{
    if (s < 0) s = 0;
    lastResult = ma_sound_seek_to_second(snd, (float)s);
    return lastResult == MA_SUCCESS ? s : -1;
}

MA_JS_EXPORT double ma_js_sound_seek_milliseconds(ma_sound* snd, double ms)
{
    if (ms < 0) ms = 0;
    lastResult = ma_sound_seek_to_second(snd, (float)(ms / 1000.0));
    return lastResult == MA_SUCCESS ? ms : -1;
}

MA_JS_EXPORT int ma_js_sound_get_cursor_samples(ma_sound* snd)
{
    ma_uint64 c = 0;
    lastResult = ma_sound_get_cursor_in_pcm_frames(snd, &c);
    return lastResult == MA_SUCCESS ? (int)c : 0;
}

MA_JS_EXPORT double ma_js_sound_get_volume(ma_sound* snd) { return ma_sound_get_volume(snd); }
MA_JS_EXPORT double ma_js_sound_set_volume(ma_sound* snd, double v) { ma_sound_set_volume(snd, (float)v); return v; }
MA_JS_EXPORT double ma_js_sound_get_pan(ma_sound* snd) { return ma_sound_get_pan(snd); }
MA_JS_EXPORT double ma_js_sound_set_pan(ma_sound* snd, double v) { ma_sound_set_pan(snd, (float)v); return v; }
MA_JS_EXPORT int    ma_js_sound_get_pan_mode(ma_sound* snd) { return (int)ma_sound_get_pan_mode(snd); }
MA_JS_EXPORT int    ma_js_sound_set_pan_mode(ma_sound* snd, int m) { ma_sound_set_pan_mode(snd, (ma_pan_mode)m); return m; }
MA_JS_EXPORT double ma_js_sound_get_pitch(ma_sound* snd) { return ma_sound_get_pitch(snd); }
MA_JS_EXPORT double ma_js_sound_set_pitch(ma_sound* snd, double v) { ma_sound_set_pitch(snd, (float)v); return v; }
MA_JS_EXPORT int    ma_js_sound_get_spatialization_enabled(ma_sound* snd) { return ma_sound_is_spatialization_enabled(snd) == MA_TRUE; }
MA_JS_EXPORT int    ma_js_sound_set_spatialization_enabled(ma_sound* snd, int e) { ma_sound_set_spatialization_enabled(snd, e ? MA_TRUE : MA_FALSE); return e; }

MA_JS_EXPORT double ma_js_sound_get_time(ma_sound* snd)
{
    ma_uint64 c = 0; ma_uint32 sr = 0;
    lastResult = ma_sound_get_cursor_in_pcm_frames(snd, &c);
    if (lastResult != MA_SUCCESS) return 0;
    lastResult = ma_sound_get_data_format(snd, NULL, NULL, &sr, NULL, 0);
    if (lastResult != MA_SUCCESS || sr == 0) return 0;
    return ((double)c * 1000.0) / (double)sr;
}

MA_JS_EXPORT double ma_js_sound_set_time(ma_sound* snd, double ms)
{
    return ma_js_sound_seek_milliseconds(snd, ms);
}

MA_JS_EXPORT double ma_js_sound_get_time_seconds(ma_sound* snd)
{
    ma_uint64 c = 0; ma_uint32 sr = 0;
    lastResult = ma_sound_get_cursor_in_pcm_frames(snd, &c);
    if (lastResult != MA_SUCCESS) return 0;
    lastResult = ma_sound_get_data_format(snd, NULL, NULL, &sr, NULL, 0);
    if (lastResult != MA_SUCCESS || sr == 0) return 0;
    return (double)c / (double)sr;
}

MA_JS_EXPORT double ma_js_sound_set_time_seconds(ma_sound* snd, double s)
{
    return ma_js_sound_seek_seconds(snd, s);
}

MA_JS_EXPORT double ma_js_sound_get_duration(ma_sound* snd)
{
    float len = 0;
    lastResult = ma_sound_get_length_in_seconds(snd, &len);
    return lastResult == MA_SUCCESS ? (double)len * 1000.0 : 0;
}

MA_JS_EXPORT double ma_js_sound_get_duration_seconds(ma_sound* snd)
{
    float len = 0;
    lastResult = ma_sound_get_length_in_seconds(snd, &len);
    return lastResult == MA_SUCCESS ? (double)len : 0;
}

MA_JS_EXPORT int ma_js_sound_get_length_samples(ma_sound* snd)
{
    ma_uint64 len = 0;
    lastResult = ma_sound_get_length_in_pcm_frames(snd, &len);
    return lastResult == MA_SUCCESS ? (int)len : 0;
}

/*
 * Set end callback via integer callbackId.
 * In Haxe/JS, maintain a Map<Int,()->Void> and dispatch from ma_js_update().
 * callbackId == 0 clears the callback.
 */
MA_JS_EXPORT void ma_js_sound_set_end_callback_js(ma_sound* snd, int callbackId)
{
    sound_js_cb_entry* e = js_cb_get(snd, callbackId != 0);
    if (callbackId == 0) {
        if (e) { e->callbackId = 0; e->pending = 0; }
        ma_sound_set_end_callback(snd, NULL, NULL);
        return;
    }
    e->callbackId = callbackId;
    e->pending = 0;
    ma_sound_set_end_callback(snd, js_sound_end_cb, e);
}

MA_JS_EXPORT void ma_js_sound_clear_end_callback(ma_sound* snd)
{
    ma_js_sound_set_end_callback_js(snd, 0);
}

/*
 * Call this every frame from Haxe/JS (e.g. in your game loop).
 * Returns number of fired callbacks.
 * For each fired callback, returns the callbackId via the provided int[] outIds buffer.
 * outIds must have room for at least maxIds ints.
 */
MA_JS_EXPORT int ma_js_update(int* outIds, int maxIds)
{
    sound_js_cb_entry* e = js_callbacks;
    int n = 0;
    while (e && n < maxIds) {
        if (e->pending && e->callbackId != 0) {
            outIds[n++] = e->callbackId;
            e->pending = 0;
        }
        e = e->next;
    }
    return n;
}
