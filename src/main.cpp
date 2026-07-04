// RaythmDemo - Main Entry Point
// Starts a minimal SDL/Vulkan loop that clears and presents the platform window.
// Author: RatherHard
// Date: 2026-07-04

#include "Platform/EventPump.hpp"
#include "Platform/InputState.hpp"
#include "Platform/Window.hpp"
#include "Render/Renderer.hpp"

#include <exception>
#include <iostream>

#include <SDL3/SDL.h>

namespace
{
    /** @brief Exit code returned when startup or rendering fails. */
    constexpr int FAILURE_EXIT_CODE = 1;

    /**
     * @brief Builds default window options for the current render skeleton.
     * @return Vulkan-capable window configuration for the main executable.
     * @note The renderer currently clears this window continuously until the platform requests close.
     */
    Raythm::Platform::WindowOptions makeWindowOptions()
    {
        Raythm::Platform::WindowOptions options{};
        options.title = "RaythmDemo";
        options.width = 1280;
        options.height = 720;
        options.startHidden = true;
        options.resizable = true;
        options.vulkanSurface = true;
        return options;
    }
}

/**
 * @brief Starts the RaythmDemo application executable.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Process exit code; zero indicates clean shutdown.
 * @note This is the first render milestone: clear a Vulkan swapchain and present it through SDL.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "SDL video subsystem is unavailable: " << SDL_GetError() << std::endl;
        return FAILURE_EXIT_CODE;
    }

    try
    {
        Raythm::Platform::Window window(makeWindowOptions());
        Raythm::Render::Renderer renderer(window);
        Raythm::Platform::EventPump eventPump{};
        Raythm::Platform::InputState inputState{};
        window.show();

        while (!window.shouldClose())
        {
            inputState.beginFrame();

            Raythm::Platform::PlatformEvent event{};
            while (eventPump.pollEvent(event, window.getWindowId()))
            {
                if (event.type == Raythm::Platform::PlatformEventType::Window)
                {
                    window.applyEvent(event.window);
                    renderer.handleWindowEvent(event.window);
                    if (event.window.type == Raythm::Platform::WindowEventType::FocusLost)
                    {
                        inputState.clear();
                    }
                }
                else if (event.type == Raythm::Platform::PlatformEventType::Input)
                {
                    inputState.handleInputEvent(event.input);
                }
            }

            renderer.renderFrame();
        }

        renderer.waitIdle();
        SDL_Quit();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "RaythmDemo failed: " << exception.what() << std::endl;
        SDL_Quit();
        return FAILURE_EXIT_CODE;
    }
}
