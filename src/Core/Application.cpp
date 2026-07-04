// RaythmDemo - Core Application Lifecycle Implementation
// Implements application startup, event routing, frame timing, rendering, and shutdown.
// Author: RatherHard
// Date: 2026-07-04

#include "Core/Application.hpp"

#include "Platform/EventPump.hpp"
#include "Platform/InputState.hpp"
#include "Platform/Window.hpp"
#include "Render/Renderer.hpp"

#include <exception>
#include <iostream>
#include <memory>

#include <SDL3/SDL.h>

namespace Raythm::Core
{
    namespace
    {
        /** @brief Maximum platform events processed in one frame before rendering is allowed to proceed. */
        constexpr int MAX_EVENTS_PER_FRAME = 1024;

        /** @brief Delay used when rendering is paused by a zero-sized drawable surface. */
        constexpr Uint32 PAUSED_RENDER_DELAY_MILLISECONDS = 10;

        /** @brief RAII owner for the SDL video subsystem initialized by Core. */
        class SdlVideoSession
        {
        public:
            /**
             * @brief Initializes SDL video for the application lifetime.
             * @note Throws are avoided here so Application can print SDL_GetError consistently.
             */
            SdlVideoSession() noexcept
                : m_initialized(SDL_Init(SDL_INIT_VIDEO))
            {
            }

            /** @brief Quits SDL video if this session initialized it. */
            ~SdlVideoSession()
            {
                if (m_initialized)
                {
                    SDL_QuitSubSystem(SDL_INIT_VIDEO);
                }
            }

            /** @brief Copying is disabled because this object owns subsystem shutdown. */
            SdlVideoSession(const SdlVideoSession&) = delete;

            /** @brief Copy assignment is disabled because this object owns subsystem shutdown. */
            SdlVideoSession& operator=(const SdlVideoSession&) = delete;

            /** @brief Reports whether SDL video initialization succeeded. */
            bool isInitialized() const noexcept
            {
                return m_initialized;
            }

        private:
            /** @brief True when SDL_Init accepted the video initialization request. */
            bool m_initialized = false;
        };

        /**
         * @brief Converts Core application options into Platform window options.
         * @param options Core application startup configuration.
         * @return Platform window creation options.
         */
        Platform::WindowOptions makeWindowOptions(const ApplicationOptions& options)
        {
            Platform::WindowOptions windowOptions{};
            windowOptions.title = options.windowTitle;
            windowOptions.width = options.windowWidth;
            windowOptions.height = options.windowHeight;
            windowOptions.startHidden = options.startHidden;
            windowOptions.resizable = options.resizable;
            windowOptions.borderlessFullscreen = options.borderlessFullscreen;
            windowOptions.vulkanSurface = options.vulkanSurface;
            return windowOptions;
        }
    }

    struct Application::Impl
    {
        /** @brief Platform window owned by the running application. */
        std::unique_ptr<Platform::Window> window;

        /** @brief Renderer owned by the running application and destroyed before the window. */
        std::unique_ptr<Render::Renderer> renderer;

        /** @brief SDL global event pump used by the running application. */
        Platform::EventPump eventPump{};

        /** @brief Frame-local input snapshot updated by translated input events. */
        Platform::InputState inputState{};
    };

    Application::Application() = default;

    Application::Application(const ApplicationOptions& options)
        : m_options(options)
    {
    }

    Application::~Application() = default;

    int Application::run()
    {
        if (m_state == ApplicationState::Running)
        {
            return APPLICATION_FAILURE_EXIT_CODE;
        }

        if (m_quitRequested)
        {
            m_state = ApplicationState::Stopped;
            return APPLICATION_SUCCESS_EXIT_CODE;
        }

        try
        {
            SdlVideoSession sdlVideoSession{};
            if (!sdlVideoSession.isInitialized())
            {
                std::cerr << "SDL video subsystem is unavailable: " << SDL_GetError() << std::endl;
                m_state = ApplicationState::Failed;
                return APPLICATION_FAILURE_EXIT_CODE;
            }

            Impl impl{};
            impl.window = std::make_unique<Platform::Window>(makeWindowOptions(m_options));
            impl.renderer = std::make_unique<Render::Renderer>(*impl.window);
            impl.window->show();

            m_timeSystem.reset();
            m_quitRequested = false;
            m_state = ApplicationState::Initialized;
            m_state = ApplicationState::Running;

            while (!m_quitRequested && !impl.window->shouldClose())
            {
                m_timeSystem.beginFrame();
                impl.inputState.beginFrame();

                Platform::PlatformEvent event{};
                for (int eventsProcessed = 0;
                     eventsProcessed < MAX_EVENTS_PER_FRAME &&
                     impl.eventPump.pollEvent(event, impl.window->getWindowId());
                     ++eventsProcessed)
                {
                    if (event.type == Platform::PlatformEventType::Window)
                    {
                        impl.window->applyEvent(event.window);
                        impl.renderer->handleWindowEvent(event.window);
                        if (event.window.type == Platform::WindowEventType::FocusLost)
                        {
                            impl.inputState.clear();
                        }
                    }
                    else if (event.type == Platform::PlatformEventType::Input)
                    {
                        impl.inputState.handleInputEvent(event.input);
                    }
                }

                const bool wasFrameSubmitted = impl.renderer->renderFrame();
                if (!wasFrameSubmitted)
                {
                    SDL_Delay(PAUSED_RENDER_DELAY_MILLISECONDS);
                }
            }

            m_state = ApplicationState::Stopping;
            impl.renderer->waitIdle();
            impl.renderer.reset();
            impl.window.reset();
            m_state = ApplicationState::Stopped;
            return APPLICATION_SUCCESS_EXIT_CODE;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "RaythmDemo failed: " << exception.what() << std::endl;
            m_state = ApplicationState::Failed;
            return APPLICATION_FAILURE_EXIT_CODE;
        }
    }

    void Application::requestQuit() noexcept
    {
        m_quitRequested = true;
        if (m_state == ApplicationState::Created || m_state == ApplicationState::Initialized)
        {
            m_state = ApplicationState::Stopping;
        }
    }

    bool Application::isQuitRequested() const noexcept
    {
        return m_quitRequested;
    }

    ApplicationState Application::getState() const noexcept
    {
        return m_state;
    }

    const FrameTime& Application::getCurrentFrameTime() const noexcept
    {
        return m_timeSystem.getCurrentFrameTime();
    }
}
