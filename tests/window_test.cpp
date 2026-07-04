// RaythmDemo - SDL Window Wrapper Tests
// Exercises the SDL-backed Window wrapper lifecycle, event filtering, and Vulkan extension query behavior.
// Author: RatherHard
// Date: 2026-07-04

#include "Platform/Window.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>

namespace
{
    namespace Platform = Raythm::Platform;

    /** @brief Logical width used by hidden windows in these tests. */
    constexpr int TEST_WINDOW_WIDTH = 320;

    /** @brief Logical height used by hidden windows in these tests. */
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
     * @brief Polls translated window events until the expected event type appears.
     * @param window Window wrapper whose SDL event stream is being checked.
     * @param event Receives the matching event payload when one is found.
     * @param expectedType Event type that should appear in the wrapper event stream.
     * @return True when the expected event type is observed before the queue is exhausted.
     */
    bool pollUntil(Platform::Window& window, Platform::WindowEvent& event, Platform::WindowEventType expectedType)
    {
        while (window.pollEvent(event))
        {
            if (event.type == expectedType)
            {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Pushes a synthetic SDL window event into the global SDL event queue.
     * @param type SDL window event type to simulate.
     * @param windowId SDL window identifier that should own the simulated event.
     * @param data1 First SDL window payload integer.
     * @param data2 Second SDL window payload integer.
     * @return True when SDL accepts the event into its queue.
     */
    bool pushWindowEvent(SDL_EventType type, SDL_WindowID windowId, int data1 = 0, int data2 = 0)
    {
        SDL_Event event{};
        event.window.type = type;
        event.window.windowID = windowId;
        event.window.data1 = data1;
        event.window.data2 = data2;

        return SDL_PushEvent(&event);
    }

    /**
     * @brief Creates hidden, non-Vulkan options suitable for deterministic window wrapper tests.
     * @return Window options that avoid visible UI and Vulkan platform requirements by default.
     */
    Platform::WindowOptions makeHiddenOptions()
    {
        Platform::WindowOptions options{};
        options.title = "RaythmDemo Window Test";
        options.width = TEST_WINDOW_WIDTH;
        options.height = TEST_WINDOW_HEIGHT;
        options.startHidden = true;
        options.resizable = true;
        options.vulkanSurface = false;
        return options;
    }

    /**
     * @brief Verifies basic window creation, state queries, fullscreen toggling, and move ownership.
     * @return True when lifecycle and state expectations all hold.
     */
    bool testWindowLifecycleAndState()
    {
        Platform::Window window(makeHiddenOptions());

        auto [width, height] = window.getSize();
        auto [drawableWidth, drawableHeight] = window.getDrawableSize();

        bool passed = true;
        passed &= expect(window.getNativeHandle() != nullptr, "native window handle should be available");
        passed &= expect(width == TEST_WINDOW_WIDTH, "window width should match creation options");
        passed &= expect(height == TEST_WINDOW_HEIGHT, "window height should match creation options");
        passed &= expect(drawableWidth > 0, "drawable width should be positive");
        passed &= expect(drawableHeight > 0, "drawable height should be positive");
        passed &= expect(window.isHidden(), "hidden window should report hidden state");
        passed &= expect(!window.shouldClose(), "new window should not request close");
        passed &= expect(!window.isBorderlessFullscreen(), "hidden test window should start windowed");

        window.setBorderlessFullscreen(false);
        passed &= expect(!window.isBorderlessFullscreen(), "fullscreen disabled should keep the window windowed");

        Platform::Window movedWindow(std::move(window));
        passed &= expect(movedWindow.getNativeHandle() != nullptr, "move construction should transfer native handle");

        Platform::Window assignedWindow(makeHiddenOptions());
        assignedWindow = std::move(movedWindow);
        passed &= expect(assignedWindow.getNativeHandle() != nullptr, "move assignment should transfer native handle");

        return passed;
    }

    /**
     * @brief Verifies that SDL window events for the owned window become engine-facing window events.
     * @return True when a synthetic move event is translated with its payload intact.
     */
    bool testWindowEventTranslation()
    {
        Platform::Window window(makeHiddenOptions());
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = SDL_GetWindowID(window.getNativeHandle());

        bool passed = true;
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_MOVED, windowId, 11, 22), "should push moved event");

        Platform::WindowEvent event{};
        passed &= expect(pollUntil(window, event, Platform::WindowEventType::Moved), "moved event should be translated");
        passed &= expect(event.type == Platform::WindowEventType::Moved, "moved event type should match");
        passed &= expect(event.x == 11, "moved event x should match");
        passed &= expect(event.y == 22, "moved event y should match");

        return passed;
    }

    /**
     * @brief Verifies that non-window SDL events remain available to other subsystems.
     * @return True when keyboard events are not consumed by Window::pollEvent.
     */
    bool testEventFiltering()
    {
        Platform::Window window(makeHiddenOptions());
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

        SDL_Event keyboardEvent{};
        keyboardEvent.type = SDL_EVENT_KEY_DOWN;
        if (!SDL_PushEvent(&keyboardEvent))
        {
            std::cerr << "FAILED: should push keyboard event" << std::endl;
            return false;
        }

        Platform::WindowEvent event{};
        const bool noWindowEvent = !window.pollEvent(event);
        SDL_Event preservedEvent{};
        const bool inputEventPreserved = SDL_PollEvent(&preservedEvent);

        bool passed = true;
        passed &= expect(noWindowEvent, "input events should not be translated as window events");
        passed &= expect(inputEventPreserved, "input events should remain available to other systems");
        passed &= expect(preservedEvent.type == SDL_EVENT_KEY_DOWN, "preserved event should be the keyboard event");

        return passed;
    }

    /**
     * @brief Verifies that window events for another SDL window are replayed instead of consumed.
     * @return True when a foreign window event remains visible after polling the owned window.
     */
    bool testForeignWindowEventPreserved()
    {
        Platform::Window window(makeHiddenOptions());
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

        SDL_Window* foreignWindow = SDL_CreateWindow("RaythmDemo Foreign Test", 160, 120, SDL_WINDOW_HIDDEN);
        if (foreignWindow == nullptr)
        {
            std::cerr << "FAILED: should create foreign SDL window: " << SDL_GetError() << std::endl;
            return false;
        }

        const SDL_WindowID foreignWindowId = SDL_GetWindowID(foreignWindow);
        const bool pushed = pushWindowEvent(SDL_EVENT_WINDOW_MOVED, foreignWindowId, 44, 55);

        Platform::WindowEvent event{};
        const bool noOwnedEvent = !window.pollEvent(event);

        SDL_Event preservedEvent{};
        bool foreignEventPreserved = false;
        while (SDL_PeepEvents(&preservedEvent, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST) == 1)
        {
            if (preservedEvent.window.windowID == foreignWindowId)
            {
                foreignEventPreserved = true;
                break;
            }
        }

        bool passed = true;
        passed &= expect(pushed, "should push foreign window event");
        passed &= expect(noOwnedEvent, "foreign window event should not be translated");
        passed &= expect(foreignEventPreserved, "foreign window event should remain available");
        passed &= expect(preservedEvent.window.windowID == foreignWindowId,
                         "preserved window event should belong to the foreign window");
        passed &= expect(preservedEvent.type >= SDL_EVENT_WINDOW_FIRST && preservedEvent.type <= SDL_EVENT_WINDOW_LAST,
                         "preserved event should be a window event");

        SDL_DestroyWindow(foreignWindow);
        return passed;
    }

    /**
     * @brief Verifies that unsupported owned window events are consumed instead of replayed forever.
     * @return True when an unsupported event does not block a later supported event.
     */
    bool testUnsupportedOwnedWindowEventIsConsumed()
    {
        Platform::Window window(makeHiddenOptions());
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = SDL_GetWindowID(window.getNativeHandle());

        bool passed = true;
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_HIT_TEST, windowId), "should push unsupported hit-test event");
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_MOVED, windowId, 77, 88), "should push moved event after hit-test");

        Platform::WindowEvent event{};
        passed &= expect(pollUntil(window, event, Platform::WindowEventType::Moved),
                         "supported event after unsupported event should be reachable");
        passed &= expect(event.x == 77, "moved event after unsupported event should keep x payload");
        passed &= expect(event.y == 88, "moved event after unsupported event should keep y payload");

        return passed;
    }

    /**
     * @brief Verifies that application-level quit events close the window abstraction.
     * @return True when SDL_EVENT_QUIT translates into a sticky close request.
     */
    bool testQuitEventRequestsClose()
    {
        Platform::Window window(makeHiddenOptions());
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

        SDL_Event quitEvent{};
        quitEvent.type = SDL_EVENT_QUIT;
        bool passed = true;
        passed &= expect(SDL_PushEvent(&quitEvent), "should push quit event");

        Platform::WindowEvent event{};
        passed &= expect(window.pollEvent(event), "quit event should be translated");
        passed &= expect(event.type == Platform::WindowEventType::QuitRequested, "quit event type should match");
        passed &= expect(window.shouldClose(), "quit event should set close flag");

        return passed;
    }

    /**
     * @brief Verifies SDL Vulkan extension discovery through the Window wrapper.
     * @return True when extensions are valid, or when Vulkan window support is unavailable on the host.
     * @note Missing Vulkan window support is treated as a skip so headless or minimal CI hosts can still run tests.
     */
    bool testVulkanExtensionQuery()
    {
        Platform::WindowOptions options = makeHiddenOptions();
        options.vulkanSurface = true;

        try
        {
            Platform::Window window(options);
            const auto extensions = window.getRequiredVulkanInstanceExtensions();

            bool passed = true;
            passed &= expect(!extensions.empty(), "SDL should report Vulkan instance extensions");
            for (const char* extension : extensions)
            {
                passed &= expect(extension != nullptr && extension[0] != '\0', "Vulkan extension name should be valid");
            }

            return passed;
        }
        catch (const std::exception& exception)
        {
            std::cout << "Vulkan window support is unavailable; skipping Vulkan extension query: "
                      << exception.what() << std::endl;
            return true;
        }
    }
}

/**
 * @brief Runs all SDL window wrapper tests under an initialized SDL video subsystem.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Zero when all window wrapper checks pass; nonzero when any check fails.
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

    bool allTestsPassed = true;

    try
    {
        allTestsPassed &= testWindowLifecycleAndState();
        allTestsPassed &= testWindowEventTranslation();
        allTestsPassed &= testEventFiltering();
        allTestsPassed &= testForeignWindowEventPreserved();
        allTestsPassed &= testUnsupportedOwnedWindowEventIsConsumed();
        allTestsPassed &= testQuitEventRequestsClose();
        allTestsPassed &= testVulkanExtensionQuery();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Unhandled exception: " << exception.what() << std::endl;
        allTestsPassed = false;
    }

    SDL_Quit();
    return allTestsPassed ? 0 : 1;
}
