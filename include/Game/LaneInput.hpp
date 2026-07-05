// RaythmDemo - Game Lane Input Mapping
// Declares semantic lane input snapshots built from platform input state.
// Author: RatherHard
// Date: 2026-07-05

#pragma once

#include "Game/GameConstants.hpp"
#include "Platform/InputState.hpp"

#include <array>

#include <SDL3/SDL.h>

namespace Raythm::Game
{
    /**
     * @brief Frame-local input state for one gameplay lane.
     */
    struct LaneInputState
    {
        /** @brief True while the lane input is currently held. */
        bool isDown = false;

        /** @brief True when the lane input was pressed during this frame. */
        bool wasPressed = false;

        /** @brief True when the lane input was released during this frame. */
        bool wasReleased = false;
    };

    /**
     * @brief Semantic 4K lane input snapshot consumed by Game logic.
     */
    struct LaneInputSnapshot
    {
        /** @brief Per-lane frame state ordered from lane 0 to lane 3. */
        std::array<LaneInputState, GAME_LANE_COUNT> lanes{};
    };

    /**
     * @brief Physical keyboard bindings for the initial 4K lane layout.
     */
    struct LaneKeyMap
    {
        /** @brief SDL physical scancode bound to each lane. */
        std::array<SDL_Scancode, GAME_LANE_COUNT> laneScancodes{};

        /**
         * @brief Creates the documented default 4K key map.
         * @return Default D/F/J/K scancode mapping.
         * @note Uses physical SDL scancodes so gameplay is stable across keyboard layouts.
         */
        [[nodiscard]] static LaneKeyMap createDefault4K() noexcept;
    };

    /**
     * @brief Converts platform input state into semantic lane input state.
     */
    class LaneInputMapper
    {
    public:
        /**
         * @brief Creates a mapper from a physical lane key map.
         * @param keyMap Physical scancode bindings for all lanes.
         * @throws std::runtime_error when bindings contain unknown or duplicate scancodes.
         * @note Validation happens once during construction so sampling stays noexcept.
         */
        explicit LaneInputMapper(const LaneKeyMap& keyMap = LaneKeyMap::createDefault4K());

        /**
         * @brief Samples semantic lane input from a platform input state snapshot.
         * @param inputState Platform keyboard and mouse state for the current frame.
         * @return Game-facing per-lane held, pressed, and released state.
         * @note This method does not allocate and should be called once per frame before judgement.
         */
        [[nodiscard]] LaneInputSnapshot sample(const Platform::InputState& inputState) const noexcept;

        /**
         * @brief Returns the validated key map used by this mapper.
         * @return Physical scancode bindings for all lanes.
         * @note Exposed for tests and future configuration inspection.
         */
        [[nodiscard]] const LaneKeyMap& getKeyMap() const noexcept;

    private:
        /** @brief Validated physical lane key map. */
        LaneKeyMap m_keyMap{};
    };
}
