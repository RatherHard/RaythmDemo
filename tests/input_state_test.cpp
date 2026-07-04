// RaythmDemo - Platform Input State Tests
// Verifies keyboard and mouse state snapshots derived from engine-facing input events.
// Author: RatherHard
// Date: 2026-07-04

#include "Platform/InputState.hpp"

#include <iostream>
#include <string>
#include <utility>

#include <SDL3/SDL.h>

namespace
{
    namespace Platform = Raythm::Platform;

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
     * @brief Creates a keyboard input event for InputState tests.
     * @param type Engine-facing key event type.
     * @param scancode Physical SDL key code.
     * @param key Virtual SDL key code.
     * @param isRepeat True when the key press is an OS repeat.
     * @return Input event carrying the supplied keyboard payload.
     */
    Platform::InputEvent makeKeyEvent(
        Platform::InputEventType type,
        SDL_Scancode scancode,
        SDL_Keycode key,
        bool isRepeat = false
    )
    {
        Platform::InputEvent event{};
        event.type = type;
        event.scancode = scancode;
        event.key = key;
        event.isRepeat = isRepeat;
        return event;
    }

    /**
     * @brief Creates a mouse button input event for InputState tests.
     * @param type Engine-facing mouse button event type.
     * @param button SDL mouse button index.
     * @param x Cursor x coordinate relative to the window.
     * @param y Cursor y coordinate relative to the window.
     * @return Input event carrying the supplied mouse button payload.
     */
    Platform::InputEvent makeMouseButtonEvent(
        Platform::InputEventType type,
        Uint8 button,
        float x,
        float y
    )
    {
        Platform::InputEvent event{};
        event.type = type;
        event.mouseButton = button;
        event.x = x;
        event.y = y;
        return event;
    }

    /**
     * @brief Creates a mouse motion input event for InputState tests.
     * @param x Cursor x coordinate relative to the window.
     * @param y Cursor y coordinate relative to the window.
     * @param xRelative Relative x movement since the previous motion event.
     * @param yRelative Relative y movement since the previous motion event.
     * @return Input event carrying the supplied mouse motion payload.
     */
    Platform::InputEvent makeMouseMotionEvent(float x, float y, float xRelative, float yRelative)
    {
        Platform::InputEvent event{};
        event.type = Platform::InputEventType::MouseMoved;
        event.x = x;
        event.y = y;
        event.xRelative = xRelative;
        event.yRelative = yRelative;
        return event;
    }

    /**
     * @brief Verifies a default state snapshot reports no held inputs, edges, or motion.
     * @return True when default state queries are empty.
     */
    bool testInputStateStartsEmpty()
    {
        Platform::InputState inputState{};
        const auto [x, y] = inputState.getMousePosition();
        const auto [xDelta, yDelta] = inputState.getMouseDelta();

        bool passed = true;
        passed &= expect(!inputState.isKeyDown(SDL_SCANCODE_A), "default key should not be down");
        passed &= expect(!inputState.wasKeyPressed(SDL_SCANCODE_A), "default key pressed edge should be false");
        passed &= expect(!inputState.wasKeyReleased(SDL_SCANCODE_A), "default key released edge should be false");
        passed &= expect(!inputState.isMouseButtonDown(SDL_BUTTON_LEFT), "default mouse button should not be down");
        passed &= expect(!inputState.wasMouseButtonPressed(SDL_BUTTON_LEFT), "default mouse pressed edge should be false");
        passed &= expect(!inputState.wasMouseButtonReleased(SDL_BUTTON_LEFT), "default mouse released edge should be false");
        passed &= expect(x == 0.0F, "default mouse x should be zero");
        passed &= expect(y == 0.0F, "default mouse y should be zero");
        passed &= expect(xDelta == 0.0F, "default mouse x delta should be zero");
        passed &= expect(yDelta == 0.0F, "default mouse y delta should be zero");

        return passed;
    }

    /**
     * @brief Verifies key down/up state and per-frame pressed/released edges.
     * @return True when keyboard state transitions match expectations.
     */
    bool testKeyboardStateAndFrameEdges()
    {
        Platform::InputState inputState{};
        bool passed = true;

        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_SPACE, SDLK_SPACE));
        passed &= expect(inputState.isKeyDown(SDL_SCANCODE_SPACE), "space should be down after key press");
        passed &= expect(inputState.wasKeyPressed(SDL_SCANCODE_SPACE), "space pressed edge should be set");
        passed &= expect(!inputState.wasKeyReleased(SDL_SCANCODE_SPACE), "space released edge should not be set");

        inputState.beginFrame();
        passed &= expect(inputState.isKeyDown(SDL_SCANCODE_SPACE), "space should stay down across frames");
        passed &= expect(!inputState.wasKeyPressed(SDL_SCANCODE_SPACE), "space pressed edge should clear next frame");

        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyReleased, SDL_SCANCODE_SPACE, SDLK_SPACE));
        passed &= expect(!inputState.isKeyDown(SDL_SCANCODE_SPACE), "space should be up after key release");
        passed &= expect(inputState.wasKeyReleased(SDL_SCANCODE_SPACE), "space released edge should be set");

        return passed;
    }

    /**
     * @brief Verifies repeated key presses do not produce fresh pressed edges while the key is held.
     * @return True when repeat events preserve held state without retriggering the press edge.
     */
    bool testKeyboardRepeatDoesNotRetriggerPressedEdge()
    {
        Platform::InputState inputState{};
        bool passed = true;

        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_A, SDLK_A));
        inputState.beginFrame();
        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_A, SDLK_A, true));

        passed &= expect(inputState.isKeyDown(SDL_SCANCODE_A), "repeat key should remain down");
        passed &= expect(!inputState.wasKeyPressed(SDL_SCANCODE_A), "repeat key should not retrigger pressed edge");

        return passed;
    }

    /**
     * @brief Verifies a press and release in one frame keeps both edges while ending released.
     * @return True when a short same-frame tap keeps both edge flags and final up state.
     */
    bool testPressAndReleaseSameKeyWithinOneFrame()
    {
        Platform::InputState inputState{};
        bool passed = true;

        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_A, SDLK_A));
        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyReleased, SDL_SCANCODE_A, SDLK_A));

        passed &= expect(!inputState.isKeyDown(SDL_SCANCODE_A), "same-frame key tap should finish up");
        passed &= expect(inputState.wasKeyPressed(SDL_SCANCODE_A), "same-frame key tap should keep pressed edge");
        passed &= expect(inputState.wasKeyReleased(SDL_SCANCODE_A), "same-frame key tap should keep released edge");

        return passed;
    }

    /**
     * @brief Verifies mouse button down/up state and per-frame pressed/released edges.
     * @return True when mouse button transitions match expectations.
     */
    bool testMouseButtonStateAndFrameEdges()
    {
        Platform::InputState inputState{};
        bool passed = true;

        inputState.handleInputEvent(makeMouseButtonEvent(
            Platform::InputEventType::MouseButtonPressed,
            SDL_BUTTON_LEFT,
            10.0F,
            20.0F
        ));
        passed &= expect(inputState.isMouseButtonDown(SDL_BUTTON_LEFT), "left button should be down after press");
        passed &= expect(inputState.wasMouseButtonPressed(SDL_BUTTON_LEFT), "left button pressed edge should be set");
        passed &= expect(!inputState.wasMouseButtonReleased(SDL_BUTTON_LEFT), "left button release edge should not be set");

        inputState.beginFrame();
        passed &= expect(inputState.isMouseButtonDown(SDL_BUTTON_LEFT), "left button should stay down across frames");
        passed &= expect(!inputState.wasMouseButtonPressed(SDL_BUTTON_LEFT), "left button pressed edge should clear");

        inputState.handleInputEvent(makeMouseButtonEvent(
            Platform::InputEventType::MouseButtonReleased,
            SDL_BUTTON_LEFT,
            15.0F,
            25.0F
        ));
        passed &= expect(!inputState.isMouseButtonDown(SDL_BUTTON_LEFT), "left button should be up after release");
        passed &= expect(inputState.wasMouseButtonReleased(SDL_BUTTON_LEFT), "left button released edge should be set");

        return passed;
    }

    /**
     * @brief Verifies mouse position and accumulated per-frame relative motion.
     * @return True when mouse coordinates and delta reset semantics match expectations.
     */
    bool testMousePositionAndDelta()
    {
        Platform::InputState inputState{};
        bool passed = true;

        inputState.handleInputEvent(makeMouseMotionEvent(12.0F, 18.0F, 2.5F, -1.0F));
        inputState.handleInputEvent(makeMouseMotionEvent(15.0F, 25.0F, 3.0F, 4.5F));

        const auto [x, y] = inputState.getMousePosition();
        const auto [xDelta, yDelta] = inputState.getMouseDelta();
        passed &= expect(x == 15.0F, "mouse x should track latest absolute position");
        passed &= expect(y == 25.0F, "mouse y should track latest absolute position");
        passed &= expect(xDelta == 5.5F, "mouse x delta should accumulate within the frame");
        passed &= expect(yDelta == 3.5F, "mouse y delta should accumulate within the frame");

        inputState.beginFrame();
        const auto [nextX, nextY] = inputState.getMousePosition();
        const auto [nextXDelta, nextYDelta] = inputState.getMouseDelta();
        passed &= expect(nextX == 15.0F, "mouse x should persist across frames");
        passed &= expect(nextY == 25.0F, "mouse y should persist across frames");
        passed &= expect(nextXDelta == 0.0F, "mouse x delta should clear next frame");
        passed &= expect(nextYDelta == 0.0F, "mouse y delta should clear next frame");

        return passed;
    }

    /**
     * @brief Verifies clear removes all held state and frame edges.
     * @return True when reset state is empty.
     */
    bool testClearResetsInputState()
    {
        Platform::InputState inputState{};
        bool passed = true;

        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_SPACE, SDLK_SPACE));
        inputState.handleInputEvent(makeMouseButtonEvent(
            Platform::InputEventType::MouseButtonPressed,
            SDL_BUTTON_LEFT,
            1.0F,
            2.0F
        ));
        inputState.handleInputEvent(makeMouseMotionEvent(3.0F, 4.0F, 5.0F, 6.0F));
        inputState.clear();

        const auto [x, y] = inputState.getMousePosition();
        const auto [xDelta, yDelta] = inputState.getMouseDelta();
        passed &= expect(!inputState.isKeyDown(SDL_SCANCODE_SPACE), "clear should release held key");
        passed &= expect(!inputState.wasKeyPressed(SDL_SCANCODE_SPACE), "clear should remove key pressed edge");
        passed &= expect(!inputState.isMouseButtonDown(SDL_BUTTON_LEFT), "clear should release held mouse button");
        passed &= expect(!inputState.wasMouseButtonPressed(SDL_BUTTON_LEFT), "clear should remove mouse pressed edge");
        passed &= expect(x == 0.0F, "clear should reset mouse x");
        passed &= expect(y == 0.0F, "clear should reset mouse y");
        passed &= expect(xDelta == 0.0F, "clear should reset mouse x delta");
        passed &= expect(yDelta == 0.0F, "clear should reset mouse y delta");

        return passed;
    }
}

/**
 * @brief Runs all Platform input state tests.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Zero when all input state checks pass; nonzero when any check fails.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    bool allTestsPassed = true;
    allTestsPassed &= testInputStateStartsEmpty();
    allTestsPassed &= testKeyboardStateAndFrameEdges();
    allTestsPassed &= testKeyboardRepeatDoesNotRetriggerPressedEdge();
    allTestsPassed &= testPressAndReleaseSameKeyWithinOneFrame();
    allTestsPassed &= testMouseButtonStateAndFrameEdges();
    allTestsPassed &= testMousePositionAndDelta();
    allTestsPassed &= testClearResetsInputState();

    return allTestsPassed ? 0 : 1;
}
