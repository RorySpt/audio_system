#include "soloud.h"
#include "soloud_internal.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_NULL
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_ENGINE
#include "miniaudio.h"

namespace SoLoud
{
namespace
{
    ma_device g_device{};

    void audio_mixer(ma_device* device, void* output, const void* input, ma_uint32 frame_count)
    {
        auto* soloud = static_cast<SoLoud::Soloud*>(device->pUserData);
        if (soloud != nullptr)
        {
            soloud->mix(static_cast<float*>(output), frame_count);
        }

        (void)input;
    }

    void cleanup_backend(SoLoud::Soloud*)
    {
        ma_device_uninit(&g_device);
    }
}

result miniaudio_init(SoLoud::Soloud* soloud, unsigned int flags, unsigned int sample_rate, unsigned int buffer_size,
                      unsigned int channels)
{
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = channels;
    config.sampleRate = sample_rate;
    config.dataCallback = audio_mixer;
    config.pUserData = soloud;

    if (buffer_size != 0 && buffer_size != SoLoud::Soloud::AUTO)
    {
        config.periodSizeInFrames = buffer_size;
    }

    ma_result init_result = ma_device_init(nullptr, &config, &g_device);
    if (init_result != MA_SUCCESS && config.periodSizeInFrames != 0)
    {
        config.periodSizeInFrames = 0;
        init_result = ma_device_init(nullptr, &config, &g_device);
    }

    if (init_result != MA_SUCCESS)
    {
        return UNKNOWN_ERROR;
    }

    soloud->postinit_internal(g_device.sampleRate, g_device.playback.internalPeriodSizeInFrames, flags,
                              g_device.playback.channels);

    soloud->mBackendCleanupFunc = cleanup_backend;
    soloud->mBackendString = "miniaudio";

    if (ma_device_start(&g_device) != MA_SUCCESS)
    {
        ma_device_uninit(&g_device);
        return UNKNOWN_ERROR;
    }

    return SO_NO_ERROR;
}
}