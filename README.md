# audio_system

一个基于 C++20 的 `SoLoud + miniaudio` 音频工程模板。

## 目录结构

- `deps/soloud`: SoLoud 子模块
- `deps/glfw`: GLFW 子模块，供图形 demo 创建窗口和 Vulkan surface
- `deps/imgui`: Dear ImGui 子模块，供图形 demo 绘制调试面板
- `deps/auto-vk`: Auto-Vk 子模块，供图形 demo 使用 Vulkan 辅助层
- `src/main.cpp`: 最小可运行示例，会直接生成一段正弦波并播放
- `src/play3d_demo.cpp`: 多音源 3D 音频示例，使用 `data/spatial_demo` 里的真实素材
- `src/auto_vk_imgui_demo.cpp`: GLFW + ImGui + Auto-Vk 图形框架入口，用来承载 SoLoud 官方图形 demo

## SoLoud 官方 Demo

当前根 CMake 已接入一批不需要额外第三方依赖的 SoLoud 官方 demo：

- `audio_system_soloud_enumerate`
- `audio_system_soloud_null`
- `audio_system_soloud_simplest`

其中：

- `enumerate` 用来查看当前编进来的 SoLoud backend
- `null` 演示 `NULLDRIVER`
- `simplest` 演示 `Speech`

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
.\build-msvc\audio_system_auto_vk_imgui_demo.exe
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

`audio_system_auto_vk_imgui_demo` 当前会：

1. 使用 GLFW 创建窗口和 Vulkan surface
2. 使用 ImGui 官方 GLFW/Vulkan backend 绘制 UI
3. 用 Auto-Vk/Vulkan-Hpp 管理 Vulkan instance、device、图形队列、纹理上传和 descriptor pool
4. 直接链接 SoLoud 官方 `demos/megademo` 的菜单与子 demo 源码
5. 在 `data/soloud_official` 下加载官方 `audio/` 与 `graphics/` 资源
6. 复刻 SoLoud 官方 `megademo` 的界面、动画和交互展示内容

其中 `space` 子 demo 依赖 OpenMPT 和 `BRUCE.S3M`，当前按本工程前面的清理要求没有重新接入 OpenMPT，因此菜单入口会显示未启用提示。
另外本地当前缺少 `adversary.pt3_2ay.zak`、`Angel_Project.sid_sid.zak`、`ted_storm.prg_ted.zak` 这 3 个官方素材，`ay` 和 `tedsid` 子 demo 的界面已接入，但对应音乐需要补齐素材后才完整。

这个 target 需要本机安装 Vulkan SDK。若不想构建它，可以在配置时关闭：

```powershell
cmake -S . -B build-msvc -G Ninja -DAUDIO_SYSTEM_BUILD_AUTO_VK_IMGUI_DEMO=OFF
```

后续你可以直接在这个工程上继续加：

- `Wav::load()` / `WavStream::load()` 文件播放
- Bus、Filter、3D audio
- 你自己的音频资源管理层

## Spatial Demo Data

空间音频示例使用的素材放在 [data/spatial_demo](/D:/WorkSpace/Repository/audio_system/data/spatial_demo)。

- [SOURCES.md](/D:/WorkSpace/Repository/audio_system/data/spatial_demo/SOURCES.md): 记录下载链接和来源页面
- 这些素材来自 OpenGameArt，对应页面标注为 CC0 / Public Domain

## SoLoud Official Demo Data

SoLoud 官方 demo 需要的资源放在 [data/soloud_official](/D:/WorkSpace/Repository/audio_system/data/soloud_official)。

- `audio/`: 从 SoLoud 官方发布包提取的 demo 音频
