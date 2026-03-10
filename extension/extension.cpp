#define HL_NAME(n) miniaudio_##n
#include <hl.h>
#include <string.h>

#ifdef _GUID
#undef _GUID
#endif

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_VORBIS
#include <miniaudio.h>

#include "extras/decoders/libvorbis/miniaudio_libvorbis.h"

#define _BUFFER _ABSTRACT(ma_audio_buffer*)
#define _SOUND _ABSTRACT(ma_sound*)
#define _GROUP _ABSTRACT(ma_sound_group*)

ma_engine engine;
ma_result lastResult;
int lastDecodedChannels, lastDecodedSampleRate, lastDecodedSamples;

typedef struct
{
    const unsigned char* data;
    size_t size;
    size_t cursor;
} memory_stream;

HL_PRIM ma_audio_buffer* HL_NAME(buffer_from_pcm_float)(vbyte* bytes, int size, int channels, int sampleRate);

static ma_result memory_stream_read(void* userData, void* bufferOut, size_t bytesToRead, size_t* bytesRead)
{
    memory_stream* stream = (memory_stream*)userData;
    size_t remaining;
    size_t bytesToCopy;

    if (bytesRead != NULL)
        *bytesRead = 0;

    if (stream == NULL || bufferOut == NULL)
        return MA_INVALID_ARGS;

    remaining = stream->size - stream->cursor;
    bytesToCopy = bytesToRead;
    if (bytesToCopy > remaining)
        bytesToCopy = remaining;

    if (bytesToCopy > 0)
    {
        memcpy(bufferOut, stream->data + stream->cursor, bytesToCopy);
        stream->cursor += bytesToCopy;
    }

    if (bytesRead != NULL)
        *bytesRead = bytesToCopy;

    return bytesToCopy == 0 ? MA_AT_END : MA_SUCCESS;
}

static ma_result memory_stream_seek(void* userData, ma_int64 byteOffset, ma_seek_origin origin)
{
    memory_stream* stream = (memory_stream*)userData;
    size_t base = 0;
    ma_int64 target;

    if (stream == NULL)
        return MA_INVALID_ARGS;

    switch (origin)
    {
        case ma_seek_origin_start:
            base = 0;
            break;
        case ma_seek_origin_current:
            base = stream->cursor;
            break;
        case ma_seek_origin_end:
            base = stream->size;
            break;
        default:
            return MA_INVALID_ARGS;
    }

    target = (ma_int64)base + byteOffset;
    if (target < 0 || (ma_uint64)target > stream->size)
        return MA_INVALID_ARGS;

    stream->cursor = (size_t)target;
    return MA_SUCCESS;
}

static ma_result memory_stream_tell(void* userData, ma_int64* cursor)
{
    memory_stream* stream = (memory_stream*)userData;
    if (stream == NULL || cursor == NULL)
        return MA_INVALID_ARGS;

    *cursor = (ma_int64)stream->cursor;
    return MA_SUCCESS;
}

static ma_audio_buffer* create_buffer_from_pcm(float* data, ma_uint64 frameCount, ma_uint32 channels, ma_uint32 sampleRate)
{
    ma_audio_buffer_config bufferConfig;
    ma_audio_buffer* buffer = NULL;

    bufferConfig = ma_audio_buffer_config_init(ma_format_f32, channels, frameCount, data, NULL);
    lastResult = ma_audio_buffer_alloc_and_init(&bufferConfig, &buffer);
    if (lastResult == MA_SUCCESS)
    {
        buffer->ref.sampleRate = sampleRate;
        return buffer;
    }

    ma_free(data, NULL);
    return nullptr;
}

static void set_last_decoded_format(ma_uint32 channels, ma_uint32 sampleRate, ma_uint64 frameCount)
{
    lastDecodedChannels = (int)channels;
    lastDecodedSampleRate = (int)sampleRate;
    lastDecodedSamples = (int)frameCount;
}

static vbyte* copy_pcm_float_bytes(const float* data, ma_uint64 frameCount, ma_uint32 channels)
{
    ma_uint64 byteCount64 = frameCount * channels * (ma_uint64)sizeof(float);
    int byteCount;

    if (byteCount64 == 0 || byteCount64 > 0x7FFFFFFF)
    {
        lastResult = MA_OUT_OF_MEMORY;
        return nullptr;
    }

    byteCount = (int)byteCount64;
    return hl_copy_bytes((const vbyte*)data, byteCount);
}

static vbyte* decode_vorbis_to_pcm_float(const unsigned char* bytes, size_t size)
{
    memory_stream stream;
    ma_libvorbis decoder;
    ma_decoding_backend_config config;
    ma_uint64 frameCount = 0;
    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
    float* data;
    ma_uint64 framesRead = 0;
    vbyte* result;

    stream.data = bytes;
    stream.size = size;
    stream.cursor = 0;

    config = ma_decoding_backend_config_init(ma_format_f32, 0);
    lastResult = ma_libvorbis_init(memory_stream_read, memory_stream_seek, memory_stream_tell, &stream, &config, NULL, &decoder);
    if (lastResult != MA_SUCCESS)
        return nullptr;

    lastResult = ma_libvorbis_get_data_format(&decoder, NULL, &channels, &sampleRate, NULL, 0);
    if (lastResult != MA_SUCCESS)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        return nullptr;
    }

    lastResult = ma_libvorbis_get_length_in_pcm_frames(&decoder, &frameCount);
    if (lastResult != MA_SUCCESS || frameCount == 0)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        lastResult = MA_INVALID_FILE;
        return nullptr;
    }

    data = (float*)ma_malloc((size_t)(frameCount * channels * sizeof(float)), NULL);
    if (data == NULL)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return nullptr;
    }

    lastResult = ma_libvorbis_read_pcm_frames(&decoder, data, frameCount, &framesRead);
    ma_libvorbis_uninit(&decoder, NULL);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || framesRead == 0)
    {
        ma_free(data, NULL);
        return nullptr;
    }

    result = copy_pcm_float_bytes(data, framesRead, channels);
    if (result != nullptr)
        set_last_decoded_format(channels, sampleRate, framesRead);

    ma_free(data, NULL);
    return result;
}

static vbyte* decode_vorbis_to_pcm_s16(const unsigned char* bytes, size_t size)
{
    memory_stream stream;
    ma_libvorbis decoder;
    ma_decoding_backend_config config;
    ma_uint64 frameCount = 0;
    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
    ma_int16* data;
    ma_uint64 framesRead = 0;
    ma_uint64 byteCount64;
    int byteCount;
    vbyte* result;

    stream.data = bytes;
    stream.size = size;
    stream.cursor = 0;

    config = ma_decoding_backend_config_init(ma_format_s16, 0);
    lastResult = ma_libvorbis_init(memory_stream_read, memory_stream_seek, memory_stream_tell, &stream, &config, NULL, &decoder);
    if (lastResult != MA_SUCCESS)
        return nullptr;

    lastResult = ma_libvorbis_get_data_format(&decoder, NULL, &channels, &sampleRate, NULL, 0);
    if (lastResult != MA_SUCCESS)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        return nullptr;
    }

    lastResult = ma_libvorbis_get_length_in_pcm_frames(&decoder, &frameCount);
    if (lastResult != MA_SUCCESS || frameCount == 0)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        lastResult = MA_INVALID_FILE;
        return nullptr;
    }

    data = (ma_int16*)ma_malloc((size_t)(frameCount * channels * sizeof(ma_int16)), NULL);
    if (data == NULL)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return nullptr;
    }

    lastResult = ma_libvorbis_read_pcm_frames(&decoder, data, frameCount, &framesRead);
    ma_libvorbis_uninit(&decoder, NULL);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || framesRead == 0)
    {
        ma_free(data, NULL);
        return nullptr;
    }

    byteCount64 = framesRead * channels * (ma_uint64)sizeof(ma_int16);
    if (byteCount64 == 0 || byteCount64 > 0x7FFFFFFF)
    {
        ma_free(data, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return nullptr;
    }

    byteCount = (int)byteCount64;
    result = hl_copy_bytes((const vbyte*)data, byteCount);
    ma_free(data, NULL);

    if (result != nullptr)
        set_last_decoded_format(channels, sampleRate, framesRead);

    return result;
}

static vbyte* decode_bytes_to_pcm_float(const unsigned char* bytes, size_t size)
{
    ma_decoder decoder;
    ma_decoder_config config;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint64 frameCount;
    ma_uint64 framesRead = 0;
    float* data;
    vbyte* result;

    if (bytes == nullptr || size == 0)
    {
        lastResult = MA_INVALID_ARGS;
        return nullptr;
    }

    if (size >= 4 && memcmp(bytes, "OggS", 4) == 0)
        return decode_vorbis_to_pcm_float(bytes, size);

    config = ma_decoder_config_init(ma_format_f32, 0, 0);
    lastResult = ma_decoder_init_memory(bytes, size, &config, &decoder);
    if (lastResult != MA_SUCCESS)
        return nullptr;

    channels = decoder.outputChannels;
    sampleRate = decoder.outputSampleRate;

    lastResult = ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);
    if (lastResult != MA_SUCCESS || frameCount == 0)
    {
        ma_decoder_uninit(&decoder);
        lastResult = MA_INVALID_FILE;
        return nullptr;
    }

    data = (float*)ma_malloc((size_t)(frameCount * channels * sizeof(float)), NULL);
    if (data == NULL)
    {
        ma_decoder_uninit(&decoder);
        lastResult = MA_OUT_OF_MEMORY;
        return nullptr;
    }

    lastResult = ma_decoder_read_pcm_frames(&decoder, data, frameCount, &framesRead);
    ma_decoder_uninit(&decoder);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || framesRead == 0)
    {
        ma_free(data, NULL);
        return nullptr;
    }

    result = copy_pcm_float_bytes(data, framesRead, channels);
    if (result != nullptr)
        set_last_decoded_format(channels, sampleRate, framesRead);

    ma_free(data, NULL);
    return result;
}

static vbyte* decode_bytes_to_pcm_s16(const unsigned char* bytes, size_t size)
{
    ma_decoder decoder;
    ma_decoder_config config;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint64 frameCount;
    ma_uint64 framesRead = 0;
    ma_int16* data;
    ma_uint64 byteCount64;
    int byteCount;
    vbyte* result;

    if (bytes == nullptr || size == 0)
    {
        lastResult = MA_INVALID_ARGS;
        return nullptr;
    }

    if (size >= 4 && memcmp(bytes, "OggS", 4) == 0)
        return decode_vorbis_to_pcm_s16(bytes, size);

    config = ma_decoder_config_init(ma_format_s16, 0, 0);
    lastResult = ma_decoder_init_memory(bytes, size, &config, &decoder);
    if (lastResult != MA_SUCCESS)
        return nullptr;

    channels = decoder.outputChannels;
    sampleRate = decoder.outputSampleRate;

    lastResult = ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);
    if (lastResult != MA_SUCCESS || frameCount == 0)
    {
        ma_decoder_uninit(&decoder);
        lastResult = MA_INVALID_FILE;
        return nullptr;
    }

    data = (ma_int16*)ma_malloc((size_t)(frameCount * channels * sizeof(ma_int16)), NULL);
    if (data == NULL)
    {
        ma_decoder_uninit(&decoder);
        lastResult = MA_OUT_OF_MEMORY;
        return nullptr;
    }

    lastResult = ma_decoder_read_pcm_frames(&decoder, data, frameCount, &framesRead);
    ma_decoder_uninit(&decoder);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || framesRead == 0)
    {
        ma_free(data, NULL);
        return nullptr;
    }

    byteCount64 = framesRead * channels * (ma_uint64)sizeof(ma_int16);
    if (byteCount64 == 0 || byteCount64 > 0x7FFFFFFF)
    {
        ma_free(data, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return nullptr;
    }

    byteCount = (int)byteCount64;
    result = hl_copy_bytes((const vbyte*)data, byteCount);
    ma_free(data, NULL);

    if (result != nullptr)
        set_last_decoded_format(channels, sampleRate, framesRead);

    return result;
}

static ma_audio_buffer* decode_vorbis_from_memory(const unsigned char* bytes, size_t size)
{
    vbyte* pcmBytes = decode_vorbis_to_pcm_float(bytes, size);
    if (pcmBytes == nullptr)
        return nullptr;

    return HL_NAME(buffer_from_pcm_float)(pcmBytes, lastDecodedSamples * lastDecodedChannels * (int)sizeof(float), lastDecodedChannels, lastDecodedSampleRate);
}
HL_PRIM bool HL_NAME(init)()
{
	lastResult = ma_engine_init(NULL, &engine);
	return lastResult == MA_SUCCESS;
}
DEFINE_PRIM(_BOOL, init, _NO_ARG);

HL_PRIM void HL_NAME(uninit)()
{
	ma_engine_uninit(&engine);
}
DEFINE_PRIM(_VOID, uninit, _NO_ARG);

HL_PRIM vbyte* HL_NAME(describe_last_error)()
{
    return (vbyte*)ma_result_description(lastResult);
}
DEFINE_PRIM(_BYTES, describe_last_error, _NO_ARG);

// ===== BUFFER =====

HL_PRIM void HL_NAME(buffer_dispose)(ma_audio_buffer* buffer)
{
    ma_audio_buffer_uninit_and_free(buffer);
}
DEFINE_PRIM(_VOID, buffer_dispose, _BUFFER);

HL_PRIM ma_audio_buffer* HL_NAME(buffer_from_bytes)(vbyte* bytes, int size)
{
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);

    if (bytes == nullptr || size <= 0)
    {
        lastResult = MA_INVALID_ARGS;
        return nullptr;
    }

    if (size >= 4 && memcmp(bytes, "OggS", 4) == 0)
        return decode_vorbis_from_memory((const unsigned char*)bytes, (size_t)size);

    lastResult = ma_decoder_init_memory(bytes, (size_t)size, &config, &decoder);
    if (lastResult != MA_SUCCESS)
        return nullptr;

    ma_uint32 channels = decoder.outputChannels;
    ma_uint32 sampleRate = decoder.outputSampleRate;

    ma_uint64 frameCount;
    ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);

    float* data = (float*)ma_malloc(frameCount * channels * sizeof(float), NULL);
    ma_decoder_read_pcm_frames(&decoder, data, frameCount, NULL);
    ma_decoder_uninit(&decoder);

    ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
        ma_format_f32,
        channels,
        frameCount,
        data,
        NULL
    );

    ma_audio_buffer* buffer;
    lastResult = ma_audio_buffer_alloc_and_init(&bufferConfig, &buffer);

    if (lastResult == MA_SUCCESS)
    {
        buffer->ref.sampleRate = sampleRate;
        return buffer;
    }
    else
    {
        HL_NAME(buffer_dispose)(buffer);
        return nullptr;
    }
}
DEFINE_PRIM(_BUFFER, buffer_from_bytes, _BYTES _I32);

HL_PRIM vbyte* HL_NAME(decode_pcm_float)(vbyte* bytes, int size)
{
    return decode_bytes_to_pcm_float((const unsigned char*)bytes, (size_t)size);
}
DEFINE_PRIM(_BYTES, decode_pcm_float, _BYTES _I32);

HL_PRIM vbyte* HL_NAME(decode_pcm_s16)(vbyte* bytes, int size)
{
    return decode_bytes_to_pcm_s16((const unsigned char*)bytes, (size_t)size);
}
DEFINE_PRIM(_BYTES, decode_pcm_s16, _BYTES _I32);

HL_PRIM int HL_NAME(decoded_channels)()
{
    return lastDecodedChannels;
}
DEFINE_PRIM(_I32, decoded_channels, _NO_ARG);

HL_PRIM int HL_NAME(decoded_sample_rate)()
{
    return lastDecodedSampleRate;
}
DEFINE_PRIM(_I32, decoded_sample_rate, _NO_ARG);

HL_PRIM int HL_NAME(decoded_samples)()
{
    return lastDecodedSamples;
}
DEFINE_PRIM(_I32, decoded_samples, _NO_ARG);

HL_PRIM ma_audio_buffer* HL_NAME(buffer_from_pcm_float)(vbyte* bytes, int size, int channels, int sampleRate)
{
    if (bytes == nullptr || size <= 0 || channels <= 0 || sampleRate <= 0)
    {
        lastResult = MA_INVALID_ARGS;
        return nullptr;
    }

    int frameSize = channels * (int)sizeof(float);
    if (frameSize <= 0 || (size % frameSize) != 0)
    {
        lastResult = MA_INVALID_ARGS;
        return nullptr;
    }

    ma_uint64 frameCount = (ma_uint64)(size / frameSize);
    float* data = (float*)ma_malloc((size_t)size, NULL);
    if (data == nullptr)
    {
        lastResult = MA_OUT_OF_MEMORY;
        return nullptr;
    }

    memcpy(data, bytes, (size_t)size);

    ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
        ma_format_f32,
        (ma_uint32)channels,
        frameCount,
        data,
        NULL
    );

    ma_audio_buffer* buffer;
    lastResult = ma_audio_buffer_alloc_and_init(&bufferConfig, &buffer);
    if (lastResult == MA_SUCCESS)
    {
        buffer->ref.sampleRate = (ma_uint32)sampleRate;
        return buffer;
    }

    ma_free(data, NULL);
    return nullptr;
}
DEFINE_PRIM(_BUFFER, buffer_from_pcm_float, _BYTES _I32 _I32 _I32);

HL_PRIM ma_audio_buffer* HL_NAME(buffer_from_pcm_s16)(vbyte* bytes, int size, int channels, int sampleRate)
{
    if (bytes == nullptr || size <= 0 || channels <= 0 || sampleRate <= 0)
    {
        lastResult = MA_INVALID_ARGS;
        return nullptr;
    }

    int frameSize = channels * (int)sizeof(ma_int16);
    if (frameSize <= 0 || (size % frameSize) != 0)
    {
        lastResult = MA_INVALID_ARGS;
        return nullptr;
    }

    ma_uint64 frameCount = (ma_uint64)(size / frameSize);
    ma_int16* data = (ma_int16*)ma_malloc((size_t)size, NULL);
    if (data == nullptr)
    {
        lastResult = MA_OUT_OF_MEMORY;
        return nullptr;
    }

    memcpy(data, bytes, (size_t)size);

    ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
        ma_format_s16,
        (ma_uint32)channels,
        frameCount,
        data,
        NULL
    );

    ma_audio_buffer* buffer;
    lastResult = ma_audio_buffer_alloc_and_init(&bufferConfig, &buffer);
    if (lastResult == MA_SUCCESS)
    {
        buffer->ref.sampleRate = (ma_uint32)sampleRate;
        return buffer;
    }

    ma_free(data, NULL);
    return nullptr;
}
DEFINE_PRIM(_BUFFER, buffer_from_pcm_s16, _BYTES _I32 _I32 _I32);

// ===== SOUND GROUP =====

HL_PRIM void HL_NAME(sound_group_dispose)(ma_sound_group* group)
{
    ma_sound_group_uninit(group);
    ma_free(group, NULL);
}
DEFINE_PRIM(_VOID, sound_group_dispose, _GROUP);

HL_PRIM ma_sound_group* HL_NAME(sound_group_init)(ma_sound_group* parent)
{
    ma_sound_group* group = (ma_sound_group*)ma_malloc(sizeof(ma_sound_group), NULL);
    lastResult = ma_sound_group_init(&engine, 0, parent, group);

    if (lastResult == MA_SUCCESS)
        return group;
    else
    {
        HL_NAME(sound_group_dispose)(group);
        return nullptr;
    }
}
DEFINE_PRIM(_GROUP, sound_group_init, _GROUP);

HL_PRIM bool HL_NAME(sound_group_start)(ma_sound_group* group)
{
    lastResult = ma_sound_group_start(group);
    return lastResult == MA_SUCCESS;
}
DEFINE_PRIM(_BOOL, sound_group_start, _GROUP);

HL_PRIM bool HL_NAME(sound_group_stop)(ma_sound_group* group)
{
    lastResult = ma_sound_group_stop(group);
    return lastResult == MA_SUCCESS;
}
DEFINE_PRIM(_BOOL, sound_group_stop, _GROUP);

#define GET_SET_FLOAT(n) HL_PRIM double HL_NAME(sound_group_get_##n)(ma_sound_group* group) \
{ \
    return ma_sound_group_get_##n(group); \
} \
DEFINE_PRIM(_F64, sound_group_get_##n, _GROUP) \
HL_PRIM double HL_NAME(sound_group_set_##n)(ma_sound_group* group, double value) \
{ \
    ma_sound_group_set_##n(group, value); \
    return value; \
} \
DEFINE_PRIM(_F64, sound_group_set_##n, _GROUP _F64)

GET_SET_FLOAT(volume)
GET_SET_FLOAT(pan)

HL_PRIM int HL_NAME(sound_group_get_pan_mode)(ma_sound_group* group)
{
    return (int)ma_sound_group_get_pan_mode(group);
}
DEFINE_PRIM(_I32, sound_group_get_pan_mode, _GROUP);

HL_PRIM int HL_NAME(sound_group_set_pan_mode)(ma_sound_group* group, int mode)
{
    ma_sound_group_set_pan_mode(group, (ma_pan_mode)mode);
    return mode;
}
DEFINE_PRIM(_I32, sound_group_set_pan_mode, _GROUP _I32);

GET_SET_FLOAT(pitch)

HL_PRIM bool HL_NAME(sound_group_get_spatialization_enabled)(ma_sound_group* group)
{
    return ma_sound_group_is_spatialization_enabled(group) == MA_TRUE;
}
DEFINE_PRIM(_BOOL, sound_group_get_spatialization_enabled, _GROUP);

HL_PRIM bool HL_NAME(sound_group_set_spatialization_enabled)(ma_sound_group* group, bool enabled)
{
    ma_sound_group_set_spatialization_enabled(group, enabled ? MA_TRUE : MA_FALSE);
    return enabled;
}
DEFINE_PRIM(_BOOL, sound_group_set_spatialization_enabled, _GROUP _BOOL);

// ===== SOUND =====

HL_PRIM void HL_NAME(sound_dispose)(ma_sound* sound)
{
    ma_sound_uninit(sound);
    ma_free(sound, NULL);
}
DEFINE_PRIM(_VOID, sound_dispose, _SOUND);

HL_PRIM ma_sound* HL_NAME(sound_init)(ma_audio_buffer* buffer, ma_sound_group* parent)
{
    ma_sound* sound = (ma_sound*)ma_malloc(sizeof(ma_sound), NULL);
    lastResult = ma_sound_init_from_data_source(&engine, buffer, 0, parent == nullptr ? NULL : parent, sound);

    if (lastResult == MA_SUCCESS)
        return sound;
    else
    {
        HL_NAME(sound_dispose)(sound);
        return nullptr;
    }
}
DEFINE_PRIM(_SOUND, sound_init, _BUFFER _GROUP);

HL_PRIM bool HL_NAME(sound_start)(ma_sound* sound)
{
    lastResult = ma_sound_start(sound);
    return lastResult == MA_SUCCESS;
}
DEFINE_PRIM(_BOOL, sound_start, _SOUND);

HL_PRIM bool HL_NAME(sound_stop)(ma_sound* sound)
{
    lastResult = ma_sound_stop(sound);
    return lastResult == MA_SUCCESS;
}
DEFINE_PRIM(_BOOL, sound_stop, _SOUND);

HL_PRIM int HL_NAME(sound_seek_samples)(ma_sound* sound, int sample)
{
    if (sample < 0)
        sample = 0;

    lastResult = ma_sound_seek_to_pcm_frame(sound, (ma_uint64)sample);
    return lastResult == MA_SUCCESS ? sample : -1;
}
DEFINE_PRIM(_I32, sound_seek_samples, _SOUND _I32);

HL_PRIM int HL_NAME(sound_get_cursor_samples)(ma_sound* sound)
{
    ma_uint64 cursor = 0;
    lastResult = ma_sound_get_cursor_in_pcm_frames(sound, &cursor);
    return lastResult == MA_SUCCESS ? (int)cursor : 0;
}
DEFINE_PRIM(_I32, sound_get_cursor_samples, _SOUND);

HL_PRIM bool HL_NAME(sound_is_playing)(ma_sound* sound)
{
    return ma_sound_is_playing(sound) == MA_TRUE;
}
DEFINE_PRIM(_BOOL, sound_is_playing, _SOUND);

#undef GET_SET_FLOAT
#define GET_SET_FLOAT(n) HL_PRIM double HL_NAME(sound_get_##n)(ma_sound* sound) \
{ \
    return ma_sound_get_##n(sound); \
} \
DEFINE_PRIM(_F64, sound_get_##n, _SOUND) \
HL_PRIM double HL_NAME(sound_set_##n)(ma_sound* sound, double value) \
{ \
    ma_sound_set_##n(sound, value); \
    return value; \
} \
DEFINE_PRIM(_F64, sound_set_##n, _SOUND _F64)

GET_SET_FLOAT(volume)
GET_SET_FLOAT(pan)

HL_PRIM int HL_NAME(sound_get_pan_mode)(ma_sound* sound)
{
    return (int)ma_sound_get_pan_mode(sound);
}
DEFINE_PRIM(_I32, sound_get_pan_mode, _SOUND);

HL_PRIM int HL_NAME(sound_set_pan_mode)(ma_sound* sound, int mode)
{
    ma_sound_set_pan_mode(sound, (ma_pan_mode)mode);
    return mode;
}
DEFINE_PRIM(_I32, sound_set_pan_mode, _SOUND _I32);

GET_SET_FLOAT(pitch)

HL_PRIM bool HL_NAME(sound_get_spatialization_enabled)(ma_sound* sound)
{
    return ma_sound_is_spatialization_enabled(sound) == MA_TRUE;
}
DEFINE_PRIM(_BOOL, sound_get_spatialization_enabled, _SOUND);

HL_PRIM bool HL_NAME(sound_set_spatialization_enabled)(ma_sound* sound, bool enabled)
{
    ma_sound_set_spatialization_enabled(sound, enabled ? MA_TRUE : MA_FALSE);
    return enabled;
}
DEFINE_PRIM(_BOOL, sound_set_spatialization_enabled, _SOUND _BOOL);

HL_PRIM double HL_NAME(sound_get_time)(ma_sound* sound)
{
    return ma_sound_get_time_in_milliseconds(sound);
}
DEFINE_PRIM(_F64, sound_get_time, _SOUND);
