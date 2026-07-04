// RaythmDemo - Platform Input State
// Defines the frame-local keyboard and mouse state snapshot built from input events.
// Author: RatherHard
// Date: 2026-07-04

#pragma once

#include <array>
#include <cstddef>
#include <utility>

#include <SDL3/SDL.h>

#include "Platform/EventPump.hpp"

namespace Raythm::Platform
{
    /**
     * @brief Tracks held input state and frame-local input edges from engine-facing input events.
     */
    class InputState
    {
    public:
        /**
         * @brief Clears one-frame edges and mouse relative motion while preserving held state.
         * @note Call once at the start of each simulation frame before applying newly polled input events.
         */
        void beginFrame() noexcept;

        /**
         * @brief Clears all held state, one-frame edges, and mouse coordinates.
         * @note Use when focus is lost, input capture resets, or a new gameplay context starts.
         */
        void clear() noexcept;

        /**
         * @brief Applies one translated device input event to the current state snapshot.
         * @param event Engine-facing input event produced by EventPump.
         * @note Repeated key-down events keep the key held but do not create a fresh pressed edge.
         */
        void handleInputEvent(const InputEvent& event) noexcept;

        /**
         * @brief Reports whether a keyboard scancode is currently held.
         * @param scancode SDL physical key code to query.
         * @return True when the key is currently down.
         */
        bool isKeyDown(SDL_Scancode scancode) const noexcept;

        /**
         * @brief Reports whether a keyboard scancode was pressed during the current frame.
         * @param scancode SDL physical key code to query.
         * @return True when a non-repeat key press edge occurred this frame.
         */
        bool wasKeyPressed(SDL_Scancode scancode) const noexcept;

        /**
         * @brief Reports whether a keyboard scancode was released during the current frame.
         * @param scancode SDL physical key code to query.
         * @return True when a key release edge occurred this frame.
         */
        bool wasKeyReleased(SDL_Scancode scancode) const noexcept;

        /**
         * @brief Reports whether a mouse button is currently held.
         * @param button SDL mouse button index to query.
         * @return True when the mouse button is currently down.
         */
        bool isMouseButtonDown(Uint8 button) const noexcept;

        /**
         * @brief Reports whether a mouse button was pressed during the current frame.
         * @param button SDL mouse button index to query.
         * @return True when a mouse button press edge occurred this frame.
         */
        bool wasMouseButtonPressed(Uint8 button) const noexcept;

        /**
         * @brief Reports whether a mouse button was released during the current frame.
         * @param button SDL mouse button index to query.
         * @return True when a mouse button release edge occurred this frame.
         */
        bool wasMouseButtonReleased(Uint8 button) const noexcept;

        /**
         * @brief Returns the latest absolute mouse position relative to the window.
         * @return Pair of x and y coordinates.
         */
        std::pair<float, float> getMousePosition() const noexcept;

        /**
         * @brief Returns accumulated mouse relative motion for the current frame.
         * @return Pair of x and y relative movement accumulated since the last beginFrame call.
         */
        std::pair<float, float> getMouseDelta() const noexcept;

    private:
        /** @brief Number of mouse button slots tracked; SDL mouse buttons are 1-based and sparse. */
        static constexpr std::size_t MOUSE_BUTTON_CAPACITY = 8;

        /**
         * @brief Converts an SDL scancode to an array index when it is inside SDL's valid range.
         * @param scancode SDL physical key code to normalize.
         * @return Valid array index, or SDL_SCANCODE_COUNT when the scancode is invalid.
         */
        static std::size_t normalizeScancode(SDL_Scancode scancode) noexcept;

        /**
         * @brief Converts an SDL mouse button index to an array index when it is tracked.
         * @param button SDL mouse button index to normalize.
         * @return Valid array index, or MOUSE_BUTTON_CAPACITY when the button is invalid.
         */
        static std::size_t normalizeMouseButton(Uint8 button) noexcept;

        /** @brief Currently held keyboard scancodes. */
        std::array<bool, SDL_SCANCODE_COUNT> m_keysDown{};

        /** @brief Keyboard scancodes pressed during the current frame. */
        std::array<bool, SDL_SCANCODE_COUNT> m_keysPressed{};

        /** @brief Keyboard scancodes released during the current frame. */
        std::array<bool, SDL_SCANCODE_COUNT> m_keysReleased{};

        /** @brief Currently held mouse buttons. */
        std::array<bool, MOUSE_BUTTON_CAPACITY> m_mouseButtonsDown{};

        /** @brief Mouse buttons pressed during the current frame. */
        std::array<bool, MOUSE_BUTTON_CAPACITY> m_mouseButtonsPressed{};

        /** @brief Mouse buttons released during the current frame. */
        std::array<bool, MOUSE_BUTTON_CAPACITY> m_mouseButtonsReleased{};

        /** @brief Latest mouse x coordinate relative to the window. */
        float m_mouseX = 0.0F;

        /** @brief Latest mouse y coordinate relative to the window. */
        float m_mouseY = 0.0F;

        /** @brief Accumulated mouse x movement during the current frame. */
        float m_mouseDeltaX = 0.0F;

        /** @brief Accumulated mouse y movement during the current frame. */
        float m_mouseDeltaY = 0.0F;
    };
}
