// RaythmDemo - Game Gameplay View Adapter Implementation
// Implements conversion from gameplay view snapshots to render commands.
// Author: RatherHard
// Date: 2026-07-05

#include "Game/GameplayView.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace Raythm::Game
{
    namespace
    {
        /** @brief Largest supported viewport or playfield dimension for safe integer geometry. */
        constexpr int MAX_VIEW_DIMENSION = 16'384;

        /**
         * @brief Computes integer lane width from a validated layout.
         * @param layout Validated view layout.
         * @return Width of each lane in pixels.
         * @note Any remainder stays unused on the right edge for deterministic integer geometry.
         */
        int computeLaneWidth(const GameplayViewLayout& layout) noexcept
        {
            const std::int64_t totalGapWidth = static_cast<std::int64_t>(layout.laneGap) *
                static_cast<std::int64_t>(GAME_LANE_COUNT - 1U);
            return static_cast<int>((static_cast<std::int64_t>(layout.playfieldWidth) - totalGapWidth) /
                static_cast<std::int64_t>(GAME_LANE_COUNT));
        }

        /**
         * @brief Computes the x coordinate for a lane.
         * @param layout Validated view layout.
         * @param lane Zero-based lane index.
         * @param laneWidth Lane width in pixels.
         * @return Pixel x coordinate for the lane.
         * @note Uses deterministic integer spacing for stable tests and rendering.
         */
        int computeLaneX(const GameplayViewLayout& layout, int lane, int laneWidth) noexcept
        {
            return layout.playfieldLeft + lane * (laneWidth + layout.laneGap);
        }

        /**
         * @brief Tests whether a floating point value is finite.
         * @param value Value to inspect.
         * @param context Context used in error messages.
         * @throws std::runtime_error when value is NaN or infinity.
         * @note Prevents invalid geometry from reaching renderer command generation.
         */
        void requireFinite(float value, const std::string& context)
        {
            if (!std::isfinite(value))
            {
                throw std::runtime_error("Invalid gameplay view snapshot: " + context + " must be finite");
            }
        }

        /**
         * @brief Validates layout dimensions and spacing.
         * @param layout Layout to validate.
         * @throws std::runtime_error when dimensions cannot produce visible lanes.
         * @note Color values are not clamped by this adapter.
         */
        void validateLayout(const GameplayViewLayout& layout)
        {
            if (layout.viewportWidth <= 0 || layout.viewportHeight <= 0)
            {
                throw std::runtime_error("Invalid gameplay view layout: viewport dimensions must be positive");
            }
            if (layout.viewportWidth > MAX_VIEW_DIMENSION || layout.viewportHeight > MAX_VIEW_DIMENSION)
            {
                throw std::runtime_error("Invalid gameplay view layout: viewport dimensions exceed the supported range");
            }
            if (layout.playfieldLeft < 0 || layout.playfieldTop < 0)
            {
                throw std::runtime_error("Invalid gameplay view layout: playfield origin must be non-negative");
            }
            if (layout.playfieldLeft > MAX_VIEW_DIMENSION || layout.playfieldTop > MAX_VIEW_DIMENSION)
            {
                throw std::runtime_error("Invalid gameplay view layout: playfield origin exceeds the supported range");
            }
            if (layout.playfieldWidth <= 0 || layout.playfieldHeight <= 0)
            {
                throw std::runtime_error("Invalid gameplay view layout: playfield dimensions must be positive");
            }
            if (layout.playfieldWidth > MAX_VIEW_DIMENSION || layout.playfieldHeight > MAX_VIEW_DIMENSION)
            {
                throw std::runtime_error("Invalid gameplay view layout: playfield dimensions exceed the supported range");
            }
            if (layout.laneGap < 0)
            {
                throw std::runtime_error("Invalid gameplay view layout: lane gap must be non-negative");
            }
            if (layout.laneGap > MAX_VIEW_DIMENSION)
            {
                throw std::runtime_error("Invalid gameplay view layout: lane gap exceeds the supported range");
            }
            if (layout.judgementLineThickness <= 0)
            {
                throw std::runtime_error("Invalid gameplay view layout: judgement line thickness must be positive");
            }
            if (layout.judgementLineThickness > MAX_VIEW_DIMENSION)
            {
                throw std::runtime_error("Invalid gameplay view layout: judgement line thickness exceeds the supported range");
            }
            if (static_cast<std::int64_t>(layout.playfieldLeft) + layout.playfieldWidth > layout.viewportWidth ||
                static_cast<std::int64_t>(layout.playfieldTop) + layout.playfieldHeight > layout.viewportHeight)
            {
                throw std::runtime_error("Invalid gameplay view layout: playfield must fit inside viewport");
            }
            if (computeLaneWidth(layout) <= 0)
            {
                throw std::runtime_error("Invalid gameplay view layout: playfield is too narrow for four lanes");
            }
        }

        /**
         * @brief Converts a normalized y coordinate into a safe playfield pixel coordinate.
         * @param layout Validated view layout.
         * @param normalizedY Normalized y coordinate to convert.
         * @param context Context used in error messages.
         * @return Pixel y coordinate inside or near the playfield.
         * @throws std::runtime_error when conversion would exceed int range.
         * @note Allows offscreen values for note clipping while bounding representable output.
         */
        int toPlayfieldPixelY(const GameplayViewLayout& layout, float normalizedY, const std::string& context)
        {
            requireFinite(normalizedY, context);
            const double scaled = static_cast<double>(normalizedY) * static_cast<double>(layout.playfieldHeight);
            if (scaled < static_cast<double>(std::numeric_limits<int>::min() + layout.playfieldTop) ||
                scaled > static_cast<double>(std::numeric_limits<int>::max() - layout.playfieldTop))
            {
                throw std::runtime_error("Invalid gameplay view snapshot: " + context + " is outside the supported pixel range");
            }

            return layout.playfieldTop + static_cast<int>(scaled);
        }

        /**
         * @brief Converts a normalized note height into a safe positive pixel height.
         * @param layout Validated view layout.
         * @param normalizedHeight Normalized note height to convert.
         * @param context Context used in error messages.
         * @return Pixel height rounded up to preserve thin notes.
         * @throws std::runtime_error when conversion would exceed int range.
         * @note Height zero is handled by callers before conversion.
         */
        int toPlayfieldPixelHeight(const GameplayViewLayout& layout, float normalizedHeight, const std::string& context)
        {
            requireFinite(normalizedHeight, context);
            constexpr double INTEGER_EPSILON = 0.0001;
            const double pixelHeight = std::ceil(
                static_cast<double>(normalizedHeight) * static_cast<double>(layout.playfieldHeight) - INTEGER_EPSILON);
            if (pixelHeight < 0.0 || pixelHeight > static_cast<double>(std::numeric_limits<int>::max()))
            {
                throw std::runtime_error("Invalid gameplay view snapshot: " + context + " is outside the supported pixel range");
            }

            return static_cast<int>(pixelHeight);
        }

        /**
         * @brief Adds two integers with range validation.
         * @param lhs Left operand.
         * @param rhs Right operand.
         * @param context Context used in error messages.
         * @return Sum when representable as int.
         * @throws std::runtime_error when the sum would overflow int.
         * @note Keeps render rectangle arithmetic within defined behavior.
         */
        int checkedAdd(int lhs, int rhs, const std::string& context)
        {
            const std::int64_t result = static_cast<std::int64_t>(lhs) + static_cast<std::int64_t>(rhs);
            if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
            {
                throw std::runtime_error("Invalid gameplay view geometry: " + context + " is outside the supported range");
            }

            return static_cast<int>(result);
        }

        /**
         * @brief Subtracts two integers with range validation.
         * @param lhs Left operand.
         * @param rhs Right operand.
         * @param context Context used in error messages.
         * @return Difference when representable as int.
         * @throws std::runtime_error when the difference would overflow int.
         * @note Keeps render rectangle arithmetic within defined behavior.
         */
        int checkedSubtract(int lhs, int rhs, const std::string& context)
        {
            const std::int64_t result = static_cast<std::int64_t>(lhs) - static_cast<std::int64_t>(rhs);
            if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
            {
                throw std::runtime_error("Invalid gameplay view geometry: " + context + " is outside the supported range");
            }

            return static_cast<int>(result);
        }

        /**
         * @brief Clips a note rectangle to the vertical playfield range.
         * @param layout Validated view layout.
         * @param rect Note rectangle before clipping.
         * @return True when visible pixels remain after clipping.
         * @note Horizontal clipping is unnecessary because lane geometry is layout-derived.
         */
        bool clipNoteToPlayfield(const GameplayViewLayout& layout, Render::Rect2D& rect)
        {
            const int playfieldBottom = checkedAdd(layout.playfieldTop, layout.playfieldHeight, "playfield bottom");
            const int rectBottom = checkedAdd(rect.y, rect.height, "note bottom");
            const int clippedTop = std::max(rect.y, layout.playfieldTop);
            const int clippedBottom = std::min(rectBottom, playfieldBottom);
            if (clippedBottom <= clippedTop)
            {
                return false;
            }

            rect.y = clippedTop;
            rect.height = clippedBottom - clippedTop;
            return true;
        }
    }

    GameplayViewAdapter::GameplayViewAdapter(const GameplayViewLayout& layout)
        : m_layout(layout)
    {
        validateLayout(m_layout);
    }

    std::vector<Render::RenderCommand> GameplayViewAdapter::buildCommands(const GameplaySnapshot& snapshot) const
    {
        requireFinite(snapshot.judgementLineY, "judgementLineY");

        const int laneWidth = computeLaneWidth(m_layout);
        std::vector<Render::RenderCommand> commands;
        commands.reserve(GAME_LANE_COUNT + 1U + snapshot.visibleNotes.size());

        for (std::size_t lane = 0; lane < GAME_LANE_COUNT; ++lane)
        {
            commands.push_back({
                {computeLaneX(m_layout, static_cast<int>(lane), laneWidth), m_layout.playfieldTop, laneWidth, m_layout.playfieldHeight},
                snapshot.lanes[lane].isPressed ? m_layout.pressedLaneColor : m_layout.laneColor
            });
        }

        const int judgementCenterY = toPlayfieldPixelY(m_layout, snapshot.judgementLineY, "judgementLineY");
        commands.push_back({
            {
                m_layout.playfieldLeft,
                checkedSubtract(judgementCenterY, m_layout.judgementLineThickness / 2, "judgement line y"),
                m_layout.playfieldWidth,
                m_layout.judgementLineThickness
            },
            m_layout.judgementLineColor
        });

        for (std::size_t index = 0; index < snapshot.visibleNotes.size(); ++index)
        {
            const VisibleNoteVisual& note = snapshot.visibleNotes[index];
            if (note.lane < 0 || note.lane >= static_cast<int>(GAME_LANE_COUNT))
            {
                throw std::runtime_error("Invalid gameplay view snapshot: visibleNotes[" + std::to_string(index) + "].lane is out of range");
            }
            requireFinite(note.top, "visibleNotes[" + std::to_string(index) + "].top");
            requireFinite(note.height, "visibleNotes[" + std::to_string(index) + "].height");
            if (note.height < 0.0F)
            {
                throw std::runtime_error("Invalid gameplay view snapshot: visibleNotes[" + std::to_string(index) + "].height must be non-negative");
            }
            if (note.height == 0.0F)
            {
                continue;
            }

            const int noteTop = toPlayfieldPixelY(m_layout, note.top, "visibleNotes[" + std::to_string(index) + "].top");
            const int noteHeight = toPlayfieldPixelHeight(m_layout, note.height, "visibleNotes[" + std::to_string(index) + "].height");
            Render::Rect2D bounds{computeLaneX(m_layout, note.lane, laneWidth), noteTop, laneWidth, noteHeight};
            if (!clipNoteToPlayfield(m_layout, bounds))
            {
                continue;
            }

            commands.push_back({bounds, note.isHold ? m_layout.holdNoteColor : m_layout.tapNoteColor});
        }

        return commands;
    }

    const GameplayViewLayout& GameplayViewAdapter::getLayout() const noexcept
    {
        return m_layout;
    }
}
