// RaythmDemo - SDL Event Pump
// Defines the thin SDL event pump used to translate platform window events.
// Author: RatherHard
// Date: 2026-07-04

#pragma once

#include <SDL3/SDL.h>

#include "Platform/Window.hpp"

namespace Raythm::Platform
{
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
         * @note Events that do not translate to this window's lifecycle are consumed so later events are not starved.
         */
        bool pollWindowEvent(WindowEvent& event, SDL_WindowID windowId);

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
    };
}
