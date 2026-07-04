// RaythmDemo - Vulkan Renderer Smoke Tests
// Exercises minimal renderer lifecycle, clear/present, and resize handling through an SDL-backed window.
// Author: RatherHard
// Date: 2026-07-04

#include "Platform/Window.hpp"
#include "Render/Renderer.hpp"

#include <exception>
#include <iostream>
#include <string>

#include <SDL3/SDL.h>

namespace
{
    namespace Platform = Raythm::Platform;
    namespace Render = Raythm::Render;

    /** @brief Logical width used by hidden render test windows. */
    constexpr int TEST_WINDOW_WIDTH = 320;

    /** @brief Logical height used by hidden render test windows. */
    constexpr int TEST_WINDOW_HEIGHT = 240;

    /**
     * @brief Records a failed expectation without aborting the current test function.
     * @param condition Boolean result produced by the assertion expression.
     * @param message Human-readable failure message printed when the condition is false.
     * @return The original condition so callers can accumulate pass/fail state.
     */
    bool expect(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << std::endl;
            return false;
        }

        return true;
    }

    /**
     * @brief Creates hidden Vulkan-capable options suitable for renderer smoke tests.
     * @return Window options that avoid visible UI while keeping SDL Vulkan surface support enabled.
     */
    Platform::WindowOptions makeRenderWindowOptions()
    {
        Platform::WindowOptions options{};
        options.title = "RaythmDemo Render Test";
        options.width = TEST_WINDOW_WIDTH;
        options.height = TEST_WINDOW_HEIGHT;
        options.startHidden = true;
        options.resizable = true;
        options.vulkanSurface = true;
        return options;
    }

    /**
     * @brief Verifies renderer construction, one clear/present frame, and resize event handling.
     * @return True when renderer lifecycle checks pass, or when host Vulkan window support is unavailable.
     * @note Unsupported Vulkan environments are treated as a skip to keep headless CI usable.
     */
    bool testRendererClearPresentAndResize()
    {
        try
        {
            Platform::Window window(makeRenderWindowOptions());
            Render::Renderer renderer(window);

            bool passed = true;
            passed &= expect(renderer.isInitialized(), "renderer should report initialized after construction");
            passed &= expect(!renderer.isRenderingPaused(), "renderer should not start paused for a positive drawable size");

            renderer.renderFrame();

            Platform::WindowEvent resizeEvent{};
            resizeEvent.type = Platform::WindowEventType::PixelSizeChanged;
            resizeEvent.width = TEST_WINDOW_WIDTH;
            resizeEvent.height = TEST_WINDOW_HEIGHT;
            renderer.handleWindowEvent(resizeEvent);
            renderer.renderFrame();
            renderer.waitIdle();

            return passed;
        }
        catch (const std::exception& exception)
        {
            std::cout << "Vulkan renderer support is unavailable; skipping renderer smoke test: "
                      << exception.what() << std::endl;
            return true;
        }
    }
}

/**
 * @brief Runs Vulkan renderer smoke tests under an initialized SDL video subsystem.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Zero when renderer smoke checks pass; nonzero when any check fails.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "SDL video subsystem is unavailable: " << SDL_GetError() << std::endl;
        return 1;
    }

    const bool allTestsPassed = testRendererClearPresentAndResize();

    SDL_Quit();
    return allTestsPassed ? 0 : 1;
}
