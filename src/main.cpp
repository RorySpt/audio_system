#include "soloud.h"
#include "soloud_wav.h"

#include <cmath>
#include <chrono>
#include <iostream>
#include <numbers>
#include <thread>
#include <vector>

namespace
{
constexpr unsigned int k_sample_rate = 48000;
constexpr unsigned int k_channels = 2;
constexpr unsigned int k_buffer_size = 512;
constexpr float k_duration_seconds = 2.5f;
constexpr float k_frequency_hz = 440.0f;
constexpr float k_amplitude = 0.2f;

std::vector<float> make_tone()
{
    const auto frame_count = static_cast<std::size_t>(k_sample_rate * k_duration_seconds);
    std::vector<float> samples(frame_count * k_channels);

    for (std::size_t frame = 0; frame < frame_count; ++frame)
    {
        const auto phase = static_cast<float>(frame) / static_cast<float>(k_sample_rate);
        const auto sample = std::sin(2.0f * std::numbers::pi_v<float> * k_frequency_hz * phase) * k_amplitude;

        for (unsigned int channel = 0; channel < k_channels; ++channel)
        {
            samples[frame * k_channels + channel] = sample;
        }
    }

    return samples;
}
}

int main()
{
    SoLoud::Soloud engine;

    const auto initResult = engine.init(
        SoLoud::Soloud::CLIP_ROUNDOFF,
        SoLoud::Soloud::MINIAUDIO,
        k_sample_rate,
        k_buffer_size,
        k_channels);

    if (initResult != SoLoud::SO_NO_ERROR)
    {
        std::cerr << "Failed to initialize SoLoud with miniaudio backend: "
                  << engine.getErrorString(initResult) << '\n';
        return 1;
    }

    auto samples = make_tone();
    SoLoud::Wav tone;

    const auto loadResult = tone.loadRawWave(
        samples.data(),
        static_cast<unsigned int>(samples.size()),
        static_cast<float>(k_sample_rate),
        k_channels,
        true,
        false);

    if (loadResult != SoLoud::SO_NO_ERROR)
    {
        std::cerr << "Failed to create demo tone: "
                  << engine.getErrorString(loadResult) << '\n';
        engine.deinit();
        return 1;
    }

    tone.setVolume(1.0f);
    engine.play(tone);

    std::cout << "Playing a " << k_duration_seconds << " second sine tone via SoLoud + miniaudio..." << '\n';
    std::this_thread::sleep_for(std::chrono::duration<float>(k_duration_seconds + 0.25f));

    engine.deinit();
    return 0;
}
