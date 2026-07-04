// RaythmDemo - Platform Input State Implementation
// Implements keyboard and mouse state snapshots from engine-facing input events.
// Author: RatherHard
// Date: 2026-07-04

#include "Platform/InputState.hpp"

#include <algorithm>

namespace Raythm::Platform
{
    void InputState::beginFrame() noexcept
    {
        std::fill(m_keysPressed.begin(), m_keysPressed.end(), false);
        std::fill(m_keysReleased.begin(), m_keysReleased.end(), false);
        std::fill(m_mouseButtonsPressed.begin(), m_mouseButtonsPressed.end(), false);
        std::fill(m_mouseButtonsReleased.begin(), m_mouseButtonsReleased.end(), false);
        m_mouseDeltaX = 0.0F;
        m_mouseDeltaY = 0.0F;
    }

    void InputState::clear() noexcept
    {
        std::fill(m_keysDown.begin(), m_keysDown.end(), false);
        std::fill(m_mouseButtonsDown.begin(), m_mouseButtonsDown.end(), false);
        beginFrame();
        m_mouseX = 0.0F;
        m_mouseY = 0.0F;
    }

    void InputState::handleInputEvent(const InputEvent& event) noexcept
    {
        switch (event.type)
        {
        case InputEventType::KeyPressed:
        {
            const std::size_t keyIndex = normalizeScancode(event.scancode);
            if (keyIndex == SDL_SCANCODE_COUNT)
            {
                return;
            }

            const bool wasAlreadyDown = m_keysDown[keyIndex];
            m_keysDown[keyIndex] = true;
            if (!event.isRepeat && !wasAlreadyDown)
            {
                m_keysPressed[keyIndex] = true;
            }
            break;
        }
        case InputEventType::KeyReleased:
        {
            const std::size_t keyIndex = normalizeScancode(event.scancode);
            if (keyIndex == SDL_SCANCODE_COUNT)
            {
                return;
            }

            m_keysDown[keyIndex] = false;
            m_keysReleased[keyIndex] = true;
            break;
        }
        case InputEventType::MouseButtonPressed:
        {
            const std::size_t buttonIndex = normalizeMouseButton(event.mouseButton);
            if (buttonIndex == MOUSE_BUTTON_CAPACITY)
            {
                return;
            }

            const bool wasAlreadyDown = m_mouseButtonsDown[buttonIndex];
            m_mouseButtonsDown[buttonIndex] = true;
            if (!wasAlreadyDown)
            {
                m_mouseButtonsPressed[buttonIndex] = true;
            }
            m_mouseX = event.x;
            m_mouseY = event.y;
            break;
        }
        case InputEventType::MouseButtonReleased:
        {
            const std::size_t buttonIndex = normalizeMouseButton(event.mouseButton);
            if (buttonIndex == MOUSE_BUTTON_CAPACITY)
            {
                return;
            }

            m_mouseButtonsDown[buttonIndex] = false;
            m_mouseButtonsReleased[buttonIndex] = true;
            m_mouseX = event.x;
            m_mouseY = event.y;
            break;
        }
        case InputEventType::MouseMoved:
            m_mouseX = event.x;
            m_mouseY = event.y;
            m_mouseDeltaX += event.xRelative;
            m_mouseDeltaY += event.yRelative;
            break;
        case InputEventType::None:
        default:
            break;
        }
    }

    bool InputState::isKeyDown(SDL_Scancode scancode) const noexcept
    {
        const std::size_t keyIndex = normalizeScancode(scancode);
        return keyIndex != SDL_SCANCODE_COUNT && m_keysDown[keyIndex];
    }

    bool InputState::wasKeyPressed(SDL_Scancode scancode) const noexcept
    {
        const std::size_t keyIndex = normalizeScancode(scancode);
        return keyIndex != SDL_SCANCODE_COUNT && m_keysPressed[keyIndex];
    }

    bool InputState::wasKeyReleased(SDL_Scancode scancode) const noexcept
    {
        const std::size_t keyIndex = normalizeScancode(scancode);
        return keyIndex != SDL_SCANCODE_COUNT && m_keysReleased[keyIndex];
    }

    bool InputState::isMouseButtonDown(Uint8 button) const noexcept
    {
        const std::size_t buttonIndex = normalizeMouseButton(button);
        return buttonIndex != MOUSE_BUTTON_CAPACITY && m_mouseButtonsDown[buttonIndex];
    }

    bool InputState::wasMouseButtonPressed(Uint8 button) const noexcept
    {
        const std::size_t buttonIndex = normalizeMouseButton(button);
        return buttonIndex != MOUSE_BUTTON_CAPACITY && m_mouseButtonsPressed[buttonIndex];
    }

    bool InputState::wasMouseButtonReleased(Uint8 button) const noexcept
    {
        const std::size_t buttonIndex = normalizeMouseButton(button);
        return buttonIndex != MOUSE_BUTTON_CAPACITY && m_mouseButtonsReleased[buttonIndex];
    }

    std::pair<float, float> InputState::getMousePosition() const noexcept
    {
        return {m_mouseX, m_mouseY};
    }

    std::pair<float, float> InputState::getMouseDelta() const noexcept
    {
        return {m_mouseDeltaX, m_mouseDeltaY};
    }

    std::size_t InputState::normalizeScancode(SDL_Scancode scancode) noexcept
    {
        const int scancodeValue = static_cast<int>(scancode);
        if (scancodeValue < 0 || scancodeValue >= SDL_SCANCODE_COUNT)
        {
            return SDL_SCANCODE_COUNT;
        }

        return static_cast<std::size_t>(scancodeValue);
    }

    std::size_t InputState::normalizeMouseButton(Uint8 button) noexcept
    {
        if (button == 0 || button > MOUSE_BUTTON_CAPACITY)
        {
            return MOUSE_BUTTON_CAPACITY;
        }

        return static_cast<std::size_t>(button - 1);
    }
}
