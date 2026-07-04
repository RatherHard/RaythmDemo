// RaythmDemo - SDL Event Pump Implementation
// Implements SDL global queue polling and window event translation.
// Author: RatherHard
// Date: 2026-07-04

#include "Platform/EventPump.hpp"

namespace Raythm::Platform
{
    bool EventPump::pollWindowEvent(WindowEvent& event, SDL_WindowID windowId)
    {
        if (windowId == 0)
        {
            return false;
        }

        SDL_Event sdlEvent{};
        while (SDL_PeepEvents(&sdlEvent, 1, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) == 1)
        {
            if (sdlEvent.type == SDL_EVENT_QUIT)
            {
                SDL_PeepEvents(&sdlEvent, 1, SDL_GETEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT);
                event = {};
                event.type = WindowEventType::QuitRequested;
                return true;
            }

            if (sdlEvent.type < SDL_EVENT_WINDOW_FIRST || sdlEvent.type > SDL_EVENT_WINDOW_LAST)
            {
                SDL_PeepEvents(&sdlEvent, 1, SDL_GETEVENT, sdlEvent.type, sdlEvent.type);
                continue;
            }

            if (sdlEvent.window.windowID != windowId)
            {
                SDL_PeepEvents(&sdlEvent, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST);
                continue;
            }

            SDL_PeepEvents(&sdlEvent, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST);

            WindowEvent translatedEvent{};
            if (!translateWindowEvent(sdlEvent, translatedEvent, windowId))
            {
                continue;
            }

            event = translatedEvent;
            return true;
        }

        return false;
    }

    bool EventPump::translateWindowEvent(
        const SDL_Event& sdlEvent,
        WindowEvent& event,
        SDL_WindowID windowId
    ) noexcept
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
}
