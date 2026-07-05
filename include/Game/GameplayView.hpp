// RaythmDemo - Game Gameplay View Adapter
// Declares gameplay snapshot data and conversion to minimal render commands.
// Author: RatherHard
// Date: 2026-07-05

#pragma once

#include "Game/GameConstants.hpp"
#include "Render/RenderStrategy.hpp"

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

    /**
     * @brief Pixel-space layout and colors used to convert gameplay snapshots to render commands.
     */
    struct GameplayViewLayout
    {
        /** @brief Full viewport width in pixels. */
        int viewportWidth = 1280;

        /** @brief Full viewport height in pixels. */
        int viewportHeight = 720;

        /** @brief Left margin before the first lane in pixels. */
        int playfieldLeft = 440;

        /** @brief Top margin before the playfield in pixels. */
        int playfieldTop = 48;

        /** @brief Width of the complete lane playfield in pixels. */
        int playfieldWidth = 400;

        /** @brief Height of the complete lane playfield in pixels. */
        int playfieldHeight = 624;

        /** @brief Horizontal gap between adjacent lanes in pixels. */
        int laneGap = 6;

        /** @brief Judgement line thickness in pixels. */
        int judgementLineThickness = 6;

        /** @brief Color used for idle lane backgrounds. */
        Render::Color laneColor{0.08F, 0.09F, 0.12F, 1.0F};

        /** @brief Color used for pressed lane backgrounds. */
        Render::Color pressedLaneColor{0.18F, 0.22F, 0.30F, 1.0F};

        /** @brief Color used for tap note rectangles. */
        Render::Color tapNoteColor{0.30F, 0.78F, 1.0F, 1.0F};

        /** @brief Color used for hold note rectangles. */
        Render::Color holdNoteColor{0.67F, 0.42F, 1.0F, 1.0F};

        /** @brief Color used for the judgement line. */
        Render::Color judgementLineColor{1.0F, 0.92F, 0.35F, 1.0F};
    };

    /**
     * @brief Converts gameplay view snapshots into minimal 2D render commands.
     */
    class GameplayViewAdapter
    {
    public:
        /**
         * @brief Creates a view adapter with deterministic pixel layout.
         * @param layout Viewport, playfield, spacing, and color configuration.
         * @throws std::runtime_error when layout dimensions are invalid.
         * @note The adapter owns only pure layout math and never submits commands to Renderer.
         */
        explicit GameplayViewAdapter(const GameplayViewLayout& layout = {});

        /**
         * @brief Converts a gameplay snapshot into ordered render commands.
         * @param snapshot View-facing lane, judgement line, and visible note data.
         * @return Render commands for lane backgrounds, judgement line, and visible notes.
         * @throws std::runtime_error when snapshot data is invalid.
         * @note Commands use pixel-space rectangles accepted by Render::Renderer.
         */
        [[nodiscard]] std::vector<Render::RenderCommand> buildCommands(const GameplaySnapshot& snapshot) const;

        /**
         * @brief Returns the validated layout used by this adapter.
         * @return View layout and color configuration.
         * @note Exposed for tests and future runtime inspection.
         */
        [[nodiscard]] const GameplayViewLayout& getLayout() const noexcept;

    private:
        /** @brief Validated view layout used for command generation. */
        GameplayViewLayout m_layout{};
    };
}
