# audio_system

一个基于 C++20 的 `SoLoud + miniaudio` 音频工程模板。

## 目录结构

- `deps/soloud`: SoLoud 子模块
- `deps/SDL2`: SDL2 子模块，供 SoLoud 官方图形 demo 使用
- `src/main.cpp`: 最小可运行示例，会直接生成一段正弦波并播放
- `src/play3d_demo.cpp`: 多音源 3D 音频示例，使用 `data/spatial_demo` 里的真实素材

## SoLoud 官方 Demo

当前根 CMake 已接入一批不需要额外第三方依赖的 SoLoud 官方 demo：

- `audio_system_soloud_enumerate`
- `audio_system_soloud_null`
- `audio_system_soloud_simplest`
- `audio_system_soloud_welcome`
- `audio_system_soloud_env`
- `audio_system_soloud_megademo`

其中：

- `enumerate` 用来查看当前编进来的 SoLoud backend
- `null` 演示 `NULLDRIVER`
- `simplest` 演示 `Speech`
- `welcome` 演示 `Speech + Wav + Openmpt`
- `env` 使用官方 `SDL2 + ImGui + GLEW` 图形链路，演示环境声与滤镜
- `megademo` 使用官方 `SDL2 + ImGui + GLEW` 图形链路，汇总多种官方子 demo

当前仍然没有并入 `piano`，因为它依赖 `RtMidi`。

## 后端实现

当前项目直接复用 SoLoud 自带的 `src/backend/miniaudio/soloud_miniaudio.cpp` 作为 miniaudio 后端实现。

## Windows 构建

在 Visual Studio Developer Command Prompt 或先执行 `vcvars64.bat` 后：

```powershell
cmake -S . -B build-msvc -G Ninja
cmake --build build-msvc
.\build-msvc\audio_system_demo.exe
.\build-msvc\audio_system_play3d_demo.exe
.\build-msvc\audio_system_soloud_enumerate.exe
.\build-msvc\audio_system_soloud_null.exe
.\build-msvc\audio_system_soloud_simplest.exe
.\build-msvc\audio_system_soloud_welcome.exe
.\build-msvc\audio_system_soloud_env.exe
.\build-msvc\audio_system_soloud_megademo.exe
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

`audio_system_play3d_demo` 会：

1. 初始化 `SoLoud::Soloud::MINIAUDIO`
2. 加载 `data/spatial_demo` 下的 3 个环境音和 1 个 click 提示音
3. 在监听者周围同时播放多个固定声源
4. 周期性地从一个绕圈移动的声源位置播放 click
5. 每帧调用 `update3dAudio()` 更新空间音频

`audio_system_soloud_welcome` 当前会：

1. 使用构建后自动复制到可执行目录下 `audio/windy_ambience.ogg` 的本地环境音
2. 正常运行语音播报流程
3. 尝试加载 `audio/BRUCE.S3M`

如果 `BRUCE.S3M` 或 `libopenmpt.dll` 不存在，它会像官方 demo 一样给出提示并继续退出，不会导致构建失败。

`audio_system_soloud_env` 和 `audio_system_soloud_megademo` 当前会：

1. 直接复用 SoLoud 官方 `SDL2 + ImGui + GLEW` demo framework
2. 在构建后自动把官方 `audio/` 和 `graphics/` 资源复制到目标输出目录
3. 使用当前仓库里的 `audio_system_runtime` 作为 SoLoud 运行库

后续你可以直接在这个工程上继续加：

- `Wav::load()` / `WavStream::load()` 文件播放
- Bus、Filter、3D audio
- 你自己的音频资源管理层

## Spatial Demo Data

空间音频示例使用的素材放在 [data/spatial_demo](/D:/WorkSpace/Repository/audio_system/data/spatial_demo)。

- [SOURCES.md](/D:/WorkSpace/Repository/audio_system/data/spatial_demo/SOURCES.md): 记录下载链接和来源页面
- 这些素材来自 OpenGameArt，对应页面标注为 CC0 / Public Domain

## SoLoud Official Demo Data

SoLoud 官方图形 demo 需要的资源放在 [data/soloud_official](/D:/WorkSpace/Repository/audio_system/data/soloud_official)。

- `audio/`: 从 SoLoud 官方发布包提取的 demo 音频
- `graphics/`: 从 SoLoud 官方发布包提取的图形资源

这些资源会在构建 `audio_system_soloud_welcome`、`audio_system_soloud_env` 和 `audio_system_soloud_megademo` 时自动复制到对应输出目录。
