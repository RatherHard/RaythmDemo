// RaythmDemo - SDL Event Pump
// Defines the thin SDL event pump used to translate platform window events.
// Author: RatherHard
// Date: 2026-07-04

#pragma once

#include <cstdint>
#include <deque>

#include <SDL3/SDL.h>

#include "Platform/Window.hpp"

namespace Raythm::Platform
{
    /**
     * @brief Engine-facing input event kinds translated from SDL device input events.
     */
    enum class InputEventType
    {
        /** @brief No translated input event is available. */
        None,

        /** @brief A keyboard key was pressed. */
        KeyPressed,

        /** @brief A keyboard key was released. */
        KeyReleased,

        /** @brief A mouse button was pressed. */
        MouseButtonPressed,

        /** @brief A mouse button was released. */
        MouseButtonReleased,

        /** @brief The mouse moved within the owning window. */
        MouseMoved
    };

    /**
     * @brief Engine-facing device input event payload.
     */
    struct InputEvent
    {
        /** @brief Translated input event kind, or None when the payload is empty. */
        InputEventType type = InputEventType::None;

        /** @brief SDL event timestamp in nanoseconds for input timing reconstruction. */
        std::uint64_t timestampNanoseconds = 0;

        /** @brief SDL window identifier that owns this input event. */
        SDL_WindowID windowId = 0;

        /** @brief SDL physical key code for keyboard events. */
        SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;

        /** @brief SDL virtual key code for keyboard events. */
        SDL_Keycode key = SDLK_UNKNOWN;

        /** @brief True when SDL marks a key-down event as repeat. */
        bool isRepeat = false;

        /** @brief Mouse button index for mouse button events. */
        Uint8 mouseButton = 0;

        /** @brief Click count for mouse button events. */
        Uint8 clicks = 0;

        /** @brief Active mouse button bitmask for mouse motion events. */
        SDL_MouseButtonFlags mouseButtonState = 0;

        /** @brief Cursor x coordinate relative to the window for mouse events. */
        float x = 0.0F;

        /** @brief Cursor y coordinate relative to the window for mouse events. */
        float y = 0.0F;

        /** @brief Relative x movement for mouse motion events. */
        float xRelative = 0.0F;

        /** @brief Relative y movement for mouse motion events. */
        float yRelative = 0.0F;
    };

    /**
     * @brief Engine-facing platform event category produced by the unified event pump.
     */
    enum class PlatformEventType
    {
        /** @brief No translated event is available. */
        None,

        /** @brief The payload contains a translated window event. */
        Window,

        /** @brief The payload contains a translated device input event. */
        Input
    };

    /**
     * @brief Engine-facing platform event payload produced from the SDL global queue.
     */
    struct PlatformEvent
    {
        /** @brief Category that determines which payload field is valid. */
        PlatformEventType type = PlatformEventType::None;

        /** @brief Window lifecycle payload, valid when type is Window. */
        WindowEvent window{};

        /** @brief Device input payload, valid when type is Input. */
        InputEvent input{};
    };

    /**
     * @brief Thin SDL event pump that translates global SDL events into engine-facing platform events.
     */
    class EventPump
    {
    public:
        /**
         * @brief Polls the SDL queue until a window event for the requested SDL window is found.
         * @param event Receives the translated window event.
         * @param windowId SDL window identifier that should own translated window events.
         * @return True when a translated window event was produced.
         * @note Non-window events are preserved internally for later input or unified polling calls.
         */
        bool pollWindowEvent(WindowEvent& event, SDL_WindowID windowId);

        /**
         * @brief Polls the SDL queue until a platform event for the requested SDL window is found.
         * @param event Receives the translated platform event.
         * @param windowId SDL window identifier that should own translated events.
         * @return True when a translated window or input event was produced.
         * @note Unsupported events are consumed; supported events for other windows are preserved internally.
         */
        bool pollEvent(PlatformEvent& event, SDL_WindowID windowId);

        /**
         * @brief Polls the SDL queue until an input event for the requested SDL window is found.
         * @param event Receives the translated input event.
         * @param windowId SDL window identifier that should own translated input events.
         * @return True when a translated input event was produced.
         * @note Non-input events are preserved internally for later window or unified polling calls.
         */
        bool pollInputEvent(InputEvent& event, SDL_WindowID windowId);

    private:
        /**
         * @brief Converts an SDL window event for the requested native window into an engine event payload.
         * @param sdlEvent SDL event read from the event queue.
         * @param event Receives the translated engine-facing event on success.
         * @param windowId SDL identifier for the target native window.
         * @return True when the SDL event belongs to this window and maps to a supported window event.
         */
        static bool translateWindowEvent(
            const SDL_Event& sdlEvent,
            WindowEvent& event,
            SDL_WindowID windowId
        ) noexcept;

        /**
         * @brief Converts an SDL input event for the requested native window into an engine event payload.
         * @param sdlEvent SDL event read from the event queue.
         * @param event Receives the translated engine-facing event on success.
         * @param windowId SDL identifier for the target native window.
         * @return True when the SDL event belongs to this window and maps to a supported input event.
         */
        static bool translateInputEvent(
            const SDL_Event& sdlEvent,
            InputEvent& event,
            SDL_WindowID windowId
        ) noexcept;

        /**
         * @brief Preserves an SDL event for a later polling path without allowing unbounded growth.
         * @param event SDL event that should remain observable by a later caller.
         */
        void preserveEvent(const SDL_Event& event);

        /** @brief SDL events preserved for other polling paths or windows. */
        std::deque<SDL_Event> m_pendingEvents{};
    };
}
