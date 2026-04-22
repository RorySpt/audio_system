#include "soloud.h"
#include "soloud_wav.h"

#include <wildmidi_lib.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr unsigned int k_sample_rate = 48000;
constexpr unsigned int k_channels = 2;
constexpr unsigned int k_buffer_size = 512;
constexpr std::uint32_t k_chunk_frames = 4096;

static_assert(sizeof(std::int16_t) == sizeof(short));

class WildMidiShutdownGuard
{
public:
    ~WildMidiShutdownGuard()
    {
        WildMidi_Shutdown();
    }
};

class WildMidiSongGuard
{
public:
    explicit WildMidiSongGuard(midi* handle)
        : handle_(handle)
    {
    }

    ~WildMidiSongGuard()
    {
        WildMidi_Close(handle_);
    }

private:
    midi* handle_{};
};

void append_variable_length(std::vector<std::uint8_t>& track, std::uint32_t value)
{
    std::array<std::uint8_t, 5> buffer{};
    int count = 0;

    buffer[count++] = static_cast<std::uint8_t>(value & 0x7F);
    while ((value >>= 7U) != 0)
    {
        buffer[count++] = static_cast<std::uint8_t>((value & 0x7F) | 0x80);
    }

    while (count-- > 0)
    {
        track.push_back(buffer[count]);
    }
}

void append_bytes(std::vector<std::uint8_t>& out, std::initializer_list<std::uint8_t> bytes)
{
    out.insert(out.end(), bytes.begin(), bytes.end());
}

std::vector<std::uint8_t> make_midi_demo()
{
    std::vector<std::uint8_t> track;

    append_variable_length(track, 0);
    append_bytes(track, {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20}); // 120 BPM.

    append_variable_length(track, 0);
    append_bytes(track, {0xC0, 0x00}); // Piano.
    append_variable_length(track, 0);
    append_bytes(track, {0xC1, 0x30}); // String ensemble.
    append_variable_length(track, 0);
    append_bytes(track, {0xB0, 0x07, 100});
    append_variable_length(track, 0);
    append_bytes(track, {0xB1, 0x07, 72});

    append_variable_length(track, 0);
    append_bytes(track, {0x91, 55, 84}); // G3 drone.
    append_variable_length(track, 0);
    append_bytes(track, {0x90, 60, 108}); // C4.
    append_variable_length(track, 0);
    append_bytes(track, {0x99, 36, 118}); // Kick.

    append_variable_length(track, 240);
    append_bytes(track, {0x99, 42, 76}); // Closed hat.
    append_variable_length(track, 240);
    append_bytes(track, {0x80, 60, 0});
    append_variable_length(track, 0);
    append_bytes(track, {0x90, 64, 108}); // E4.
    append_variable_length(track, 0);
    append_bytes(track, {0x99, 38, 108}); // Snare.

    append_variable_length(track, 240);
    append_bytes(track, {0x99, 42, 76});
    append_variable_length(track, 240);
    append_bytes(track, {0x80, 64, 0});
    append_variable_length(track, 0);
    append_bytes(track, {0x90, 67, 108}); // G4.
    append_variable_length(track, 0);
    append_bytes(track, {0x99, 36, 118});

    append_variable_length(track, 240);
    append_bytes(track, {0x99, 42, 76});
    append_variable_length(track, 240);
    append_bytes(track, {0x80, 67, 0});
    append_variable_length(track, 0);
    append_bytes(track, {0x90, 72, 112}); // C5.
    append_variable_length(track, 0);
    append_bytes(track, {0x99, 38, 108});

    append_variable_length(track, 240);
    append_bytes(track, {0x99, 42, 76});
    append_variable_length(track, 240);
    append_bytes(track, {0x80, 72, 0});
    append_variable_length(track, 0);
    append_bytes(track, {0x99, 49, 96}); // Crash.

    append_variable_length(track, 480);
    append_bytes(track, {0x81, 55, 0});
    append_variable_length(track, 0);
    append_bytes(track, {0xFF, 0x2F, 0x00});

    std::vector<std::uint8_t> midi;
    append_bytes(midi, {'M', 'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x01, 0xE0});
    append_bytes(midi, {'M', 'T', 'r', 'k'});

    const std::uint32_t track_size = static_cast<std::uint32_t>(track.size());
    append_bytes(midi,
        {
            static_cast<std::uint8_t>((track_size >> 24) & 0xFF),
            static_cast<std::uint8_t>((track_size >> 16) & 0xFF),
            static_cast<std::uint8_t>((track_size >> 8) & 0xFF),
            static_cast<std::uint8_t>(track_size & 0xFF)
        });
    midi.insert(midi.end(), track.begin(), track.end());
    return midi;
}

std::filesystem::path get_source_dir()
{
    return std::filesystem::path(AUDIO_SYSTEM_SOURCE_DIR);
}

std::filesystem::path create_wildmidi_config()
{
    const auto patch_root = get_source_dir() / "data" / "timgm6mb_pat" / "pat";
    const auto timidity_config = patch_root / "timidity.cfg";

    if (!std::filesystem::exists(timidity_config))
    {
        throw std::runtime_error("Missing data/timgm6mb_pat/pat/timidity.cfg. Extract the official .pat package first.");
    }

    const auto generated_dir = get_source_dir() / "build-generated";
    std::filesystem::create_directories(generated_dir);

    const auto wildmidi_config = generated_dir / "wildmidi-timgm6mb.cfg";
    std::ofstream out(wildmidi_config, std::ios::binary);
    out << "dir " << patch_root.generic_string() << "\n";
    out << "source timidity.cfg\n";
    return wildmidi_config;
}

std::vector<std::int16_t> render_midi_with_wildmidi(const std::filesystem::path& config_path)
{
    WildMidi_ClearError();

    if (WildMidi_Init(config_path.string().c_str(), static_cast<std::uint16_t>(k_sample_rate),
            WM_MO_ENHANCED_RESAMPLING | WM_MO_REVERB | WM_MO_ROUNDTEMPO) < 0)
    {
        const char* error = WildMidi_GetError();
        throw std::runtime_error(error != nullptr ? error : "WildMidi_Init failed");
    }

    WildMidiShutdownGuard shutdown_guard;

    const auto midi_bytes = make_midi_demo();
    midi* song = WildMidi_OpenBuffer(midi_bytes.data(), static_cast<std::uint32_t>(midi_bytes.size()));
    if (song == nullptr)
    {
        const char* error = WildMidi_GetError();
        throw std::runtime_error(error != nullptr ? error : "WildMidi_OpenBuffer failed");
    }

    WildMidiSongGuard song_guard(song);

    std::vector<std::int16_t> pcm;
    std::vector<std::int16_t> chunk(k_chunk_frames * k_channels);

    while (true)
    {
        const auto bytes_requested = static_cast<std::uint32_t>(chunk.size() * sizeof(std::int16_t));
        const int bytes_written = WildMidi_GetOutput(song, reinterpret_cast<int8_t*>(chunk.data()), bytes_requested);
        if (bytes_written < 0)
        {
            const char* error = WildMidi_GetError();
            throw std::runtime_error(error != nullptr ? error : "WildMidi_GetOutput failed");
        }
        if (bytes_written == 0)
        {
            break;
        }

        const auto samples_written = static_cast<std::size_t>(bytes_written / sizeof(std::int16_t));
        pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(samples_written));
    }

    return pcm;
}
}

int main()
{
    try
    {
        const auto config_path = create_wildmidi_config();
        auto pcm = render_midi_with_wildmidi(config_path);

        if (pcm.empty())
        {
            std::cerr << "WildMIDI rendered no audio." << '\n';
            return 1;
        }

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

        SoLoud::Wav song;
        const auto loadResult = song.loadRawWave16(
            reinterpret_cast<short*>(pcm.data()),
            static_cast<unsigned int>(pcm.size()),
            static_cast<float>(k_sample_rate),
            k_channels);

        if (loadResult != SoLoud::SO_NO_ERROR)
        {
            std::cerr << "Failed to load rendered MIDI into SoLoud: "
                      << engine.getErrorString(loadResult) << '\n';
            engine.deinit();
            return 1;
        }

        song.setVolume(1.0f);
        engine.play(song);

        const auto duration_seconds = static_cast<float>(pcm.size()) / static_cast<float>(k_sample_rate * k_channels);
        std::cout << "Playing embedded MIDI rendered through WildMIDI using TimGM6mb .pat patches..." << '\n';
        std::this_thread::sleep_for(std::chrono::duration<float>(duration_seconds + 0.25f));

        engine.deinit();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "MIDI demo failed: " << ex.what() << '\n';
        return 1;
    }
}
