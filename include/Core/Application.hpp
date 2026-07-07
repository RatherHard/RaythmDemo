// RaythmDemo - Core Application Lifecycle
// Defines the application root that orchestrates platform, input, timing, and rendering lifecycles.
// Author: RatherHard
// Date: 2026-07-04

#pragma once

#include <filesystem>
#include <string>

#include "Core/Time.hpp"

namespace Raythm::Core
{
    /** @brief Process exit code returned when the application shuts down cleanly. */
    constexpr int APPLICATION_SUCCESS_EXIT_CODE = 0;

    /** @brief Process exit code returned when startup, runtime, or shutdown fails. */
    constexpr int APPLICATION_FAILURE_EXIT_CODE = 1;

    /**
     * @brief Lifecycle state for the Core application root.
     */
    enum class ApplicationState
    {
        /** @brief Application object exists but SDL/window/renderer resources are not running. */
        Created,

        /** @brief Startup completed and the run loop is about to begin. */
        Initialized,

        /** @brief Main loop is actively polling, updating, and rendering frames. */
        Running,

        /** @brief Quit was requested and shutdown is in progress or pending. */
        Stopping,

        /** @brief Main loop has finished and platform resources have been released. */
        Stopped,

        /** @brief Startup or runtime failed and the process should return a failure code. */
        Failed
    };

    /**
     * @brief Configuration used to bootstrap the application root.
     */
    struct ApplicationOptions
    {
        /** @brief UTF-8 title passed to the platform window. */
        std::string windowTitle = "RaythmDemo";

        /** @brief Initial logical window width in screen coordinates. */
        int windowWidth = 1280;

        /** @brief Initial logical window height in screen coordinates. */
        int windowHeight = 720;

        /** @brief Creates the window hidden until renderer initialization succeeds. */
        bool startHidden = true;

        /** @brief Enables user-driven native window resizing. */
        bool resizable = true;

        /** @brief Requests SDL fullscreen-desktop mode immediately after creation. */
        bool borderlessFullscreen = false;

        /** @brief Enables SDL Vulkan surface support for the renderer. */
        bool vulkanSurface = true;

        /** @brief Root directory for runtime chart and audio assets. */
        std::filesystem::path assetRoot = "assets";

        /** @brief Startup chart path resolved under assetRoot. */
        std::filesystem::path startupChartPath = "charts/00001/00001.json";
    };

    /**
     * @brief Application root that owns startup, frame loop, event routing, and shutdown order.
     */
    class Application
    {
    public:
        /**
         * @brief Creates an application with default options and a steady monotonic time system.
         */
        Application();

        /**
         * @brief Creates an application from explicit startup options.
         * @param options Application startup configuration.
         */
        explicit Application(const ApplicationOptions& options);

        /** @brief Releases application resources owned by the implementation. */
        ~Application();

        /** @brief Copying is disabled because Application owns process-level SDL lifecycle state. */
        Application(const Application&) = delete;

        /** @brief Copy assignment is disabled because Application owns process-level SDL lifecycle state. */
        Application& operator=(const Application&) = delete;

        /** @brief Moving is disabled because SDL lifecycle ownership should remain single and stable. */
        Application(Application&&) = delete;

        /** @brief Move assignment is disabled because SDL lifecycle ownership should remain single and stable. */
        Application& operator=(Application&&) = delete;

        /**
         * @brief Runs the application until a platform close or programmatic quit request occurs.
         * @return APPLICATION_SUCCESS_EXIT_CODE on clean shutdown, otherwise APPLICATION_FAILURE_EXIT_CODE.
         * @note Startup failures are caught and converted to a process-friendly result code.
         */
        int run();

        /**
         * @brief Requests that the current or next run loop exits cleanly.
         * @note Future UI or gameplay code can use this without forging window events.
         */
        void requestQuit() noexcept;

        /** @brief Reports whether a programmatic quit has been requested. */
        bool isQuitRequested() const noexcept;

        /** @brief Returns the current lifecycle state. */
        ApplicationState getState() const noexcept;

        /** @brief Returns the latest Core frame timing snapshot. */
        const FrameTime& getCurrentFrameTime() const noexcept;

    private:
        /** @brief Hidden implementation that keeps SDL and Vulkan includes out of this public API. */
        struct Impl;

        /** @brief Application startup configuration copied at construction. */
        ApplicationOptions m_options{};

        /** @brief Monotonic timing system owned by Core. */
        TimeSystem m_timeSystem{};

        /** @brief Current lifecycle state. */
        ApplicationState m_state = ApplicationState::Created;

        /** @brief True after a caller or platform path requests loop exit. */
        bool m_quitRequested = false;
    };
}
