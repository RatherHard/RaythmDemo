// RaythmDemo - Game Gameplay Snapshot
// Declares render-free gameplay view state shared by session and view adapter.
// Author: RatherHard
// Date: 2026-07-07

#pragma once

#include "Game/GameConstants.hpp"

#include <array>
#include <vector>

namespace Raythm::Game
{
    /**
     * @brief View-facing state for one gameplay lane.
     */
    struct LaneVisualState
    {
        /** @brief True when the lane should render in its pressed visual state. */
        bool isPressed = false;
    };

    /**
     * @brief View-facing representation of one visible tap or hold note.
     */
    struct VisibleNoteVisual
    {
        /** @brief Zero-based lane index containing this note. */
        int lane = 0;

        /** @brief Normalized note top within the playfield, where 0 is top and 1 is bottom. */
        float top = 0.0F;

        /** @brief Normalized note height within the playfield. */
        float height = 0.05F;

        /** @brief True when this note should use hold-note coloring. */
        bool isHold = false;
    };

    /**
     * @brief Read-only gameplay view snapshot consumed by the render adapter.
     */
    struct GameplaySnapshot
    {
        /** @brief Per-lane visual states ordered from lane 0 to lane 3. */
        std::array<LaneVisualState, GAME_LANE_COUNT> lanes{};

        /** @brief Notes currently relevant to the visible playfield. */
        std::vector<VisibleNoteVisual> visibleNotes;

        /** @brief Normalized y coordinate of the judgement line within the playfield. */
        float judgementLineY = 0.85F;
    };
}
