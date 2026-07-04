# RaythmDemo - System Prompts

## Tool usage constraints

- Do not call `apply_patch` from Bash.
- Use Claude Code's Edit or MultiEdit tools for file changes.
- When using the Read tool for paginated documents, omit the `pages` parameter if no page range is specified. Never pass `pages: ""`. Valid values are like `"1"`, `"1-5"`, or `"10-20"`.
- If a tool rejects an argument, retry once with only the documented required arguments.

---

## 项目概述

本项目是一个基于 C++ 的跨平台音乐节奏游戏（音游），目标平台为 **Windows** 和 **Linux**。采用自研架构，以实现极致低延迟、精确的音画同步，以及高性能的谱面渲染能力。

---

## 技术栈规范

**重要提示**：本项目的技术选型已在 `docs/技术设计文档/核心技术选型.md` 中明确定义。开发过程中 **严禁擅自更改技术方案**。

### 核心技术栈

| 模块 | 技术选型 | 版本/说明 |
|------|---------|----------|
| **开发语言** | C++ | C++20 标准 |
| **构建/测试系统** | CMake/CTest | 3.20+ |
| **开发环境** | VSCode | 配合 C++ 扩展 |
| **音频引擎** | miniaudio | 单头文件库，低延迟音频 |
| **图形渲染** | Vulkan/Volk | 现代跨平台图形 API |
| **窗口与输入** | SDL3 | 跨平台窗口、输入事件管理 |
| **JSON 解析** | nlohmann/json | Modern C++ JSON 库 |

### 技术选型约束

在开发过程中，你必须：

1. **遵循既定技术选型**：不得建议或使用其他图形 API（如 OpenGL、DirectX）、音频库（如 FMOD、PortAudio）
2. **保持 C++20 标准**：积极使用现代 C++ 特性（concepts、ranges、coroutines），但不得引入 C++23 特性
3. **平台兼容性优先**：任何新增代码必须同时支持 Windows 和 Linux

如果你认为某个技术选型存在问题，必须：
- 明确说明问题的严重性（性能瓶颈、安全漏洞、跨平台兼容性等）
- 提供详细的替代方案对比（包括迁移成本）
- 等待用户明确批准后方可变更

---

## 项目目录结构规范

项目遵循标准化的现代 C++ 工程目录结构，所有新建文件必须归入对应目录：

```
RaythmDemo/
├── .vscode/                # VSCode 工作区配置 (launch.json, tasks.json)
├── assets/                 # 运行时资源文件
├── docs/                   # 项目文档
│   ├── savedprompt/        # [系统保留] AI 提示词存档 (仅只读)
│   └── 技术设计文档/        # 架构设计、选型说明等
├── include/                # 核心头文件 (.hpp / .h) - 必须按模块划分子目录
│   ├── Core/               # 核心框架 (无平台依赖的基础设施、主循环、时间系统)
│   ├── Platform/           # SDL3 后端、窗口、输入、系统事件封装
│   ├── Audio/              # 音频引擎封装
│   ├── Render/             # Vulkan 渲染后端
│   └── Game/               # 游戏逻辑、谱面解析、实体
├── src/                    # 源代码文件 (.cpp) - 目录结构必须与 include 镜像一致
│   ├── Core/
│   ├── Platform/
│   ├── Audio/
│   ├── Render/
│   ├── Game/
│   └── main.cpp            # 程序主入口
├── third_party/            # 第三方源码库
│   └──miniaudio/           # miniaudio.h
├── tests/                  # 测试代码
├── CMakeLists.txt          # 顶层 CMake 构建脚本
└── vcpkg.json              # vcpkg 依赖清单
```

---

## 开发规范

### C++ 编码规范

#### 命名约定

- **命名空间**：项目中所有类型、函数和常量均应放在 `Raythm` 根 namespace 下，并按顶层模块目录继续划分二级命名空间；例如 `include/Platform` 与 `src/Platform` 中的代码应使用 `Raythm::Platform`，`Core`、`Audio`、`Render`、`Game` 模块同理使用 `Raythm::Core`、`Raythm::Audio`、`Raythm::Render`、`Raythm::Game`。
- **类名**：PascalCase
- **函数名**：camelCase
- **变量名**：camelCase
- **常量**：UPPER_SNAKE_CASE
- **成员变量**：带 `m_` 前缀的 camelCase
- **私有成员**：camelCase ，无额外前缀，通过访问控制区分

#### 代码风格

- **缩进**：4 空格（禁用 Tab）
- **花括号**：Allman 风格（左花括号单独一行）
- **行宽**：最大 120 字符
- **注释**：使用 `//` 单行注释，Doxygen 风格的函数注释

#### 代码注释规范

- **强制注释要求**：对于每一个类、类的成员和方法、变量、函数、结构体等均应有符合要求的注释
- **函数注释**：包含 `@param`、`@return`、`@note`（性能注意事项）
- **文件头注释**：每个源文件顶部包含文件说明、作者、日期
- **类注释**：Doxygen 格式的 `@brief` 说明类的职责
- **函数注释**：包含 `@param`、`@return`、`@note`（性能注意事项）
- **复杂逻辑**：算法实现前添加伪代码或步骤说明

### 内存安全规范

- 彻底遵循 RAII
- 优先使用智能指针，禁止裸 new/delete
- 用容器代替手动动态数组

### Vulkan 与 Volk 使用规范

- **头文件引入红线**: 任何需要调用 Vulkan API 的文件，必须使用 #include <volk.h>。绝对禁止直接出现 #include <vulkan/vulkan.h>。
- **初始化顺序**: 在程序生命周期中，必须保证 volkInitialize() 是被执行的第一个 Vulkan 函数。在创建 VkInstance 后，必须立刻调用 volkLoadInstance(instance)。

### clangd 配置同步规范

- 当 `CMakeLists.txt`、`.vscode/c_cpp_properties.json` 或项目目录结构发生会影响头文件搜索路径、预处理宏、C++ 标准、第三方依赖路径的变更时，必须同步检查并更新 `.clangd`。
- `.clangd` 中的 `CompileFlags.Add` 必须与当前 CMake 目标的有效编译参数保持一致，至少覆盖项目公共头文件目录 `include/`、`third_party/miniaudio/`、当前目标平台的 vcpkg 安装头文件目录，以及平台必要宏。
- Windows 与 Linux 的 include 路径和预处理宏必须分别核对；不得只按单一平台路径更新 `.clangd` 后即视为完成。
- 新增模块头文件目录或第三方头文件目录后，必须确认编辑器能够解析对应 include。
- 修改 `.clangd` 后，必须提示用户刷新对应语言服务缓存。

---

## 测试规范

在新增或修改测试代码时，必须使用 CTest 接入和执行测试。不要只创建测试文件；必须检查相关目录的 `CMakeLists.txt`，并按项目现有模式完成测试注册。
要求：

1. 新增测试源文件后，必须将其加入对应测试目标，或创建新的测试目标。
2. 必须在 `CMakeLists.txt` 中使用 `add_test()` 将测试注册到 CTest。
3. 测试目标名称和 CTest 测试名称应与项目现有命名风格保持一致。
4. 完成后必须通过 CTest 验证测试已被发现并可执行。
5. 如果修改了测试代码但无需新增 add_test() 入口，必须在回复中说明原因，例如复用了已有测试目标或已有 CTest 注册入口。

---

## 文档规范

### 文档版本记录规范

所有技术文档（位于 `docs/` 目录下）必须在文档末尾添加版本历史表格（savedprompt 目录除外）：

```markdown
## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-07-02 | 初始版本 |
| 1.1 | 2026-07-02 | 更新 XXX 模块说明 |
```

### 开发日志编写规范

项目开发日志统一维护在 `docs/开发日志.md`，用于记录项目阶段性进展、验证结果、遗留问题与下一步计划。

要求：

1. **更新时机**：完成环境配置、架构调整、模块实现、测试接入、重要 Bug 修复或阶段性验证后，必须更新开发日志。
2. **记录粒度**：按日期倒序记录，每个日期下可包含多个条目；同一天的相关工作应合并描述，避免碎片化流水账。
3. **条目结构**：每条记录至少包含“完成内容”“验证结果”“后续计划/遗留问题”。如涉及技术决策，应补充“决策说明”。
4. **证据关联**：记录中应引用相关文件、测试目标、提交或文档路径，便于追溯；不要只写笼统结论。
5. **事实优先**：只记录已经完成或已验证的事实；未完成事项必须明确标注为计划或待办。
6. **版本历史**：`docs/开发日志.md` 必须遵循文档版本记录规范，在文末维护版本历史表格。

### 开发计划更新规范

项目开发计划统一维护在 `docs/开发计划.md`，用于跟踪已完成计划、短期计划、阶段目标、阶段状态、验证依据和后续待办。

要求：

1. **更新时机**：完成模块实现、测试接入、架构调整、重要 Bug 修复、阶段性验证或计划范围变化后，必须同步更新开发计划。
2. **状态同步**：已完成事项必须从 `[ ]` 更新为 `[x]`；不再适用的计划必须删除或改写为当前真实待办，不能保留误导性的过期任务。
3. **阶段维护**：涉及某个阶段的工作完成后，必须更新该阶段的“状态”“完成情况”“短期待办”和“验证依据”。
4. **计划总览维护**：新增或完成重要事项时，必须同步更新“计划总览”中的“已完成计划”和“短期计划”。
5. **证据关联**：开发计划中的完成情况和验证依据应引用相关文件、CTest 目标、构建命令或开发日志条目，便于追溯。
6. **一致性优先**：如果开发计划、开发日志与实际代码状态不一致，以代码和实际验证结果为准，并在完成工作后同步修正 `docs/开发计划.md` 与 `docs/开发日志.md`。
7. **版本历史**：`docs/开发计划.md` 必须遵循文档版本记录规范，在文末维护版本历史表格。

### 权限说明

1. 禁止未经用户允许新增文档、修改文档
2. 文档只能放在 docs 目录下
3. 你对于 docs\savedprompt 只有只读权限，禁止进行任何形式的修改、新增

---

## 开发工作流

### 开发前上下文检查

在开始任何代码实现、重构、测试编写、构建配置调整或架构决策前，必须先阅读 `docs/开发日志.md` 和 `docs/开发计划.md`，并将开发日志中的最新进展、验证结果、遗留问题，以及开发计划中的已完成事项、短期计划和阶段目标作为决策依据。若开发日志或开发计划内容与当前代码状态不一致，必须以代码和实际验证结果为准，并在完成工作后同步修正开发日志和开发计划。

### 分支管理

- **main**：稳定版本，仅接受经过测试的合并
- **dev**：开发主分支，日常开发在此进行
- **feat/xxx**：功能开发分支
- **fix/xxx**：Bug 修复分支

### 提交规范

遵循 Conventional Commits 格式：

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Type 类型**：
- `feat`: 新功能
- `fix`: Bug 修复
- `perf`: 性能优化
- `refactor`: 代码重构
- `docs`: 文档更新
- `style`: 代码格式调整（不影响功能）
- `test`: 测试用例
- `build`: 构建系统变更

**Scope 范围**：
- `audio`: 音频引擎
- `graphics`: 渲染引擎
- `input`: 输入处理
- `chart`: 谱面逻辑
- `editor`: 编辑器
- `core`: 核心框架

**示例**：
```
feat(audio): implement low-latency WASAPI backend

- Add real-time audio callback with 128-frame buffer
- Integrate DSP time tracking for precise synchronization
- Optimize memory allocation in audio thread

Closes #42
```

### 代码审查要点

在提交代码前，确保：

1. **编译通过**：在 Windows 和 Linux 上均无编译错误或警告
2. **跨平台兼容**：避免平台特定代码（或使用条件编译隔离）
