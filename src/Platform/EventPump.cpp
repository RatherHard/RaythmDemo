// RaythmDemo - SDL Event Pump Implementation
// Implements SDL global queue polling and window/input event translation.
// Author: RatherHard
// Date: 2026-07-04

#include "Platform/EventPump.hpp"

#include <cstddef>

namespace Raythm::Platform
{
    namespace
    {
        /** @brief Maximum number of SDL events preserved across filtered polling calls. */
        constexpr std::size_t MAX_PENDING_EVENTS = 1024;

        /** @brief Maximum number of fresh SDL events one poll call may inspect before yielding to the caller. */
        constexpr std::size_t MAX_FRESH_EVENTS_PER_POLL = 1024;

        /**
         * @brief Reports whether an SDL event is a window lifecycle event.
         * @param eventType SDL event type to classify.
         * @return True when the event type is in SDL's window event range.
         */
        bool isWindowEvent(Uint32 eventType) noexcept
        {
            return eventType >= SDL_EVENT_WINDOW_FIRST && eventType <= SDL_EVENT_WINDOW_LAST;
        }

        /**
         * @brief Reports whether an SDL event carries keyboard or mouse input for a window.
         * @param eventType SDL event type to classify.
         * @return True when the event type is part of the currently supported input boundary.
         */
        bool isInputEvent(Uint32 eventType) noexcept
        {
            switch (eventType)
            {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_MOTION:
                return true;
            default:
                return false;
            }
        }

        /**
         * @brief Reads the owning SDL window identifier from a supported input event.
         * @param sdlEvent SDL event read from the queue.
         * @return SDL window id for supported input events, or zero for unsupported events.
         */
        SDL_WindowID getInputWindowId(const SDL_Event& sdlEvent) noexcept
        {
            switch (sdlEvent.type)
            {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                return sdlEvent.key.windowID;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                return sdlEvent.button.windowID;
            case SDL_EVENT_MOUSE_MOTION:
                return sdlEvent.motion.windowID;
            default:
                return 0;
            }
        }
    }

    bool EventPump::pollWindowEvent(WindowEvent& event, SDL_WindowID windowId)
    {
        if (windowId == 0)
        {
            return false;
        }

        const std::size_t pendingEventsToCheck = m_pendingEvents.size();
        std::size_t preservedEventsChecked = 0;
        std::size_t freshEventsChecked = 0;
        while (true)
        {
            SDL_Event sdlEvent{};
            if (preservedEventsChecked < pendingEventsToCheck)
            {
                sdlEvent = m_pendingEvents.front();
                m_pendingEvents.pop_front();
                ++preservedEventsChecked;
            }
            else if (freshEventsChecked >= MAX_FRESH_EVENTS_PER_POLL ||
                     SDL_PeepEvents(&sdlEvent, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) != 1)
            {
                return false;
            }
            else
            {
                ++freshEventsChecked;
            }

            if (sdlEvent.type == SDL_EVENT_QUIT)
            {
                event = {};
                event.type = WindowEventType::QuitRequested;
                return true;
            }

            if (isInputEvent(sdlEvent.type))
            {
                preserveEvent(sdlEvent);
                continue;
            }

            if (!isWindowEvent(sdlEvent.type))
            {
                continue;
            }

            if (sdlEvent.window.windowID != windowId)
            {
                preserveEvent(sdlEvent);
                continue;
            }

            WindowEvent translatedEvent{};
            if (!translateWindowEvent(sdlEvent, translatedEvent, windowId))
            {
                continue;
            }

            event = translatedEvent;
            return true;
        }
    }

    bool EventPump::pollEvent(PlatformEvent& event, SDL_WindowID windowId)
    {
        if (windowId == 0)
        {
            return false;
        }

        const std::size_t pendingEventsToCheck = m_pendingEvents.size();
        std::size_t preservedEventsChecked = 0;
        std::size_t freshEventsChecked = 0;
        while (true)
        {
            SDL_Event sdlEvent{};
            if (preservedEventsChecked < pendingEventsToCheck)
            {
                sdlEvent = m_pendingEvents.front();
                m_pendingEvents.pop_front();
                ++preservedEventsChecked;
            }
            else if (freshEventsChecked >= MAX_FRESH_EVENTS_PER_POLL ||
                     SDL_PeepEvents(&sdlEvent, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) != 1)
            {
                return false;
            }
            else
            {
                ++freshEventsChecked;
            }

            if (sdlEvent.type == SDL_EVENT_QUIT)
            {
                event = {};
                event.type = PlatformEventType::Window;
                event.window.type = WindowEventType::QuitRequested;
                return true;
            }

            if (isWindowEvent(sdlEvent.type))
            {
                if (sdlEvent.window.windowID != windowId)
                {
                    preserveEvent(sdlEvent);
                    continue;
                }

                WindowEvent translatedEvent{};
                if (!translateWindowEvent(sdlEvent, translatedEvent, windowId))
                {
                    continue;
                }

                event = {};
                event.type = PlatformEventType::Window;
                event.window = translatedEvent;
                return true;
            }

            if (isInputEvent(sdlEvent.type))
            {
                if (getInputWindowId(sdlEvent) != windowId)
                {
                    preserveEvent(sdlEvent);
                    continue;
                }

                InputEvent translatedEvent{};
                if (!translateInputEvent(sdlEvent, translatedEvent, windowId))
                {
                    continue;
                }

                event = {};
                event.type = PlatformEventType::Input;
                event.input = translatedEvent;
                return true;
            }
        }
    }

    bool EventPump::pollInputEvent(InputEvent& event, SDL_WindowID windowId)
    {
        if (windowId == 0)
        {
            return false;
        }

        const std::size_t pendingEventsToCheck = m_pendingEvents.size();
        std::size_t preservedEventsChecked = 0;
        std::size_t freshEventsChecked = 0;
        while (true)
        {
            SDL_Event sdlEvent{};
            if (preservedEventsChecked < pendingEventsToCheck)
            {
                sdlEvent = m_pendingEvents.front();
                m_pendingEvents.pop_front();
                ++preservedEventsChecked;
            }
            else if (freshEventsChecked >= MAX_FRESH_EVENTS_PER_POLL ||
                     SDL_PeepEvents(&sdlEvent, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) != 1)
            {
                return false;
            }
            else
            {
                ++freshEventsChecked;
            }

            if (sdlEvent.type == SDL_EVENT_QUIT || isWindowEvent(sdlEvent.type))
            {
                preserveEvent(sdlEvent);
                continue;
            }

            if (!isInputEvent(sdlEvent.type))
            {
                continue;
            }

            if (getInputWindowId(sdlEvent) != windowId)
            {
                preserveEvent(sdlEvent);
                continue;
            }

            InputEvent translatedEvent{};
            if (!translateInputEvent(sdlEvent, translatedEvent, windowId))
            {
                continue;
            }

            event = translatedEvent;
            return true;
        }
    }

    bool EventPump::translateWindowEvent(
        const SDL_Event& sdlEvent,
        WindowEvent& event,
        SDL_WindowID windowId
    ) noexcept
    {
        if (!isWindowEvent(sdlEvent.type))
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
        case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
            event.type = WindowEventType::Exposed;
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

    bool EventPump::translateInputEvent(
        const SDL_Event& sdlEvent,
        InputEvent& event,
        SDL_WindowID windowId
    ) noexcept
    {
        if (!isInputEvent(sdlEvent.type) || getInputWindowId(sdlEvent) != windowId)
        {
            return false;
        }

        event = {};
        event.windowId = windowId;
        switch (sdlEvent.type)
        {
        case SDL_EVENT_KEY_DOWN:
            event.type = InputEventType::KeyPressed;
            event.scancode = sdlEvent.key.scancode;
            event.key = sdlEvent.key.key;
            event.isRepeat = sdlEvent.key.repeat;
            break;
        case SDL_EVENT_KEY_UP:
            event.type = InputEventType::KeyReleased;
            event.scancode = sdlEvent.key.scancode;
            event.key = sdlEvent.key.key;
            event.isRepeat = sdlEvent.key.repeat;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            event.type = InputEventType::MouseButtonPressed;
            event.mouseButton = sdlEvent.button.button;
            event.clicks = sdlEvent.button.clicks;
            event.x = sdlEvent.button.x;
            event.y = sdlEvent.button.y;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            event.type = InputEventType::MouseButtonReleased;
            event.mouseButton = sdlEvent.button.button;
            event.clicks = sdlEvent.button.clicks;
            event.x = sdlEvent.button.x;
            event.y = sdlEvent.button.y;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            event.type = InputEventType::MouseMoved;
            event.mouseButtonState = sdlEvent.motion.state;
            event.x = sdlEvent.motion.x;
            event.y = sdlEvent.motion.y;
            event.xRelative = sdlEvent.motion.xrel;
            event.yRelative = sdlEvent.motion.yrel;
            break;
        default:
            return false;
        }

        return true;
    }

    void EventPump::preserveEvent(const SDL_Event& event)
    {
        if (m_pendingEvents.size() >= MAX_PENDING_EVENTS)
        {
            m_pendingEvents.pop_front();
        }

        m_pendingEvents.push_back(event);
    }
}
