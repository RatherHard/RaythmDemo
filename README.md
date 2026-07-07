# RaythmDemo

RaythmDemo 是一个基于 **C++20** 的跨平台音乐节奏游戏 Demo，目标平台为 **Windows** 和 **Linux**。项目采用自研模块化架构，重点探索低延迟音频播放、精确音画同步、Vulkan 2D 谱面渲染、输入判定与可测试的 Game 纯逻辑闭环。

当前项目已经具备最小可玩切片的核心骨架：启动时加载内置谱面与 Ogg 音频，以 Audio playback time 驱动谱面推进，通过 SDL3 输入映射 D/F/J/K 四轨按键，并使用 Vulkan colored rect 绘制轨道、判定线和 note。

---

## 当前状态

已完成：

- CMake / CTest / vcpkg 工程基础。
- SDL3 窗口封装、事件泵和输入状态系统。
- Core 应用生命周期、主循环和时间系统。
- miniaudio 音频引擎封装、Ogg Vorbis 内存加载与 playback time。
- Vulkan / Volk 渲染器、swapchain 管理、有限等待状态分类和最小 2D colored rect 绘制。
- Game 层谱面加载、4K 输入映射、GameplaySession tap 判定、combo / miss / judgement 统计与 GameplaySnapshot。
- `.mc` 到 RaythmDemo JSON 谱面格式的转换工具。
- 最小 Core 运行时游玩闭环：`assets/charts/00001/00001.json` + `00001.ogg`。

仍在推进：

- Vulkan-capable SDL 真实运行环境下的人工游玩复测。
- Linux 构建、CTest、SDL3 / Vulkan loader / miniaudio 后端组合验证。
- 完整 runtime state、音频 / 视觉 offset 校准、完整 score、hold sustain / release、暂停 / 选曲 / 结算界面。

---

## 技术栈

| 模块 | 技术选型 | 说明 |
|------|----------|------|
| 语言 | C++20 | 不使用 C++23 特性 |
| 构建 / 测试 | CMake 3.20+ / CTest | Visual Studio 多配置生成器下测试需指定 `-C Debug` |
| 依赖管理 | vcpkg manifest | 依赖声明见 `vcpkg.json` |
| 窗口与输入 | SDL3 | 窗口、事件、键盘 / 鼠标输入 |
| 图形渲染 | Vulkan / Volk | Vulkan API 必须通过 `#include <volk.h>` 使用 |
| 音频 | miniaudio | 单头文件音频引擎，已启用 Vorbis 解码 backend |
| JSON | nlohmann/json | 谱面解析与转换工具输出 |
| 工具脚本 | Python 3 | `.mc` 谱面转换测试和 CLI |

---

## 目录结构

```text
RaythmDemo/
├── assets/                 # 运行时资源：谱面、音频等
│   └── charts/00001/       # 当前内置启动谱面与 Ogg 音频
├── docs/                   # 项目文档、开发日志、开发计划、技术设计文档
├── include/                # 对外头文件，按模块划分
│   ├── Audio/
│   ├── Core/
│   ├── Game/
│   ├── Platform/
│   └── Render/
├── src/                    # 源文件，目录结构与 include 镜像
│   ├── Audio/
│   ├── Core/
│   ├── Game/
│   ├── Platform/
│   ├── Render/
│   ├── main.cpp
│   └── miniaudio_impl.cpp
├── tests/                  # CTest 测试源码
├── third_party/            # vendored 第三方源码
│   └── miniaudio/
├── tools/                  # 开发工具脚本
│   └── mc_to_chart_json.py
├── CMakeLists.txt
└── vcpkg.json
```

---

## 环境要求

### 通用要求

- CMake 3.20+
- 支持 C++20 的编译器
- vcpkg
- Python 3（用于 `.mc` 转换器测试；缺失时 `RaythmDemoMcConverterPyTest` 不会注册）
- Vulkan SDK 或系统 Vulkan loader / driver

### Windows

推荐：

- Windows 10 / 11
- Visual Studio 2022 Build Tools 或 Visual Studio 2022
- Vulkan-capable GPU / driver
- vcpkg manifest mode

### Linux

需要额外确认：

- SDL3 可创建 Vulkan surface
- Vulkan loader / driver 已安装
- pthread / dl 链接可用
- miniaudio 可用音频后端可初始化

Linux 构建验证仍是当前后续计划之一。

---

## 快速开始

以下命令以仓库根目录为工作目录。

### 1. 配置工程

如果使用 vcpkg toolchain：

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

如果你的环境已通过 IDE 或 CMake Presets 注入 vcpkg toolchain，可按实际配置执行等价的 CMake configure 命令。

### 2. 构建 Debug

```bash
cmake --build build --config Debug
```

构建 `RaythmDemo` 时，CMake 会通过 `POST_BUILD` 将 `assets/` 复制到可执行文件目录，例如 Windows Debug 构建下的 `build/Debug/assets`。

### 3. 运行测试

Visual Studio 等多配置生成器下需要指定配置：

```bash
ctest --test-dir build -C Debug --output-on-failure
```

当前主要 CTest 目标包括：

- `RaythmDemoEnvironmentTest`
- `RaythmDemoWindowTest`
- `RaythmDemoInputStateTest`
- `RaythmDemoAudioTest`
- `RaythmDemoGameTest`
- `RaythmDemoGameplaySessionTest`
- `RaythmDemoCoreTest`
- `RaythmDemoCoreGameplayRuntimeTest`
- `RaythmDemoRenderTest`
- `RaythmDemoRenderStrategyTest`
- `RaythmDemoMcConverterPyTest`（需要 Python 3）

### 4. 运行游戏 Demo

Windows Debug 构建示例：

```bash
./build/Debug/RaythmDemo.exe
```

Linux 或单配置生成器下，可执行文件路径按生成器输出目录调整。

> 注意：运行主程序需要 SDL3 能创建 Vulkan-capable window，并且系统具备可用 Vulkan driver / loader。部分无 GPU、远程、沙箱或 SDL video driver 受限环境可能只能运行纯逻辑测试，无法打开真实窗口。

---

## 操作方式

当前内置 Demo 为 4K 下落式输入：

| 轨道 | 默认按键 |
|------|----------|
| Lane 0 | D |
| Lane 1 | F |
| Lane 2 | J |
| Lane 3 | K |

启动时默认加载：

- 谱面：`assets/charts/00001/00001.json`
- 音频：`assets/charts/00001/00001.ogg`

---

## 谱面转换工具

项目提供 Malody 风格 `.mc` 谱面到 RaythmDemo JSON 谱面格式的转换脚本：

```bash
python tools/mc_to_chart_json.py path/to/chart.mc --stdout --pretty
```

> 当前仓库的内置启动资源是已经转换后的 `assets/charts/00001/00001.json` 与 `00001.ogg`；如果要试用转换器，请将待转换的 Malody `.mc` 谱面放在项目内的资源目录中，或把上面命令中的 `path/to/chart.mc` 替换为你的实际谱面路径。

转换器当前面向 4K 谱面，输出字段与 `ChartLoader` 兼容，主要覆盖：

- `meta.song.*` → 曲名 / 曲师
- `meta.creator` → 谱师
- BPM 时间线
- tap / hold note
- 音频事件 → `meta.path` 与 `meta.offset`

运行转换器测试：

```bash
python -m unittest tests/test_mc_to_chart_json.py -v
```

---

## 开发约定

### 模块命名空间

所有项目类型、函数和常量应位于 `Raythm` 根命名空间下，并按模块继续划分二级命名空间：

- `Raythm::Core`
- `Raythm::Platform`
- `Raythm::Audio`
- `Raythm::Render`
- `Raythm::Game`

### C++ 风格

- 使用 C++20。
- 遵循 RAII，禁止裸 `new` / `delete`。
- 优先使用智能指针和标准容器。
- 类名使用 PascalCase，函数和变量使用 camelCase，常量使用 UPPER_SNAKE_CASE。
- 成员变量使用 `m_` 前缀。
- 缩进 4 空格，Allman 风格花括号。

### Vulkan / Volk 红线

任何需要调用 Vulkan API 的文件必须：

```cpp
#include <volk.h>
```

禁止直接包含：

```cpp
#include <vulkan/vulkan.h>
```

Vulkan 初始化顺序必须保证：

1. `volkInitialize()` 是程序生命周期中的第一个 Vulkan 函数。
2. 创建 `VkInstance` 后立即调用 `volkLoadInstance(instance)`。
3. 创建 logical device 后加载 device-level function。

### 测试接入

新增测试文件时必须：

1. 在 `CMakeLists.txt` 中加入对应测试目标。
2. 使用 `add_test()` 注册到 CTest。
3. 使用 `ctest --test-dir build -C Debug --output-on-failure` 验证测试可发现并可执行。

---

## 文档入口

- `docs/开发日志.md`：阶段性进展、验证结果、遗留问题和下一步计划。
- `docs/开发计划.md`：已完成计划、短期计划、阶段目标和验证依据。
- `docs/技术设计文档/核心技术选型.md`：核心技术选型基线。
- `docs/玩法设计文档/`：玩法与谱面格式相关设计文档。

开发前请优先阅读 `docs/开发日志.md` 与 `docs/开发计划.md`，并以代码和实际验证结果为准同步修正文档。

---

## 常用命令

```bash
# 配置
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# 构建
cmake --build build --config Debug

# 全量测试
ctest --test-dir build -C Debug --output-on-failure

# 只跑 Game / GameplaySession 相关测试
ctest --test-dir build -C Debug -R "Game|GameplaySession" --output-on-failure

# 只跑运行时驱动与 Render 策略相关测试
ctest --test-dir build -C Debug -R "CoreGameplayRuntime|RenderStrategy|Render" --output-on-failure

# 转换 .mc 谱面到 stdout
python tools/mc_to_chart_json.py path/to/chart.mc --stdout --pretty
```

---

## 许可证

当前仓库尚未添加许可证文件。发布、分发或开源前请先补充明确的 `LICENSE`。
