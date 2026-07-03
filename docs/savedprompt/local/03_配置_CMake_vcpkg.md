在根目录下创建 CMakeLists.txt 和 vcpkg.json 并编写内容，要求：

1. vcpkg 使用 Manifest 模式管理
2. 使用 vcpkg 管理 SDL3 和 nlohmann-json
3. 在 CMakeLists.txt 中引入系统中 Vulkan SDK 的路径（在 CMake 中寻找 Vulkan SDK，不要手动去写硬编码的系统路径）

```
已成功创建两个文件：

  创建的文件

  1. vcpkg.json

  - 使用 Manifest 模式管理依赖
  - 包含 SDL3 和 nlohmann-json 两个依赖包
  - 使用 builtin-baseline: "master" 跟踪最新稳定版本

  2. CMakeLists.txt

  - 设置 CMake 最低版本为 3.20（符合 CLAUDE.md 要求）
  - 配置 C++20 标准
  - 通过 find_package(Vulkan REQUIRED) 自动查找系统中的 Vulkan SDK（无硬编码路径）
  - 通过 find_package() 引入 vcpkg 管理的 SDL3 和 nlohmann-json
  - 添加配置摘要输出，显示 Vulkan 路径和 C++ 标准

  下一步操作

  构建项目前需要：

  1. 安装 vcpkg（如果尚未安装）
  2. 配置 CMake toolchain：
  cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg路径]/scripts/buildsystems/vcpkg.cmake
  3. vcpkg 会自动下载并构建 SDL3 和 nlohmann-json

  项目结构现已准备好进行下一步开发（如创建 src/ 目录和源文件）。
```



根据技术栈规范为当前vscode开发环境进行配置

```
已成功为 VSCode 开发环境配置完成！

  创建的配置文件

  1. settings.json - 工作区设置

  - C++20 标准：符合技术栈规范
  - Allman 风格：左花括号单独一行
  - 4 空格缩进：禁用 Tab
  - 120 字符行宽限制：在编辑器中显示标尺
  - UTF-8 编码 + LF 换行符：跨平台兼容
  - CMake 集成：自动配置、使用 Ninja 生成器

  2. c_cpp_properties.json - IntelliSense 配置

  - 双平台配置：Windows (MSVC) 和 Linux (GCC)
  - Vulkan SDK 路径：
    - Windows: ${env:VULKAN_SDK}/Include
    - Linux: /usr/include/vulkan
  - vcpkg 依赖路径：自动引入 SDL3 和 nlohmann-json 头文件
  - C++20 标准：智能提示基于 C++20

  3. tasks.json - 构建任务

  - CMake: Configure：配置项目（带 vcpkg toolchain）
  - CMake: Build（默认构建任务）：编译项目
  - CMake: Clean：清理构建文件
  - 使用 Ninja 生成器加速构建

  4. launch.json - 调试配置

  - Windows 调试：使用 Visual Studio 调试器 (cppvsdbg)
  - Linux 调试：使用 GDB
  - 两者均在构建前自动执行 CMake: Build 任务

  5. extensions.json - 推荐扩展

  - ms-vscode.cpptools：C/C++ IntelliSense
  - ms-vscode.cmake-tools：CMake 集成
  - twxs.cmake：CMakeLists.txt 语法高亮
  - ms-vscode.cpptools-extension-pack：C++ 扩展包

  使用前准备

  1. 安装推荐扩展：VSCode 会提示安装，或按 Ctrl+Shift+P 输入 "Extensions: Show Recommended Extensions"
  2. 设置环境变量：
    - VULKAN_SDK：指向 Vulkan SDK 安装路径
    - VCPKG_ROOT：指向 vcpkg 安装路径（如 C:\vcpkg）
  3. 重新加载窗口：Ctrl+Shift+P → "Reload Window"

  快捷操作

  - 构建项目：Ctrl+Shift+B（运行默认构建任务）
  - 运行调试：F5
  - 运行任务：Ctrl+Shift+P → "Tasks: Run Task"

  配置已完全符合 CLAUDE.md 中的技术栈规范（C++20、CMake、Vulkan、SDL3、跨平台）。
```
