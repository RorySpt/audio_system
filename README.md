# audio_system

一个基于 C++20 的 `SoLoud + miniaudio` 音频工程模板。

## 目录结构

- `deps/soloud`: SoLoud 子模块
- `deps/miniaudio`: miniaudio 子模块
- `src/main.cpp`: 最小可运行示例，会直接生成一段正弦波并播放
- `src/play3d_demo.cpp`: 多音源 3D 音频示例，使用 `data/spatial_demo` 里的真实素材

## 后端实现

当前项目直接复用 SoLoud 自带的 `src/backend/miniaudio/soloud_miniaudio.cpp` 作为 miniaudio 后端实现。

## Windows 构建

在 Visual Studio Developer Command Prompt 或先执行 `vcvars64.bat` 后：

```powershell
cmake -S . -B build-msvc -G Ninja
cmake --build build-msvc
.\build-msvc\audio_system_demo.exe
.\build-msvc\audio_system_play3d_demo.exe
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

后续你可以直接在这个工程上继续加：

- `Wav::load()` / `WavStream::load()` 文件播放
- Bus、Filter、3D audio
- 你自己的音频资源管理层

## Spatial Demo Data

空间音频示例使用的素材放在 [data/spatial_demo](/D:/WorkSpace/Repository/audio_system/data/spatial_demo)。

- [SOURCES.md](/D:/WorkSpace/Repository/audio_system/data/spatial_demo/SOURCES.md): 记录下载链接和来源页面
- 这些素材来自 OpenGameArt，对应页面标注为 CC0 / Public Domain
