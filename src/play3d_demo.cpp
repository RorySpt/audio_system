#include "soloud.h"
#include "soloud_wav.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <string>
#include <thread>

namespace
{
// 3D 音频示例仍然输出普通双声道，空间感由 SoLoud 的 3D 混音计算得到。
constexpr unsigned int k_sample_rate = 48000;
constexpr unsigned int k_channels = 2;
constexpr unsigned int k_buffer_size = 512;
constexpr float k_duration_seconds = 12.0f;
constexpr float k_step_seconds = 1.0f / 30.0f;
constexpr float k_orbit_radius = 3.0f;
constexpr float k_orbit_height = 0.0f;
constexpr float k_angular_speed = std::numbers::pi_v<float> * 0.5f;
constexpr float k_click_interval_seconds = 0.9f;

std::filesystem::path get_source_dir()
{
    return std::filesystem::path(AUDIO_SYSTEM_SOURCE_DIR);
}

std::filesystem::path get_spatial_demo_dir()
{
    return get_source_dir() / "data" / "spatial_demo";
}

bool load_sound(SoLoud::Wav& sound, const std::filesystem::path& path, const char* label)
{
    const auto result = sound.load(path.string().c_str());
    if (result != SoLoud::SO_NO_ERROR)
    {
        std::cerr << "Failed to load " << label << ": " << path.string() << '\n';
        return false;
    }

    return true;
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

    const auto spatial_demo_dir = get_spatial_demo_dir();
    const auto wind_path = spatial_demo_dir / "wind1.wav";
    const auto crickets_path = spatial_demo_dir / "crickets_loop.mp3";
    const auto click_path = spatial_demo_dir / "click.wav";

    SoLoud::Wav wind_sound;
    SoLoud::Wav crickets_sound;
    SoLoud::Wav click_sound;

    // 读取放在 data/spatial_demo 下的真实音频素材。
    if (!load_sound(wind_sound, wind_path, "wind1.wav") ||
        !load_sound(crickets_sound, crickets_path, "crickets_loop.mp3") ||
        !load_sound(click_sound, click_path, "click.wav"))
    {
        engine.deinit();
        return 1;
    }

    // 两个静态环境声 + 一个移动提示声，让场景里始终有多个声源同时存在。
    wind_sound.setLooping(true);
    crickets_sound.setLooping(true);

    wind_sound.setVolume(0.65f);
    crickets_sound.setVolume(0.75f);
    click_sound.setVolume(1.0f);

    wind_sound.set3dMinMaxDistance(1.0f, 18.0f);
    crickets_sound.set3dMinMaxDistance(1.0f, 18.0f);
    click_sound.set3dMinMaxDistance(0.5f, 10.0f);

    wind_sound.set3dAttenuation(SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);
    crickets_sound.set3dAttenuation(SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);
    click_sound.set3dAttenuation(SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);

    // 监听者放在原点，面朝 +Z，Y 轴向上。
    engine.set3dListenerParameters(
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f);

    float prev_x = k_orbit_radius;
    float prev_z = 0.0f;
    float next_click_time_seconds = 0.0f;

    // 两个固定环境声源分布在监听者周围。
    const auto wind_voice = engine.play3d(wind_sound, -5.0f, 0.0f, 3.0f);
    const auto crickets_voice = engine.play3d(crickets_sound, 4.0f, 0.0f, -4.0f);

    engine.set3dSourceMinMaxDistance(wind_voice, 1.0f, 18.0f);
    engine.set3dSourceMinMaxDistance(crickets_voice, 1.0f, 18.0f);
    engine.update3dAudio();

    std::cout << "Playing multi-source spatial audio demo with downloaded sounds for "
              << k_duration_seconds << " seconds..." << '\n';

    const auto step_count = static_cast<int>(k_duration_seconds / k_step_seconds);
    for (int step = 1; step <= step_count; ++step)
    {
        // 让 click 声像一个沿圆轨道巡航的提示音，不断从不同方向出现。
        const auto time_seconds = static_cast<float>(step) * k_step_seconds;
        const auto angle = time_seconds * k_angular_speed;
        const auto pos_x = std::cos(angle) * k_orbit_radius;
        const auto pos_z = std::sin(angle) * k_orbit_radius;
        const auto vel_x = (pos_x - prev_x) / k_step_seconds;
        const auto vel_z = (pos_z - prev_z) / k_step_seconds;

        if (time_seconds >= next_click_time_seconds)
        {
            const auto click_voice = engine.play3d(click_sound, pos_x, k_orbit_height, pos_z, vel_x, 0.0f, vel_z);
            engine.set3dSourceMinMaxDistance(click_voice, 0.5f, 10.0f);
            engine.set3dSourceAttenuation(click_voice, SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);
            next_click_time_seconds += k_click_interval_seconds;
        }

        engine.update3dAudio();

        prev_x = pos_x;
        prev_z = pos_z;

        std::this_thread::sleep_for(std::chrono::duration<float>(k_step_seconds));
    }

    engine.deinit();
    return 0;
}
