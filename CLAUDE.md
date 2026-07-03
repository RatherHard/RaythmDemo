# RaythmDemo - System Prompts

## 项目概述

本项目是一个基于 C++ 的跨平台音乐节奏游戏（音游），目标平台为 **Windows** 和 **Linux**。采用自研架构，以实现极致低延迟、精确的音画同步，以及高性能的谱面渲染能力。

---

## 技术栈规范

**重要提示**：本项目的技术选型已在 `docs/技术设计文档/核心技术选型.md` 中明确定义。开发过程中 **严禁擅自更改技术方案**。

### 核心技术栈

| 模块 | 技术选型 | 版本/说明 |
|------|---------|----------|
| **开发语言** | C++ | C++20 标准 |
| **构建系统** | CMake | 3.20+ |
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
├── .vscode/               # VSCode 工作区配置 (launch.json, tasks.json)
├── assets/                # 运行时资源文件
├── docs/                  # 项目文档
│   ├── savedprompt/       # [系统保留] AI 提示词存档 (仅只读)
│   └── 技术设计文档/       # 架构设计、选型说明等
├── include/               # 核心头文件 (.hpp / .h) - 必须按模块划分子目录
│   ├── Core/              # 核心框架 (窗口管理器、主循环、时间系统)
│   ├── Audio/             # 音频引擎封装
│   ├── Render/            # Vulkan 渲染后端
│   └── Game/              # 游戏逻辑、谱面解析、实体
├── src/                   # 源代码文件 (.cpp) - 目录结构必须与 include 镜像一致
│   ├── Core/
│   ├── Audio/
│   ├── Render/
│   ├── Game/
│   └── main.cpp           # 程序主入口
├── third_party/           # 第三方源码库
│   ├── miniaudio/         # miniaudio.h
├── CMakeLists.txt         # 顶层 CMake 构建脚本
└── vcpkg.json             # vcpkg 依赖清单
```

---

## 开发规范

### C++ 编码规范

#### 命名约定

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

- **文件头注释**：每个源文件顶部包含文件说明、作者、日期
- **类注释**：Doxygen 格式的 `@brief` 说明类的职责
- **函数注释**：包含 `@param`、`@return`、`@note`（性能注意事项）
- **复杂逻辑**：算法实现前添加伪代码或步骤说明

### 内存安全规范

- 彻底遵循 RAII
- 优先使用智能指针，禁止裸 new/delete
- 用容器代替手动动态数组

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

### 权限说明

1. 禁止未经用户允许新增文档、修改文档
2. 文档只能放在 docs 目录下
3. 你对于 docs\savedprompt 只有只读权限，禁止进行任何形式的修改、新增

---

## 开发工作流

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
