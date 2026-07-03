// RaythmDemo - SDL Window Wrapper Implementation
// Implements SDL3 window lifetime, Vulkan surface creation, and event translation.
// Author: RatherHard
// Date: 2026-07-04

#include "Platform/Window.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

#include <SDL3/SDL_vulkan.h>

namespace Raythm
{
    namespace
    {
        /**
         * @brief Builds an exception message from caller context and the current SDL error string.
         * @param message High-level failure context supplied by the caller.
         * @return Runtime error containing SDL detail when SDL exposes one.
         */
        std::runtime_error makeSdlError(const std::string& message)
        {
            const char* error = SDL_GetError();
            if (error == nullptr || error[0] == '\0')
            {
                return std::runtime_error(message);
            }

            return std::runtime_error(message + ": " + error);
        }

        /**
         * @brief Checks whether an SDL window flag bit is present.
         * @param flags Bitmask returned by SDL_GetWindowFlags.
         * @param flag Single flag to test within the bitmask.
         * @return True when the flag is fully present.
         */
        bool hasFlag(SDL_WindowFlags flags, SDL_WindowFlags flag) noexcept
        {
            return (flags & flag) == flag;
        }
    }

    Window::Window(const WindowOptions& options)
    {
        if (options.width <= 0 || options.height <= 0)
        {
            throw std::invalid_argument("Window dimensions must be greater than zero.");
        }

        m_window = SDL_CreateWindow(
            options.title.c_str(),
            options.width,
            options.height,
            buildWindowFlags(options)
        );

        if (m_window == nullptr)
        {
            throw makeSdlError("Failed to create SDL window");
        }

        m_windowId = SDL_GetWindowID(m_window);
        if (m_windowId == 0)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            throw makeSdlError("Failed to get SDL window id");
        }

        if (options.borderlessFullscreen)
        {
            setBorderlessFullscreen(true);
        }
    }

    Window::~Window()
    {
        if (m_window != nullptr)
        {
            SDL_DestroyWindow(m_window);
        }
    }

    Window::Window(Window&& other) noexcept
        : m_window(std::exchange(other.m_window, nullptr)),
          m_windowId(std::exchange(other.m_windowId, 0)),
          m_shouldClose(std::exchange(other.m_shouldClose, false))
    {
    }

    Window& Window::operator=(Window&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (m_window != nullptr)
        {
            SDL_DestroyWindow(m_window);
        }

        m_window = std::exchange(other.m_window, nullptr);
        m_windowId = std::exchange(other.m_windowId, 0);
        m_shouldClose = std::exchange(other.m_shouldClose, false);

        return *this;
    }

    bool Window::pollEvent(WindowEvent& event)
    {
        SDL_Event sdlEvent{};
        std::vector<SDL_Event> deferredEvents;

        while (SDL_PeepEvents(&sdlEvent, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST) == 1)
        {
            WindowEvent translatedEvent{};
            if (!translateEvent(sdlEvent, translatedEvent, m_windowId))
            {
                deferredEvents.push_back(sdlEvent);
                continue;
            }

            // SDL owns one global event queue. Events for other windows are replayed before returning so
            // this wrapper consumes only events belonging to its native window and preserves other systems' work.
            for (SDL_Event& deferredEvent : deferredEvents)
            {
                SDL_PushEvent(&deferredEvent);
            }

            if (translatedEvent.type == WindowEventType::CloseRequested)
            {
                m_shouldClose = true;
            }

            event = translatedEvent;
            return true;
        }

        for (SDL_Event& deferredEvent : deferredEvents)
        {
            SDL_PushEvent(&deferredEvent);
        }

        return false;
    }

    VkSurfaceKHR Window::createVulkanSurface(VkInstance instance) const
    {
        if (m_window == nullptr)
        {
            throw std::runtime_error("Cannot create Vulkan surface for an empty window.");
        }

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(m_window, instance, nullptr, &surface))
        {
            throw makeSdlError("Failed to create Vulkan surface");
        }

        return surface;
    }

    std::vector<const char*> Window::getRequiredVulkanInstanceExtensions() const
    {
        Uint32 extensionCount = 0;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (extensions == nullptr)
        {
            throw makeSdlError("Failed to get Vulkan instance extensions");
        }

        return std::vector<const char*>(extensions, extensions + extensionCount);
    }

    void Window::setBorderlessFullscreen(bool enabled)
    {
        if (m_window == nullptr)
        {
            throw std::runtime_error("Cannot change fullscreen state for an empty window.");
        }

        if (!SDL_SetWindowFullscreen(m_window, enabled))
        {
            throw makeSdlError("Failed to change window fullscreen state");
        }
    }

    void Window::show()
    {
        if (m_window == nullptr)
        {
            throw std::runtime_error("Cannot show an empty window.");
        }

        if (!SDL_ShowWindow(m_window))
        {
            throw makeSdlError("Failed to show SDL window");
        }
    }

    void Window::hide()
    {
        if (m_window == nullptr)
        {
            throw std::runtime_error("Cannot hide an empty window.");
        }

        if (!SDL_HideWindow(m_window))
        {
            throw makeSdlError("Failed to hide SDL window");
        }
    }

    bool Window::shouldClose() const noexcept
    {
        return m_shouldClose;
    }

    bool Window::isHidden() const noexcept
    {
        return m_window != nullptr && hasFlag(SDL_GetWindowFlags(m_window), SDL_WINDOW_HIDDEN);
    }

    bool Window::isMinimized() const noexcept
    {
        return m_window != nullptr && hasFlag(SDL_GetWindowFlags(m_window), SDL_WINDOW_MINIMIZED);
    }

    bool Window::isMaximized() const noexcept
    {
        return m_window != nullptr && hasFlag(SDL_GetWindowFlags(m_window), SDL_WINDOW_MAXIMIZED);
    }

    bool Window::hasInputFocus() const noexcept
    {
        return m_window != nullptr && hasFlag(SDL_GetWindowFlags(m_window), SDL_WINDOW_INPUT_FOCUS);
    }

    bool Window::isBorderlessFullscreen() const noexcept
    {
        return m_window != nullptr && hasFlag(SDL_GetWindowFlags(m_window), SDL_WINDOW_FULLSCREEN);
    }

    std::pair<int, int> Window::getSize() const noexcept
    {
        int width = 0;
        int height = 0;
        if (m_window != nullptr)
        {
            SDL_GetWindowSize(m_window, &width, &height);
        }

        return {width, height};
    }

    std::pair<int, int> Window::getDrawableSize() const noexcept
    {
        int width = 0;
        int height = 0;
        if (m_window != nullptr)
        {
            SDL_GetWindowSizeInPixels(m_window, &width, &height);
        }

        return {width, height};
    }

    SDL_Window* Window::getNativeHandle() const noexcept
    {
        return m_window;
    }

    SDL_WindowFlags Window::buildWindowFlags(const WindowOptions& options) noexcept
    {
        SDL_WindowFlags flags = 0;
        if (options.startHidden)
        {
            flags |= SDL_WINDOW_HIDDEN;
        }

        if (options.resizable)
        {
            flags |= SDL_WINDOW_RESIZABLE;
        }

        if (options.borderlessFullscreen)
        {
            flags |= SDL_WINDOW_FULLSCREEN;
        }

        if (options.vulkanSurface)
        {
            flags |= SDL_WINDOW_VULKAN;
        }

        return flags;
    }

    bool Window::translateEvent(const SDL_Event& sdlEvent, WindowEvent& event, SDL_WindowID windowId) noexcept
    {
        if (sdlEvent.type < SDL_EVENT_WINDOW_FIRST || sdlEvent.type > SDL_EVENT_WINDOW_LAST)
        {
            return false;
        }

        if (sdlEvent.window.windowID != windowId)
        {
            return false;
        }

        event = {};
        switch (sdlEvent.type)
        {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            event.type = WindowEventType::CloseRequested;
            break;
        case SDL_EVENT_WINDOW_SHOWN:
            event.type = WindowEventType::Shown;
            break;
        case SDL_EVENT_WINDOW_HIDDEN:
            event.type = WindowEventType::Hidden;
            break;
        case SDL_EVENT_WINDOW_EXPOSED:
            event.type = WindowEventType::Exposed;
            break;
        case SDL_EVENT_WINDOW_MOVED:
            event.type = WindowEventType::Moved;
            event.x = sdlEvent.window.data1;
            event.y = sdlEvent.window.data2;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            event.type = WindowEventType::Resized;
            event.width = sdlEvent.window.data1;
            event.height = sdlEvent.window.data2;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            event.type = WindowEventType::PixelSizeChanged;
            event.width = sdlEvent.window.data1;
            event.height = sdlEvent.window.data2;
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            event.type = WindowEventType::Minimized;
            break;
        case SDL_EVENT_WINDOW_MAXIMIZED:
            event.type = WindowEventType::Maximized;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            event.type = WindowEventType::Restored;
            break;
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            event.type = WindowEventType::MouseEntered;
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            event.type = WindowEventType::MouseLeft;
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            event.type = WindowEventType::FocusGained;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            event.type = WindowEventType::FocusLost;
            break;
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            event.type = WindowEventType::DisplayChanged;
            event.displayId = static_cast<std::uint32_t>(sdlEvent.window.data1);
            break;
        default:
            return false;
        }

        return true;
    }
}
