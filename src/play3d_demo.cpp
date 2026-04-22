#include "soloud.h"
#include "soloud_wav.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
#include <thread>
#include <vector>

namespace
{
// 3D 音频示例仍然输出普通双声道，空间感由 SoLoud 的 3D 混音计算得到。
constexpr unsigned int k_sample_rate = 48000;
constexpr unsigned int k_channels = 2;
constexpr unsigned int k_buffer_size = 512;
constexpr float k_duration_seconds = 8.0f;
constexpr float k_frequency_hz = 660.0f;
constexpr float k_amplitude = 0.25f;
constexpr float k_step_seconds = 1.0f / 30.0f;
constexpr float k_orbit_radius = 3.0f;
constexpr float k_orbit_height = 0.0f;
constexpr float k_angular_speed = std::numbers::pi_v<float> * 0.75f;

std::vector<float> make_tone()
{
    // 生成一段可循环的单频正弦波，便于直接听出左右声像和距离变化。
    const auto frame_count = static_cast<std::size_t>(k_sample_rate * 1.0f);
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

    // 初始化 SoLoud，并显式指定 miniaudio 作为底层输出后端。
    const auto init_result = engine.init(
        SoLoud::Soloud::CLIP_ROUNDOFF,
        SoLoud::Soloud::MINIAUDIO,
        k_sample_rate,
        k_buffer_size,
        k_channels);

    if (init_result != SoLoud::SO_NO_ERROR)
    {
        std::cerr << "Failed to initialize SoLoud with miniaudio backend: "
                  << engine.getErrorString(init_result) << '\n';
        return 1;
    }

    auto samples = make_tone();
    SoLoud::Wav tone;

    // 用原始 PCM 数据构造一个可循环音源，后续用 play3d 播放。
    const auto load_result = tone.loadRawWave(
        samples.data(),
        static_cast<unsigned int>(samples.size()),
        static_cast<float>(k_sample_rate),
        k_channels,
        true,
        false);

    if (load_result != SoLoud::SO_NO_ERROR)
    {
        std::cerr << "Failed to create 3D demo tone: "
                  << engine.getErrorString(load_result) << '\n';
        engine.deinit();
        return 1;
    }

    tone.setLooping(true);
    // 设置 3D 衰减范围和模型，让声源绕圈时能明显感受到远近变化。
    tone.set3dMinMaxDistance(0.5f, 8.0f);
    tone.set3dAttenuation(SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);

    // 监听者放在原点，面朝 +Z，Y 轴向上。
    engine.set3dListenerParameters(
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f);

    float prev_x = k_orbit_radius;
    float prev_z = 0.0f;

    // 声源从监听者右侧开始播放。
    const auto voice = engine.play3d(tone, prev_x, k_orbit_height, prev_z);
    engine.set3dSourceMinMaxDistance(voice, 0.5f, 8.0f);
    engine.set3dSourceAttenuation(voice, SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);
    engine.update3dAudio();

    std::cout << "Playing looping tone with play3d. The source is orbiting around the listener for "
              << k_duration_seconds << " seconds..." << '\n';

    const auto step_count = static_cast<int>(k_duration_seconds / k_step_seconds);
    for (int step = 1; step <= step_count; ++step)
    {
        // 每一帧都更新声源位置和速度，再调用 update3dAudio() 提交到 SoLoud。
        const auto time_seconds = static_cast<float>(step) * k_step_seconds;
        const auto angle = time_seconds * k_angular_speed;
        const auto pos_x = std::cos(angle) * k_orbit_radius;
        const auto pos_z = std::sin(angle) * k_orbit_radius;
        const auto vel_x = (pos_x - prev_x) / k_step_seconds;
        const auto vel_z = (pos_z - prev_z) / k_step_seconds;

        engine.set3dSourcePosition(voice, pos_x, k_orbit_height, pos_z);
        engine.set3dSourceVelocity(voice, vel_x, 0.0f, vel_z);
        engine.update3dAudio();

        prev_x = pos_x;
        prev_z = pos_z;

        std::this_thread::sleep_for(std::chrono::duration<float>(k_step_seconds));
    }

    engine.deinit();
    return 0;
}
