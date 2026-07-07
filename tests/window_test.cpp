// RaythmDemo - SDL Window Wrapper Tests
// Exercises the SDL-backed Window wrapper lifecycle, event pump, and Vulkan extension query behavior.
// Author: RatherHard
// Date: 2026-07-04

#include "Platform/EventPump.hpp"
#include "Platform/Window.hpp"

#include <cstdint>
#include <iostream>
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
     * @param eventPump SDL event pump being checked.
     * @param windowId SDL window identifier that should own translated events.
     * @param event Receives the matching event payload when one is found.
     * @param expectedType Event type that should appear in the wrapper event stream.
     * @return True when the expected event type is observed before the queue is exhausted.
     */
    bool pollUntil(
        Platform::EventPump& eventPump,
        SDL_WindowID windowId,
        Platform::WindowEvent& event,
        Platform::WindowEventType expectedType
    )
    {
        while (eventPump.pollWindowEvent(event, windowId))
        {
            if (event.type == expectedType)
            {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Polls translated input events until the expected event type appears.
     * @param eventPump SDL event pump being checked.
     * @param windowId SDL window identifier that should own translated events.
     * @param event Receives the matching event payload when one is found.
     * @param expectedType Input event type that should appear in the wrapper event stream.
     * @return True when the expected event type is observed before the queue is exhausted.
     */
    bool pollUntil(
        Platform::EventPump& eventPump,
        SDL_WindowID windowId,
        Platform::InputEvent& event,
        Platform::InputEventType expectedType
    )
    {
        while (eventPump.pollInputEvent(event, windowId))
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
     * @brief Pushes a synthetic SDL keyboard event into the global SDL event queue.
     * @param type SDL keyboard event type to simulate.
     * @param windowId SDL window identifier that should own the simulated event.
     * @param scancode Physical key code to preserve in the translated payload.
     * @param key Virtual key code to preserve in the translated payload.
     * @param isRepeat True when simulating a repeat key press.
     * @return True when SDL accepts the event into its queue.
     */
    bool pushKeyboardEvent(
        SDL_EventType type,
        SDL_WindowID windowId,
        SDL_Scancode scancode,
        SDL_Keycode key,
        bool isRepeat = false,
        std::uint64_t timestampNanoseconds = 0)
    {
        SDL_Event event{};
        event.key.type = type;
        event.key.timestamp = timestampNanoseconds;
        event.key.windowID = windowId;
        event.key.scancode = scancode;
        event.key.key = key;
        event.key.repeat = isRepeat;

        return SDL_PushEvent(&event);
    }

    /**
     * @brief Pushes a synthetic SDL mouse button event into the global SDL event queue.
     * @param type SDL mouse button event type to simulate.
     * @param windowId SDL window identifier that should own the simulated event.
     * @param button Mouse button index to preserve in the translated payload.
     * @param clicks Click count to preserve in the translated payload.
     * @param x Cursor x coordinate relative to the window.
     * @param y Cursor y coordinate relative to the window.
     * @return True when SDL accepts the event into its queue.
     */
    bool pushMouseButtonEvent(
        SDL_EventType type,
        SDL_WindowID windowId,
        Uint8 button,
        Uint8 clicks,
        float x,
        float y,
        std::uint64_t timestampNanoseconds = 0
    )
    {
        SDL_Event event{};
        event.button.type = type;
        event.button.timestamp = timestampNanoseconds;
        event.button.windowID = windowId;
        event.button.button = button;
        event.button.clicks = clicks;
        event.button.x = x;
        event.button.y = y;

        return SDL_PushEvent(&event);
    }

    /**
     * @brief Pushes a synthetic SDL mouse motion event into the global SDL event queue.
     * @param windowId SDL window identifier that should own the simulated event.
     * @param state Mouse button bitmask active during the motion event.
     * @param x Cursor x coordinate relative to the window.
     * @param y Cursor y coordinate relative to the window.
     * @param xRelative Relative x movement since the previous motion event.
     * @param yRelative Relative y movement since the previous motion event.
     * @return True when SDL accepts the event into its queue.
     */
    bool pushMouseMotionEvent(
        SDL_WindowID windowId,
        SDL_MouseButtonFlags state,
        float x,
        float y,
        float xRelative,
        float yRelative,
        std::uint64_t timestampNanoseconds = 0
    )
    {
        SDL_Event event{};
        event.motion.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.timestamp = timestampNanoseconds;
        event.motion.windowID = windowId;
        event.motion.state = state;
        event.motion.x = x;
        event.motion.y = y;
        event.motion.xrel = xRelative;
        event.motion.yrel = yRelative;

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
        passed &= expect(window.getWindowId() != 0, "window id should be available");
        passed &= expect(width == TEST_WINDOW_WIDTH, "window width should match creation options");
        passed &= expect(height == TEST_WINDOW_HEIGHT, "window height should match creation options");
        passed &= expect(drawableWidth > 0, "drawable width should be positive");
        passed &= expect(drawableHeight > 0, "drawable height should be positive");
        passed &= expect(window.isHidden(), "hidden window should report hidden state");
        passed &= expect(!window.shouldClose(), "new window should not request close");
        passed &= expect(!window.isBorderlessFullscreen(), "hidden test window should start windowed");

        window.setBorderlessFullscreen(false);
        passed &= expect(!window.isBorderlessFullscreen(), "fullscreen disabled should keep the window windowed");
        (void)window.requestInputFocus();
        passed &= expect(!window.shouldClose(), "best-effort focus request should not close the window");

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
    bool testEventPumpWindowEventTranslation()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = window.getWindowId();

        bool passed = true;
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_MOVED, windowId, 11, 22), "should push moved event");

        Platform::WindowEvent event{};
        passed &= expect(pollUntil(eventPump, windowId, event, Platform::WindowEventType::Moved),
                         "moved event should be translated");
        passed &= expect(event.type == Platform::WindowEventType::Moved, "moved event type should match");
        passed &= expect(event.x == 11, "moved event x should match");
        passed &= expect(event.y == 22, "moved event y should match");

        return passed;
    }

    /**
     * @brief Verifies that non-window SDL events do not block later window lifecycle events.
     * @return True when a keyboard event ahead of a close event is consumed and the close event is translated.
     */
    bool testEventPumpSkipsNonWindowEventsBeforeWindowEvent()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

        SDL_Event keyboardEvent{};
        keyboardEvent.type = SDL_EVENT_KEY_DOWN;

        bool passed = true;
        passed &= expect(SDL_PushEvent(&keyboardEvent), "should push keyboard event");
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, window.getWindowId()),
                         "should push close event after keyboard event");

        Platform::WindowEvent event{};
        passed &= expect(eventPump.pollWindowEvent(event, window.getWindowId()),
                         "window event after non-window event should be reachable");
        passed &= expect(event.type == Platform::WindowEventType::CloseRequested, "close event type should match");

        return passed;
    }

    /**
     * @brief Verifies that events for another SDL window do not block the requested window.
     * @return True when an owned window event after a foreign event remains reachable.
     */
    bool testEventPumpSkipsForeignWindowEventsBeforeOwnedEvent()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

        SDL_Window* foreignWindow = SDL_CreateWindow("RaythmDemo Foreign Test", 160, 120, SDL_WINDOW_HIDDEN);
        if (foreignWindow == nullptr)
        {
            std::cerr << "FAILED: should create foreign SDL window: " << SDL_GetError() << std::endl;
            return false;
        }

        const SDL_WindowID foreignWindowId = SDL_GetWindowID(foreignWindow);

        bool passed = true;
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_MOVED, foreignWindowId, 44, 55),
                         "should push foreign window event");
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_RESIZED, window.getWindowId(), 640, 480),
                         "should push owned resize event after foreign event");

        Platform::WindowEvent event{};
        passed &= expect(eventPump.pollWindowEvent(event, window.getWindowId()),
                         "owned window event after foreign event should be reachable");
        passed &= expect(event.type == Platform::WindowEventType::Resized, "owned resize event type should match");
        passed &= expect(event.width == 640, "owned resize event width should match");
        passed &= expect(event.height == 480, "owned resize event height should match");

        SDL_DestroyWindow(foreignWindow);
        return passed;
    }

    /**
     * @brief Verifies that unsupported owned window events are consumed instead of replayed forever.
     * @return True when an unsupported event does not block a later supported event.
     */
    bool testEventPumpConsumesUnsupportedOwnedWindowEvent()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = window.getWindowId();

        bool passed = true;
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_HIT_TEST, windowId), "should push unsupported hit-test event");
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_MOVED, windowId, 77, 88), "should push moved event after hit-test");

        Platform::WindowEvent event{};
        passed &= expect(pollUntil(eventPump, windowId, event, Platform::WindowEventType::Moved),
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
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

        SDL_Event quitEvent{};
        quitEvent.type = SDL_EVENT_QUIT;
        bool passed = true;
        passed &= expect(SDL_PushEvent(&quitEvent), "should push quit event");

        Platform::WindowEvent event{};
        passed &= expect(eventPump.pollWindowEvent(event, window.getWindowId()), "quit event should be translated");
        passed &= expect(event.type == Platform::WindowEventType::QuitRequested, "quit event type should match");

        window.applyEvent(event);
        passed &= expect(window.shouldClose(), "quit event should set close flag");

        return passed;
    }

    /**
     * @brief Verifies that close-request events apply a sticky close state to the window abstraction.
     * @return True when a close-request event sets shouldClose.
     */
    bool testCloseRequestedEventRequestsClose()
    {
        Platform::Window window(makeHiddenOptions());
        bool passed = true;
        passed &= expect(!window.shouldClose(), "new window should not request close");

        Platform::WindowEvent event{};
        event.type = Platform::WindowEventType::CloseRequested;
        window.applyEvent(event);
        passed &= expect(window.shouldClose(), "close-request event should set close flag");

        return passed;
    }

    /**
     * @brief Verifies keyboard events become engine-facing input events with key identity preserved.
     * @return True when key down and up events translate with expected payloads.
     */
    bool testEventPumpKeyboardEventTranslation()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = window.getWindowId();

        bool passed = true;
        passed &= expect(pushKeyboardEvent(
            SDL_EVENT_KEY_DOWN,
            windowId,
            SDL_SCANCODE_SPACE,
            SDLK_SPACE,
            true,
            12'345'678ULL),
            "should push key down event");

        Platform::InputEvent event{};
        passed &= expect(pollUntil(eventPump, windowId, event, Platform::InputEventType::KeyPressed),
                         "key down event should be translated");
        passed &= expect(event.type == Platform::InputEventType::KeyPressed, "key down type should match");
        passed &= expect(event.windowId == windowId, "keyboard window id should match");
        passed &= expect(event.scancode == SDL_SCANCODE_SPACE, "keyboard scancode should match");
        passed &= expect(event.key == SDLK_SPACE, "keyboard keycode should match");
        passed &= expect(event.isRepeat, "keyboard repeat flag should match");
        passed &= expect(event.timestampNanoseconds == 12'345'678ULL, "keyboard timestamp should match");

        passed &= expect(pushKeyboardEvent(
            SDL_EVENT_KEY_UP,
            windowId,
            SDL_SCANCODE_SPACE,
            SDLK_SPACE,
            false,
            12'345'999ULL),
            "should push key up event");
        passed &= expect(pollUntil(eventPump, windowId, event, Platform::InputEventType::KeyReleased),
                         "key up event should be translated");
        passed &= expect(event.type == Platform::InputEventType::KeyReleased, "key up type should match");
        passed &= expect(!event.isRepeat, "key up repeat flag should remain false");
        passed &= expect(event.timestampNanoseconds == 12'345'999ULL, "key up timestamp should match");

        return passed;
    }

    /**
     * @brief Verifies mouse button events become engine-facing input events with click payloads preserved.
     * @return True when mouse button down and up events translate with expected payloads.
     */
    bool testEventPumpMouseButtonEventTranslation()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = window.getWindowId();

        bool passed = true;
        passed &= expect(pushMouseButtonEvent(
            SDL_EVENT_MOUSE_BUTTON_DOWN,
            windowId,
            SDL_BUTTON_LEFT,
            2,
            13.5F,
            24.25F,
            22'000'000ULL),
            "should push mouse button down event");

        Platform::InputEvent event{};
        passed &= expect(pollUntil(eventPump, windowId, event, Platform::InputEventType::MouseButtonPressed),
                         "mouse button down event should be translated");
        passed &= expect(event.type == Platform::InputEventType::MouseButtonPressed, "button down type should match");
        passed &= expect(event.windowId == windowId, "mouse button window id should match");
        passed &= expect(event.mouseButton == SDL_BUTTON_LEFT, "mouse button index should match");
        passed &= expect(event.clicks == 2, "mouse button click count should match");
        passed &= expect(event.x == 13.5F, "mouse button x should match");
        passed &= expect(event.y == 24.25F, "mouse button y should match");
        passed &= expect(event.timestampNanoseconds == 22'000'000ULL, "mouse button timestamp should match");

        passed &= expect(pushMouseButtonEvent(
            SDL_EVENT_MOUSE_BUTTON_UP,
            windowId,
            SDL_BUTTON_LEFT,
            1,
            15.0F,
            26.0F,
            22'000'333ULL),
            "should push mouse button up event");
        passed &= expect(pollUntil(eventPump, windowId, event, Platform::InputEventType::MouseButtonReleased),
                         "mouse button up event should be translated");
        passed &= expect(event.type == Platform::InputEventType::MouseButtonReleased, "button up type should match");
        passed &= expect(event.clicks == 1, "mouse button up click count should match");
        passed &= expect(event.x == 15.0F, "mouse button up x should match");
        passed &= expect(event.y == 26.0F, "mouse button up y should match");
        passed &= expect(event.timestampNanoseconds == 22'000'333ULL, "mouse button up timestamp should match");

        return passed;
    }

    /**
     * @brief Verifies mouse motion events preserve absolute position, relative delta, and button state.
     * @return True when a synthetic mouse motion event translates with the expected payload.
     */
    bool testEventPumpMouseMotionTranslation()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = window.getWindowId();
        const SDL_MouseButtonFlags buttonState = SDL_BUTTON_LMASK | SDL_BUTTON_RMASK;

        bool passed = true;
        passed &= expect(pushMouseMotionEvent(windowId, buttonState, 30.0F, 40.0F, -2.0F, 3.5F, 33'444'555ULL),
                         "should push mouse motion event");

        Platform::InputEvent event{};
        passed &= expect(pollUntil(eventPump, windowId, event, Platform::InputEventType::MouseMoved),
                         "mouse motion event should be translated");
        passed &= expect(event.type == Platform::InputEventType::MouseMoved, "mouse motion type should match");
        passed &= expect(event.windowId == windowId, "mouse motion window id should match");
        passed &= expect(event.mouseButtonState == buttonState, "mouse motion button state should match");
        passed &= expect(event.x == 30.0F, "mouse motion x should match");
        passed &= expect(event.y == 40.0F, "mouse motion y should match");
        passed &= expect(event.xRelative == -2.0F, "mouse motion relative x should match");
        passed &= expect(event.yRelative == 3.5F, "mouse motion relative y should match");
        passed &= expect(event.timestampNanoseconds == 33'444'555ULL, "mouse motion timestamp should match");

        return passed;
    }

    /**
     * @brief Verifies input polling preserves queued window events for later window polling.
     * @return True when a close request remains observable after an input-only poll.
     */
    bool testInputPollingPreservesWindowEvents()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = window.getWindowId();

        bool passed = true;
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, windowId), "should push close event");
        passed &= expect(pushKeyboardEvent(SDL_EVENT_KEY_DOWN, windowId, SDL_SCANCODE_A, SDLK_A),
                         "should push key event after close");

        Platform::InputEvent inputEvent{};
        passed &= expect(eventPump.pollInputEvent(inputEvent, windowId), "input polling should find key event");
        passed &= expect(inputEvent.type == Platform::InputEventType::KeyPressed, "input polling key type should match");

        Platform::WindowEvent windowEvent{};
        passed &= expect(eventPump.pollWindowEvent(windowEvent, windowId), "window event should survive input polling");
        passed &= expect(windowEvent.type == Platform::WindowEventType::CloseRequested,
                         "preserved close event type should match");

        return passed;
    }

    /**
     * @brief Verifies window polling preserves queued input events for later input polling.
     * @return True when a key event remains observable after a window-only poll.
     */
    bool testWindowPollingPreservesInputEvents()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = window.getWindowId();

        bool passed = true;
        passed &= expect(pushKeyboardEvent(SDL_EVENT_KEY_DOWN, windowId, SDL_SCANCODE_A, SDLK_A),
                         "should push key event");
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, windowId), "should push close after key");

        Platform::WindowEvent windowEvent{};
        passed &= expect(eventPump.pollWindowEvent(windowEvent, windowId), "window polling should find close event");
        passed &= expect(windowEvent.type == Platform::WindowEventType::CloseRequested,
                         "window polling close type should match");

        Platform::InputEvent inputEvent{};
        passed &= expect(eventPump.pollInputEvent(inputEvent, windowId), "input event should survive window polling");
        passed &= expect(inputEvent.type == Platform::InputEventType::KeyPressed, "preserved key event type should match");
        passed &= expect(inputEvent.scancode == SDL_SCANCODE_A, "preserved key scancode should match");
        passed &= expect(inputEvent.key == SDLK_A, "preserved keycode should match");

        return passed;
    }

    /**
     * @brief Verifies polling one SDL window preserves events owned by another SDL window.
     * @return True when foreign and owned window events are both delivered to their requested windows.
     */
    bool testEventPumpPreservesForeignWindowEvents()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

        SDL_Window* foreignWindow = SDL_CreateWindow("RaythmDemo Preserved Foreign Test", 160, 120, SDL_WINDOW_HIDDEN);
        if (foreignWindow == nullptr)
        {
            std::cerr << "FAILED: should create foreign SDL window: " << SDL_GetError() << std::endl;
            return false;
        }

        const SDL_WindowID ownedWindowId = window.getWindowId();
        const SDL_WindowID foreignWindowId = SDL_GetWindowID(foreignWindow);
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

        bool passed = true;
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_MOVED, foreignWindowId, 44, 55),
                         "should push foreign window event");
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_RESIZED, ownedWindowId, 640, 480),
                         "should push owned window event");

        Platform::PlatformEvent event{};
        passed &= expect(eventPump.pollEvent(event, ownedWindowId), "owned window should receive its event");
        passed &= expect(event.type == Platform::PlatformEventType::Window, "owned platform event type should match");
        passed &= expect(event.window.type == Platform::WindowEventType::Resized, "owned resize type should match");

        passed &= expect(eventPump.pollEvent(event, foreignWindowId), "foreign window event should be preserved");
        passed &= expect(event.type == Platform::PlatformEventType::Window, "foreign platform event type should match");
        passed &= expect(event.window.type == Platform::WindowEventType::Moved, "foreign moved type should match");
        passed &= expect(event.window.x == 44, "foreign moved x should match");
        passed &= expect(event.window.y == 55, "foreign moved y should match");

        SDL_DestroyWindow(foreignWindow);
        return passed;
    }

    /**
     * @brief Verifies that focus transitions are delivered through the unified platform event stream in order.
     * @return True when focus lost and focus gained events are translated without reordering.
     */
    bool testEventPumpUnifiedPlatformFocusEventsTranslateInOrder()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = window.getWindowId();

        bool passed = true;
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_FOCUS_LOST, windowId), "should push focus lost event");
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED, windowId), "should push focus gained event");

        Platform::PlatformEvent event{};
        passed &= expect(eventPump.pollEvent(event, windowId), "unified stream should translate focus lost event");
        passed &= expect(event.type == Platform::PlatformEventType::Window, "focus lost should be a window event");
        passed &= expect(event.window.type == Platform::WindowEventType::FocusLost, "focus lost type should match");

        passed &= expect(eventPump.pollEvent(event, windowId), "unified stream should translate focus gained event");
        passed &= expect(event.type == Platform::PlatformEventType::Window, "focus gained should be a window event");
        passed &= expect(event.window.type == Platform::WindowEventType::FocusGained, "focus gained type should match");

        return passed;
    }

    /**
     * @brief Verifies the unified platform event stream preserves window and input event order.
     * @return True when window and input payloads are delivered through one polling entrypoint.
     */
    bool testEventPumpUnifiedPlatformEventTranslation()
    {
        Platform::Window window(makeHiddenOptions());
        Platform::EventPump eventPump{};
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const SDL_WindowID windowId = window.getWindowId();

        bool passed = true;
        passed &= expect(pushWindowEvent(SDL_EVENT_WINDOW_RESIZED, windowId, 800, 600),
                         "should push resize event");
        passed &= expect(pushKeyboardEvent(SDL_EVENT_KEY_DOWN, windowId, SDL_SCANCODE_A, SDLK_A),
                         "should push key event after resize");

        Platform::PlatformEvent event{};
        passed &= expect(eventPump.pollEvent(event, windowId), "unified stream should translate first event");
        passed &= expect(event.type == Platform::PlatformEventType::Window, "first unified event should be window event");
        passed &= expect(event.window.type == Platform::WindowEventType::Resized, "first window event type should match");
        passed &= expect(event.window.width == 800, "unified resize width should match");
        passed &= expect(event.window.height == 600, "unified resize height should match");

        passed &= expect(eventPump.pollEvent(event, windowId), "unified stream should translate second event");
        passed &= expect(event.type == Platform::PlatformEventType::Input, "second unified event should be input event");
        passed &= expect(event.input.type == Platform::InputEventType::KeyPressed, "second input event type should match");
        passed &= expect(event.input.scancode == SDL_SCANCODE_A, "unified key scancode should match");
        passed &= expect(event.input.key == SDLK_A, "unified keycode should match");

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
        allTestsPassed &= testEventPumpWindowEventTranslation();
        allTestsPassed &= testEventPumpSkipsNonWindowEventsBeforeWindowEvent();
        allTestsPassed &= testEventPumpSkipsForeignWindowEventsBeforeOwnedEvent();
        allTestsPassed &= testEventPumpConsumesUnsupportedOwnedWindowEvent();
        allTestsPassed &= testQuitEventRequestsClose();
        allTestsPassed &= testCloseRequestedEventRequestsClose();
        allTestsPassed &= testEventPumpKeyboardEventTranslation();
        allTestsPassed &= testEventPumpMouseButtonEventTranslation();
        allTestsPassed &= testEventPumpMouseMotionTranslation();
        allTestsPassed &= testInputPollingPreservesWindowEvents();
        allTestsPassed &= testWindowPollingPreservesInputEvents();
        allTestsPassed &= testEventPumpPreservesForeignWindowEvents();
        allTestsPassed &= testEventPumpUnifiedPlatformFocusEventsTranslateInOrder();
        allTestsPassed &= testEventPumpUnifiedPlatformEventTranslation();
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
