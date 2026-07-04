// RaythmDemo - SDL Window Wrapper
// Defines the engine-facing RAII abstraction over SDL3 windows and window state.
// Author: RatherHard
// Date: 2026-07-04

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <volk.h>

namespace Raythm::Platform
{
    /**
     * @brief Configuration used to create a platform window.
     */
    struct WindowOptions
    {
        /** @brief UTF-8 title passed to SDL when creating the native window. */
        std::string title = "RaythmDemo";

        /** @brief Initial logical window width in screen coordinates. */
        int width = 1280;

        /** @brief Initial logical window height in screen coordinates. */
        int height = 720;

        /** @brief Creates the window hidden so tests or bootstrapping code can configure it before display. */
        bool startHidden = false;

        /** @brief Enables user-driven window resizing through the native window manager. */
        bool resizable = true;

        /** @brief Requests SDL fullscreen-desktop mode immediately after creation. */
        bool borderlessFullscreen = false;

        /** @brief Adds SDL's Vulkan-capable window flag so a VkSurfaceKHR can be created later. */
        bool vulkanSurface = true;
    };

    /**
     * @brief Window-only events translated from SDL events.
     */
    enum class WindowEventType
    {
        /** @brief No translated event is available. */
        None,

        /** @brief The platform requested that the window close. */
        CloseRequested,

        /** @brief The platform requested that the application quit. */
        QuitRequested,

        /** @brief The native window became visible. */
        Shown,

        /** @brief The native window became hidden. */
        Hidden,

        /** @brief The window contents should be redrawn after exposure. */
        Exposed,

        /** @brief The window moved to a new screen position. */
        Moved,

        /** @brief The logical window size changed. */
        Resized,

        /** @brief The drawable pixel size changed, which can differ from logical size on HiDPI displays. */
        PixelSizeChanged,

        /** @brief The window was minimized by the platform. */
        Minimized,

        /** @brief The window was maximized by the platform. */
        Maximized,

        /** @brief The window returned from minimized or maximized state. */
        Restored,

        /** @brief The pointing device entered the window bounds. */
        MouseEntered,

        /** @brief The pointing device left the window bounds. */
        MouseLeft,

        /** @brief The window gained keyboard input focus. */
        FocusGained,

        /** @brief The window lost keyboard input focus. */
        FocusLost,

        /** @brief The window moved to or changed association with another display. */
        DisplayChanged
    };

    /**
     * @brief Engine-facing window event payload.
     */
    struct WindowEvent
    {
        /** @brief Translated event kind, or None when the payload is empty. */
        WindowEventType type = WindowEventType::None;

        /** @brief Logical or drawable width for resize-related events. */
        int width = 0;

        /** @brief Logical or drawable height for resize-related events. */
        int height = 0;

        /** @brief Screen-space x coordinate for move events. */
        int x = 0;

        /** @brief Screen-space y coordinate for move events. */
        int y = 0;

        /** @brief SDL display identifier for display-change events. */
        std::uint32_t displayId = 0;
    };

    /**
     * @brief RAII owner for an SDL window and its engine-facing state.
     */
    class Window
    {
    public:
        /**
         * @brief Creates an SDL window from the supplied options.
         * @param options Window creation options.
         * @note SDL video subsystem must be initialized before construction.
         */
        explicit Window(const WindowOptions& options);

        /**
         * @brief Destroys the owned SDL window.
         */
        ~Window();

        /** @brief Copying is disabled because this object uniquely owns the native SDL window. */
        Window(const Window&) = delete;

        /** @brief Copy assignment is disabled because this object uniquely owns the native SDL window. */
        Window& operator=(const Window&) = delete;

        /**
         * @brief Transfers SDL window ownership from another Window.
         * @param other Source window that becomes empty after the move.
         * @note Move construction is noexcept so Window can be stored in standard containers safely.
         */
        Window(Window&& other) noexcept;

        /**
         * @brief Replaces this window by taking ownership from another Window.
         * @param other Source window that becomes empty after the move.
         * @return Reference to this window after ownership transfer.
         * @note Any currently owned SDL window is destroyed before the transfer.
         */
        Window& operator=(Window&& other) noexcept;

        /**
         * @brief Applies a translated window event to this window's sticky state.
         * @param event Engine-facing window event produced by the platform event pump.
         * @note Close and quit events set shouldClose; other events are currently observed by consumers directly.
         */
        void applyEvent(const WindowEvent& event) noexcept;

        /**
         * @brief Creates a Vulkan surface for this window.
         * @param instance A Vulkan instance created with SDL's required instance extensions.
         * @return Created Vulkan surface handle.
         * @note The caller owns the returned VkSurfaceKHR and must destroy it with the same Vulkan instance.
         */
        VkSurfaceKHR createVulkanSurface(VkInstance instance) const;

        /**
         * @brief Returns Vulkan instance extensions required by SDL windows.
         * @return Extension names owned by SDL.
         * @note The returned pointers are SDL-owned and must not be freed by the caller.
         */
        std::vector<const char*> getRequiredVulkanInstanceExtensions() const;

        /**
         * @brief Enables or disables borderless fullscreen mode.
         * @param enabled True to enter fullscreen-desktop mode, false to return to windowed mode.
         */
        void setBorderlessFullscreen(bool enabled);

        /** @brief Shows the native window through SDL. */
        void show();

        /** @brief Hides the native window through SDL. */
        void hide();

        /** @brief Reports whether a translated close request has been received. */
        bool shouldClose() const noexcept;

        /** @brief Reports whether SDL currently marks the window as hidden. */
        bool isHidden() const noexcept;

        /** @brief Reports whether SDL currently marks the window as minimized. */
        bool isMinimized() const noexcept;

        /** @brief Reports whether SDL currently marks the window as maximized. */
        bool isMaximized() const noexcept;

        /** @brief Reports whether the window currently owns keyboard input focus. */
        bool hasInputFocus() const noexcept;

        /** @brief Reports whether SDL currently marks the window as fullscreen. */
        bool isBorderlessFullscreen() const noexcept;

        /**
         * @brief Returns the current logical window size.
         * @return Width and height in screen coordinates, or zeroes if the window is empty.
         */
        std::pair<int, int> getSize() const noexcept;

        /**
         * @brief Returns the current drawable pixel size.
         * @return Width and height in physical pixels, or zeroes if the window is empty.
         */
        std::pair<int, int> getDrawableSize() const noexcept;

        /**
         * @brief Returns the SDL window identifier used to route platform events.
         * @return Native SDL window ID, or zero after this object has been moved from.
         */
        SDL_WindowID getWindowId() const noexcept;

        /**
         * @brief Returns the owned SDL window handle without transferring ownership.
         * @return Native SDL window pointer, or nullptr after this object has been moved from.
         */
        SDL_Window* getNativeHandle() const noexcept;

    private:
        /**
         * @brief Converts engine window options into SDL creation flags.
         * @param options Engine-facing window configuration.
         * @return SDL bitmask used by SDL_CreateWindow.
         */
        static SDL_WindowFlags buildWindowFlags(const WindowOptions& options) noexcept;

        /** @brief Owned SDL window handle; nullptr only after moves or failed construction cleanup. */
        SDL_Window* m_window = nullptr;

        /** @brief SDL identifier used to filter global window events to this window instance. */
        SDL_WindowID m_windowId = 0;

        /** @brief Sticky close flag set after a close-request event is translated. */
        bool m_shouldClose = false;
    };
}
