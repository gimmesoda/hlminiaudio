#define HL_NAME(n) miniaudio_##n
#include <hl.h>
#include <string.h>

#ifdef _GUID
#undef _GUID
#endif

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_VORBIS
#include <miniaudio.h>

#include "extras/decoders/libopus/miniaudio_libopus.h"
#include "extras/decoders/libvorbis/miniaudio_libvorbis.h"

#define _BUFFER _ABSTRACT(ma_audio_buffer)
#define _SOUND _ABSTRACT(ma_sound)
#define _GROUP _ABSTRACT(ma_sound_group)

ma_engine engine;
ma_result lastResult;
int lastDecodedChannels, lastDecodedSampleRate, lastDecodedSamples;

typedef struct sound_callback_entry
{
    ma_sound* sound;
    vclosure* callback;
    volatile int pending;
    struct sound_callback_entry* next;
} sound_callback_entry;

static sound_callback_entry* sound_callbacks = NULL;

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
    return NULL;
}

static void set_last_decoded_format(ma_uint32 channels, ma_uint32 sampleRate, ma_uint64 frameCount)
{
    lastDecodedChannels = (int)channels;
    lastDecodedSampleRate = (int)sampleRate;
    lastDecodedSamples = (int)frameCount;
}

static int is_ogg_stream(const unsigned char* bytes, size_t size)
{
    return size >= 4 && memcmp(bytes, "OggS", 4) == 0;
}

static int read_ogg_packet_signature(const unsigned char* bytes, size_t size, const char* signature, size_t signatureSize)
{
    size_t segmentTableSize;
    size_t packetOffset;

    if (!is_ogg_stream(bytes, size) || size < 27)
        return 0;

    segmentTableSize = bytes[26];
    packetOffset = 27 + segmentTableSize;
    if (packetOffset + signatureSize > size)
        return 0;

    return memcmp(bytes + packetOffset, signature, signatureSize) == 0;
}

static int is_opus_stream(const unsigned char* bytes, size_t size)
{
    return read_ogg_packet_signature(bytes, size, "OpusHead", 8);
}

static int is_vorbis_stream(const unsigned char* bytes, size_t size)
{
    static const unsigned char signature[] = { 0x01, 'v', 'o', 'r', 'b', 'i', 's' };
    return read_ogg_packet_signature(bytes, size, (const char*)signature, sizeof(signature));
}

static vbyte* copy_pcm_float_bytes(const float* data, ma_uint64 frameCount, ma_uint32 channels)
{
    ma_uint64 byteCount64 = frameCount * channels * (ma_uint64)sizeof(float);
    int byteCount;

    if (byteCount64 == 0 || byteCount64 > 0x7FFFFFFF)
    {
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
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
        return NULL;

    lastResult = ma_libvorbis_get_data_format(&decoder, NULL, &channels, &sampleRate, NULL, 0);
    if (lastResult != MA_SUCCESS)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        return NULL;
    }

    lastResult = ma_libvorbis_get_length_in_pcm_frames(&decoder, &frameCount);
    if (lastResult != MA_SUCCESS || frameCount == 0)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        lastResult = MA_INVALID_FILE;
        return NULL;
    }

    data = (float*)ma_malloc((size_t)(frameCount * channels * sizeof(float)), NULL);
    if (data == NULL)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
    }

    lastResult = ma_libvorbis_read_pcm_frames(&decoder, data, frameCount, &framesRead);
    ma_libvorbis_uninit(&decoder, NULL);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || framesRead == 0)
    {
        ma_free(data, NULL);
        return NULL;
    }

    result = copy_pcm_float_bytes(data, framesRead, channels);
    if (result != NULL)
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
        return NULL;

    lastResult = ma_libvorbis_get_data_format(&decoder, NULL, &channels, &sampleRate, NULL, 0);
    if (lastResult != MA_SUCCESS)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        return NULL;
    }

    lastResult = ma_libvorbis_get_length_in_pcm_frames(&decoder, &frameCount);
    if (lastResult != MA_SUCCESS || frameCount == 0)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        lastResult = MA_INVALID_FILE;
        return NULL;
    }

    data = (ma_int16*)ma_malloc((size_t)(frameCount * channels * sizeof(ma_int16)), NULL);
    if (data == NULL)
    {
        ma_libvorbis_uninit(&decoder, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
    }

    lastResult = ma_libvorbis_read_pcm_frames(&decoder, data, frameCount, &framesRead);
    ma_libvorbis_uninit(&decoder, NULL);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || framesRead == 0)
    {
        ma_free(data, NULL);
        return NULL;
    }

    byteCount64 = framesRead * channels * (ma_uint64)sizeof(ma_int16);
    if (byteCount64 == 0 || byteCount64 > 0x7FFFFFFF)
    {
        ma_free(data, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
    }

    byteCount = (int)byteCount64;
    result = hl_copy_bytes((const vbyte*)data, byteCount);
    ma_free(data, NULL);

    if (result != NULL)
        set_last_decoded_format(channels, sampleRate, framesRead);

    return result;
}

static vbyte* decode_opus_to_pcm_float(const unsigned char* bytes, size_t size)
{
    memory_stream stream;
    ma_libopus decoder;
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
    lastResult = ma_libopus_init(memory_stream_read, memory_stream_seek, memory_stream_tell, &stream, &config, NULL, &decoder);
    if (lastResult != MA_SUCCESS)
        return NULL;

    lastResult = ma_libopus_get_data_format(&decoder, NULL, &channels, &sampleRate, NULL, 0);
    if (lastResult != MA_SUCCESS)
    {
        ma_libopus_uninit(&decoder, NULL);
        return NULL;
    }

    lastResult = ma_libopus_get_length_in_pcm_frames(&decoder, &frameCount);
    if (lastResult != MA_SUCCESS || frameCount == 0)
    {
        ma_libopus_uninit(&decoder, NULL);
        lastResult = MA_INVALID_FILE;
        return NULL;
    }

    data = (float*)ma_malloc((size_t)(frameCount * channels * sizeof(float)), NULL);
    if (data == NULL)
    {
        ma_libopus_uninit(&decoder, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
    }

    lastResult = ma_libopus_read_pcm_frames(&decoder, data, frameCount, &framesRead);
    ma_libopus_uninit(&decoder, NULL);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || framesRead == 0)
    {
        ma_free(data, NULL);
        return NULL;
    }

    result = copy_pcm_float_bytes(data, framesRead, channels);
    if (result != NULL)
        set_last_decoded_format(channels, sampleRate, framesRead);

    ma_free(data, NULL);
    return result;
}

static vbyte* decode_opus_to_pcm_s16(const unsigned char* bytes, size_t size)
{
    memory_stream stream;
    ma_libopus decoder;
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
    lastResult = ma_libopus_init(memory_stream_read, memory_stream_seek, memory_stream_tell, &stream, &config, NULL, &decoder);
    if (lastResult != MA_SUCCESS)
        return NULL;

    lastResult = ma_libopus_get_data_format(&decoder, NULL, &channels, &sampleRate, NULL, 0);
    if (lastResult != MA_SUCCESS)
    {
        ma_libopus_uninit(&decoder, NULL);
        return NULL;
    }

    lastResult = ma_libopus_get_length_in_pcm_frames(&decoder, &frameCount);
    if (lastResult != MA_SUCCESS || frameCount == 0)
    {
        ma_libopus_uninit(&decoder, NULL);
        lastResult = MA_INVALID_FILE;
        return NULL;
    }

    data = (ma_int16*)ma_malloc((size_t)(frameCount * channels * sizeof(ma_int16)), NULL);
    if (data == NULL)
    {
        ma_libopus_uninit(&decoder, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
    }

    lastResult = ma_libopus_read_pcm_frames(&decoder, data, frameCount, &framesRead);
    ma_libopus_uninit(&decoder, NULL);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || framesRead == 0)
    {
        ma_free(data, NULL);
        return NULL;
    }

    byteCount64 = framesRead * channels * (ma_uint64)sizeof(ma_int16);
    if (byteCount64 == 0 || byteCount64 > 0x7FFFFFFF)
    {
        ma_free(data, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
    }

    byteCount = (int)byteCount64;
    result = hl_copy_bytes((const vbyte*)data, byteCount);
    ma_free(data, NULL);

    if (result != NULL)
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

    if (bytes == NULL || size == 0)
    {
        lastResult = MA_INVALID_ARGS;
        return NULL;
    }

    if (is_opus_stream(bytes, size))
        return decode_opus_to_pcm_float(bytes, size);

    if (is_vorbis_stream(bytes, size))
        return decode_vorbis_to_pcm_float(bytes, size);

    config = ma_decoder_config_init(ma_format_f32, 0, 0);
    lastResult = ma_decoder_init_memory(bytes, size, &config, &decoder);
    if (lastResult != MA_SUCCESS)
        return NULL;

    channels = decoder.outputChannels;
    sampleRate = decoder.outputSampleRate;

    lastResult = ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);
    if (lastResult != MA_SUCCESS || frameCount == 0)
    {
        ma_decoder_uninit(&decoder);
        lastResult = MA_INVALID_FILE;
        return NULL;
    }

    data = (float*)ma_malloc((size_t)(frameCount * channels * sizeof(float)), NULL);
    if (data == NULL)
    {
        ma_decoder_uninit(&decoder);
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
    }

    lastResult = ma_decoder_read_pcm_frames(&decoder, data, frameCount, &framesRead);
    ma_decoder_uninit(&decoder);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || framesRead == 0)
    {
        ma_free(data, NULL);
        return NULL;
    }

    result = copy_pcm_float_bytes(data, framesRead, channels);
    if (result != NULL)
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

    if (bytes == NULL || size == 0)
    {
        lastResult = MA_INVALID_ARGS;
        return NULL;
    }

    if (is_opus_stream(bytes, size))
        return decode_opus_to_pcm_s16(bytes, size);

    if (is_vorbis_stream(bytes, size))
        return decode_vorbis_to_pcm_s16(bytes, size);

    config = ma_decoder_config_init(ma_format_s16, 0, 0);
    lastResult = ma_decoder_init_memory(bytes, size, &config, &decoder);
    if (lastResult != MA_SUCCESS)
        return NULL;

    channels = decoder.outputChannels;
    sampleRate = decoder.outputSampleRate;

    lastResult = ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);
    if (lastResult != MA_SUCCESS || frameCount == 0)
    {
        ma_decoder_uninit(&decoder);
        lastResult = MA_INVALID_FILE;
        return NULL;
    }

    data = (ma_int16*)ma_malloc((size_t)(frameCount * channels * sizeof(ma_int16)), NULL);
    if (data == NULL)
    {
        ma_decoder_uninit(&decoder);
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
    }

    lastResult = ma_decoder_read_pcm_frames(&decoder, data, frameCount, &framesRead);
    ma_decoder_uninit(&decoder);
    if ((lastResult != MA_SUCCESS && lastResult != MA_AT_END) || framesRead == 0)
    {
        ma_free(data, NULL);
        return NULL;
    }

    byteCount64 = framesRead * channels * (ma_uint64)sizeof(ma_int16);
    if (byteCount64 == 0 || byteCount64 > 0x7FFFFFFF)
    {
        ma_free(data, NULL);
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
    }

    byteCount = (int)byteCount64;
    result = hl_copy_bytes((const vbyte*)data, byteCount);
    ma_free(data, NULL);

    if (result != NULL)
        set_last_decoded_format(channels, sampleRate, framesRead);

    return result;
}

static ma_audio_buffer* decode_vorbis_from_memory(const unsigned char* bytes, size_t size)
{
    vbyte* pcmBytes = decode_vorbis_to_pcm_float(bytes, size);
    if (pcmBytes == NULL)
        return NULL;

    return HL_NAME(buffer_from_pcm_float)(pcmBytes, lastDecodedSamples * lastDecodedChannels * (int)sizeof(float), lastDecodedChannels, lastDecodedSampleRate);
}

static ma_audio_buffer* decode_opus_from_memory(const unsigned char* bytes, size_t size)
{
    vbyte* pcmBytes = decode_opus_to_pcm_float(bytes, size);
    if (pcmBytes == NULL)
        return NULL;

    return HL_NAME(buffer_from_pcm_float)(pcmBytes, lastDecodedSamples * lastDecodedChannels * (int)sizeof(float), lastDecodedChannels, lastDecodedSampleRate);
}

HL_PRIM ma_audio_buffer* HL_NAME(buffer_from_pcm_float)(vbyte* bytes, int size, int channels, int sampleRate);

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
    vbyte* pcmBytes;

    if (bytes == NULL || size <= 0)
    {
        lastResult = MA_INVALID_ARGS;
        return NULL;
    }

    if (is_opus_stream((const unsigned char*)bytes, (size_t)size))
        return decode_opus_from_memory((const unsigned char*)bytes, (size_t)size);

    if (is_vorbis_stream((const unsigned char*)bytes, (size_t)size))
        return decode_vorbis_from_memory((const unsigned char*)bytes, (size_t)size);

    pcmBytes = decode_bytes_to_pcm_float((const unsigned char*)bytes, (size_t)size);
    if (pcmBytes == NULL)
        return NULL;

    return HL_NAME(buffer_from_pcm_float)(
        pcmBytes,
        lastDecodedSamples * lastDecodedChannels * (int)sizeof(float),
        lastDecodedChannels,
        lastDecodedSampleRate
    );
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
    if (bytes == NULL || size <= 0 || channels <= 0 || sampleRate <= 0)
    {
        lastResult = MA_INVALID_ARGS;
        return NULL;
    }

    int frameSize = channels * (int)sizeof(float);
    if (frameSize <= 0 || (size % frameSize) != 0)
    {
        lastResult = MA_INVALID_ARGS;
        return NULL;
    }

    ma_uint64 frameCount = (ma_uint64)(size / frameSize);
    float* data = (float*)ma_malloc((size_t)size, NULL);
    if (data == NULL)
    {
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
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
    return NULL;
}
DEFINE_PRIM(_BUFFER, buffer_from_pcm_float, _BYTES _I32 _I32 _I32);

HL_PRIM ma_audio_buffer* HL_NAME(buffer_from_pcm_s16)(vbyte* bytes, int size, int channels, int sampleRate)
{
    if (bytes == NULL || size <= 0 || channels <= 0 || sampleRate <= 0)
    {
        lastResult = MA_INVALID_ARGS;
        return NULL;
    }

    int frameSize = channels * (int)sizeof(ma_int16);
    if (frameSize <= 0 || (size % frameSize) != 0)
    {
        lastResult = MA_INVALID_ARGS;
        return NULL;
    }

    ma_uint64 frameCount = (ma_uint64)(size / frameSize);
    ma_int16* data = (ma_int16*)ma_malloc((size_t)size, NULL);
    if (data == NULL)
    {
        lastResult = MA_OUT_OF_MEMORY;
        return NULL;
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
    return NULL;
}
DEFINE_PRIM(_BUFFER, buffer_from_pcm_s16, _BYTES _I32 _I32 _I32);

HL_PRIM int HL_NAME(buffer_get_length_samples)(ma_audio_buffer* buffer)
{
    ma_uint64 length = 0;
    lastResult = ma_audio_buffer_get_length_in_pcm_frames(buffer, &length);
    return lastResult == MA_SUCCESS ? (int)length : 0;
}
DEFINE_PRIM(_I32, buffer_get_length_samples, _BUFFER);

HL_PRIM double HL_NAME(buffer_get_duration)(ma_audio_buffer* buffer)
{
    ma_uint64 length = 0;

    lastResult = ma_audio_buffer_get_length_in_pcm_frames(buffer, &length);
    if (lastResult != MA_SUCCESS || buffer->ref.sampleRate == 0)
        return 0;

    return ((double)length * 1000.0) / (double)buffer->ref.sampleRate;
}
DEFINE_PRIM(_F64, buffer_get_duration, _BUFFER);

HL_PRIM double HL_NAME(buffer_get_duration_seconds)(ma_audio_buffer* buffer)
{
    ma_uint64 length = 0;

    lastResult = ma_audio_buffer_get_length_in_pcm_frames(buffer, &length);
    if (lastResult != MA_SUCCESS || buffer->ref.sampleRate == 0)
        return 0;

    return (double)length / (double)buffer->ref.sampleRate;
}
DEFINE_PRIM(_F64, buffer_get_duration_seconds, _BUFFER);

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
        return NULL;
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
    sound_callback_entry** entry = &sound_callbacks;

    lastResult = ma_sound_set_end_callback(sound, NULL, NULL);

    while (*entry != NULL)
    {
        if ((*entry)->sound == sound)
        {
            sound_callback_entry* current = *entry;
            *entry = current->next;
            hl_remove_root(&current->callback);
            free(current);
            break;
        }

        entry = &(*entry)->next;
    }

    ma_sound_uninit(sound);
    ma_free(sound, NULL);
}
DEFINE_PRIM(_VOID, sound_dispose, _SOUND);

static sound_callback_entry* sound_get_callback_entry(ma_sound* sound, bool create)
{
    sound_callback_entry* entry = sound_callbacks;

    while (entry != NULL)
    {
        if (entry->sound == sound)
            return entry;

        entry = entry->next;
    }

    if (!create)
        return NULL;

    entry = (sound_callback_entry*)calloc(1, sizeof(sound_callback_entry));
    entry->sound = sound;
    hl_add_root(&entry->callback);
    entry->next = sound_callbacks;
    sound_callbacks = entry;
    return entry;
}

static void sound_end_callback(void* pUserData, ma_sound* pSound)
{
    sound_callback_entry* entry = (sound_callback_entry*)pUserData;
    if (entry != NULL && entry->sound == pSound && entry->callback != NULL)
        entry->pending = 1;
}

HL_PRIM ma_sound* HL_NAME(sound_init)(ma_audio_buffer* buffer, ma_sound_group* parent)
{
    ma_sound* sound = (ma_sound*)ma_malloc(sizeof(ma_sound), NULL);
    lastResult = ma_sound_init_from_data_source(&engine, buffer, 0, parent == NULL ? NULL : parent, sound);

    if (lastResult == MA_SUCCESS)
        return sound;
    else
    {
        HL_NAME(sound_dispose)(sound);
        return NULL;
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

HL_PRIM double HL_NAME(sound_seek_seconds)(ma_sound* sound, double seconds)
{
    if (seconds < 0)
        seconds = 0;

    lastResult = ma_sound_seek_to_second(sound, (float)seconds);
    return lastResult == MA_SUCCESS ? seconds : -1;
}
DEFINE_PRIM(_F64, sound_seek_seconds, _SOUND _F64);

HL_PRIM double HL_NAME(sound_seek_milliseconds)(ma_sound* sound, double milliseconds)
{
    double seconds;

    if (milliseconds < 0)
        milliseconds = 0;

    seconds = milliseconds / 1000.0;
    lastResult = ma_sound_seek_to_second(sound, (float)seconds);
    return lastResult == MA_SUCCESS ? milliseconds : -1;
}
DEFINE_PRIM(_F64, sound_seek_milliseconds, _SOUND _F64);

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

HL_PRIM void HL_NAME(sound_set_end_callback)(ma_sound* sound, vclosure* callback)
{
    sound_callback_entry* entry = sound_get_callback_entry(sound, true);
    entry->callback = callback;
    entry->pending = 0;
    lastResult = ma_sound_set_end_callback(sound, callback == NULL ? NULL : sound_end_callback, callback == NULL ? NULL : entry);
}
DEFINE_PRIM(_VOID, sound_set_end_callback, _SOUND _FUN(_VOID, _NO_ARG));

HL_PRIM void HL_NAME(sound_clear_end_callback)(ma_sound* sound)
{
    sound_callback_entry* entry = sound_get_callback_entry(sound, false);
    if (entry != NULL)
    {
        entry->callback = NULL;
        entry->pending = 0;
    }

    lastResult = ma_sound_set_end_callback(sound, NULL, NULL);
}
DEFINE_PRIM(_VOID, sound_clear_end_callback, _SOUND);

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
	ma_uint64 cursor = 0;
	ma_uint32 sampleRate = 0;

	lastResult = ma_sound_get_cursor_in_pcm_frames(sound, &cursor);
	if (lastResult != MA_SUCCESS)
		return 0;

	lastResult = ma_sound_get_data_format(sound, NULL, NULL, &sampleRate, NULL, 0);
	if (lastResult != MA_SUCCESS || sampleRate == 0)
		return 0;

	return ((double)cursor * 1000.0) / (double)sampleRate;
}
DEFINE_PRIM(_F64, sound_get_time, _SOUND);

HL_PRIM double HL_NAME(sound_set_time)(ma_sound* sound, double seconds)
{
	return HL_NAME(sound_seek_milliseconds)(sound, seconds);
}
DEFINE_PRIM(_F64, sound_set_time, _SOUND _F64);

HL_PRIM double HL_NAME(sound_get_time_seconds)(ma_sound* sound)
{
	ma_uint64 cursor = 0;
	ma_uint32 sampleRate = 0;

	lastResult = ma_sound_get_cursor_in_pcm_frames(sound, &cursor);
	if (lastResult != MA_SUCCESS)
		return 0;

	lastResult = ma_sound_get_data_format(sound, NULL, NULL, &sampleRate, NULL, 0);
	if (lastResult != MA_SUCCESS || sampleRate == 0)
		return 0;

	return (double)cursor / (double)sampleRate;
}
DEFINE_PRIM(_F64, sound_get_time_seconds, _SOUND);

HL_PRIM double HL_NAME(sound_set_time_seconds)(ma_sound* sound, double seconds)
{
	return HL_NAME(sound_seek_seconds)(sound, seconds);
}
DEFINE_PRIM(_F64, sound_set_time_seconds, _SOUND _F64);

HL_PRIM double HL_NAME(sound_get_duration)(ma_sound* sound)
{
    float length = 0;
    lastResult = ma_sound_get_length_in_seconds(sound, &length);
    return lastResult == MA_SUCCESS ? (double)length * 1000.0 : 0;
}
DEFINE_PRIM(_F64, sound_get_duration, _SOUND);

HL_PRIM double HL_NAME(sound_get_duration_seconds)(ma_sound* sound)
{
    float length = 0;
    lastResult = ma_sound_get_length_in_seconds(sound, &length);
    return lastResult == MA_SUCCESS ? length : 0;
}
DEFINE_PRIM(_F64, sound_get_duration_seconds, _SOUND);

HL_PRIM int HL_NAME(sound_get_length_samples)(ma_sound* sound)
{
    ma_uint64 length = 0;
    lastResult = ma_sound_get_length_in_pcm_frames(sound, &length);
    return lastResult == MA_SUCCESS ? (int)length : 0;
}
DEFINE_PRIM(_I32, sound_get_length_samples, _SOUND);

HL_PRIM int HL_NAME(update)()
{
    sound_callback_entry* entry = sound_callbacks;
    int dispatched = 0;

    while (entry != NULL)
    {
        if (entry->pending && entry->callback != NULL)
        {
            bool isException = false;
            entry->pending = 0;
            hl_dyn_call_safe(entry->callback, NULL, 0, &isException);
            dispatched++;
        }

        entry = entry->next;
    }

    return dispatched;
}
DEFINE_PRIM(_I32, update, _NO_ARG);
