# audio_system

一个基于 C++20 的 `SoLoud + miniaudio` 音频工程模板。

## 目录结构

- `deps/soloud`: SoLoud 子模块
- `deps/miniaudio`: miniaudio 子模块
- `src/main.cpp`: 最小可运行示例，会直接生成一段正弦波并播放
- `src/play3d_demo.cpp`: 最小 3D 音频示例，演示 `play3d()`、listener 和 source 更新

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
2. 生成一段循环正弦波
3. 用 `play3d()` 在监听者周围做圆周运动
4. 每帧调用 `set3dSourcePosition()`、`set3dSourceVelocity()` 和 `update3dAudio()` 更新 3D 音频

后续你可以直接在这个工程上继续加：

- `Wav::load()` / `WavStream::load()` 文件播放
- Bus、Filter、3D audio
- 你自己的音频资源管理层
