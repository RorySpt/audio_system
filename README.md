# audio_system

一个基于 C++20 的 `SoLoud + miniaudio` 音频工程模板。

## 目录结构

- `deps/soloud`: SoLoud 子模块
- `deps/miniaudio`: miniaudio 子模块
- `deps/wildmidi`: WildMIDI 子模块，用于把 `.mid` 通过 `.pat` 音色库渲染成 PCM
- `src/soloud_miniaudio_backend.cpp`: 项目内自定义的 SoLoud miniaudio backend，显式使用 `deps/miniaudio/miniaudio.h`
- `src/main.cpp`: 最小可运行示例，会直接生成一段正弦波并播放
- `src/midi_demo.cpp`: 使用官方 TimGM6mb `.pat` 音色库渲染内置 MIDI，并交给 SoLoud 播放的 demo

## 为什么有自定义 backend

SoLoud 仓库内自带了一份较旧的 `miniaudio.h`。为了让项目真正使用 `deps/miniaudio` 这份子模块，这里没有直接编译
`deps/soloud/src/backend/miniaudio/soloud_miniaudio.cpp`，而是改为在项目里提供一个兼容的 backend 实现。

## Windows 构建

在 Visual Studio Developer Command Prompt 或先执行 `vcvars64.bat` 后：

```powershell
cmake -S . -B build-msvc -G Ninja
cmake --build build-msvc
.\build-msvc\audio_system_demo.exe
.\build-msvc\audio_system_midi_demo.exe
```

如果你在普通 PowerShell 里构建，需要先注入 MSVC 环境，例如：

```powershell
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cmake -S . -B build-msvc -G Ninja && cmake --build build-msvc'
```

## 当前示例

`audio_system_demo` 会：

1. 用 `SoLoud::Soloud::MINIAUDIO` 初始化音频后端
2. 生成一段 48kHz、双声道、440Hz 的正弦波
3. 播放约 2.5 秒后退出

`audio_system_midi_demo` 会：

1. 使用 `WildMIDI` 加载内置的一段最小 MIDI
2. 使用 `data/timgm6mb_pat/pat` 下面的官方 TimGM6mb `.pat` 音色库渲染成 16-bit stereo PCM
3. 再把这段 PCM 通过 `SoLoud::Wav` 交给 `SoLoud + miniaudio` 播放

这意味着 `.pat` 官方音色现在已经真正参与到发声链路中，而不是只停留在资源目录里。

后续你可以直接在这个工程上继续加：

- `Wav::load()` / `WavStream::load()` 文件播放
- Bus、Filter、3D audio
- 你自己的音频资源管理层

## data 目录说明

当前 [data](/D:/WorkSpace/Repository/audio_system/data) 下面的 `timgm6mb_pat.zip` 是官方的 General MIDI `.pat` 音色库。

- 这类素材适合给 `.mid` / `.abc` 之类的 MIDI 播放链路当乐器采样库用
- 它不是 `wav/mp3/flac/ogg`，所以不能直接用 `SoLoud::Wav::load()` 播放
- 现在项目里的 `audio_system_midi_demo` 已经通过 `WildMIDI` 把这批 `.pat` 真正用起来了

项目会在运行时生成一个 `build-generated/wildmidi-timgm6mb.cfg`，里面会把 patch 根目录指向本地的 `data/timgm6mb_pat/pat`，再 `source timidity.cfg`。

因此需要保证下面这个文件存在：

- [timidity.cfg](/D:/WorkSpace/Repository/audio_system/data/timgm6mb_pat/pat/timidity.cfg)

如果你只有 zip，还没解压，那么先把 `timgm6mb_pat.zip` 解压到 [data](/D:/WorkSpace/Repository/audio_system/data) 下即可。

## 许可证说明

`WildMIDI` 使用 LGPLv3 / GPLv3 许可证。当前工程为了让 demo 最简单可跑，链接的是它的静态库。

如果你后面准备把这个方案用于正式分发，建议先重新评估：

- 是否改成动态链接 `WildMIDI`
- 你的分发方式是否满足对应许可证要求
