// RaythmDemo - Game Lane Input Mapping Implementation
// Implements semantic lane input snapshots from platform input state.
// Author: RatherHard
// Date: 2026-07-05

#include "Game/LaneInput.hpp"

#include <stdexcept>
#include <string>

namespace Raythm::Game
{
    namespace
    {
        /**
         * @brief Validates a physical scancode for lane binding.
         * @param scancode SDL scancode to validate.
         * @return True when the scancode can be tracked by Platform::InputState.
         * @note SDL_SCANCODE_UNKNOWN is rejected so lanes cannot bind to a non-key.
         */
        bool isValidLaneScancode(SDL_Scancode scancode) noexcept
        {
            const int scancodeValue = static_cast<int>(scancode);
            return scancode != SDL_SCANCODE_UNKNOWN && scancodeValue > 0 && scancodeValue < SDL_SCANCODE_COUNT;
        }

        /**
         * @brief Validates lane key map uniqueness and scancode range.
         * @param keyMap Key map to validate.
         * @throws std::runtime_error when a lane uses an unknown or duplicate scancode.
         * @note Validation is kept outside sample() so per-frame mapping is noexcept.
         */
        void validateKeyMap(const LaneKeyMap& keyMap)
        {
            for (std::size_t lane = 0; lane < keyMap.laneScancodes.size(); ++lane)
            {
                if (!isValidLaneScancode(keyMap.laneScancodes[lane]))
                {
                    throw std::runtime_error("Invalid lane key map: lane " + std::to_string(lane) + " uses an unknown scancode");
                }

                for (std::size_t previousLane = 0; previousLane < lane; ++previousLane)
                {
                    if (keyMap.laneScancodes[previousLane] == keyMap.laneScancodes[lane])
                    {
                        throw std::runtime_error("Invalid lane key map: duplicate scancode assigned to multiple lanes");
                    }
                }
            }
        }
    }

    LaneKeyMap LaneKeyMap::createDefault4K() noexcept
    {
        return {{{SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_J, SDL_SCANCODE_K}}};
    }

    LaneInputMapper::LaneInputMapper(const LaneKeyMap& keyMap)
        : m_keyMap(keyMap)
    {
        validateKeyMap(m_keyMap);
    }

    LaneInputSnapshot LaneInputMapper::sample(const Platform::InputState& inputState) const noexcept
    {
        LaneInputSnapshot snapshot{};
        for (std::size_t lane = 0; lane < m_keyMap.laneScancodes.size(); ++lane)
        {
            const SDL_Scancode scancode = m_keyMap.laneScancodes[lane];
            snapshot.lanes[lane].isDown = inputState.isKeyDown(scancode);
            snapshot.lanes[lane].wasPressed = inputState.wasKeyPressed(scancode);
            snapshot.lanes[lane].wasReleased = inputState.wasKeyReleased(scancode);
        }

        return snapshot;
    }

    const LaneKeyMap& LaneInputMapper::getKeyMap() const noexcept
    {
        return m_keyMap;
    }
}
