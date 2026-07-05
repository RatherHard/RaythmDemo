// RaythmDemo - Game Chart Model
// Defines runtime chart data produced by chart loading.
// Author: RatherHard
// Date: 2026-07-05

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Game/GameConstants.hpp"

namespace Raythm::Game
{
    /**
     * @brief Canonical rational beat position used for sorting and equality comparisons.
     */
    struct BeatPosition
    {
        /** @brief Reduced rational numerator measured in whole-beat units. */
        std::int64_t numerator = 0;

        /** @brief Positive reduced rational denominator measured in whole-beat units. */
        std::int64_t denominator = 1;

        /**
         * @brief Builds a normalized beat position from a mixed fraction.
         * @param whole Whole beat count from the chart beat tuple.
         * @param fractionNumerator Fraction numerator from the chart beat tuple.
         * @param fractionDenominator Fraction denominator from the chart beat tuple.
         * @return Canonical beat position with a positive reduced denominator.
         * @throws std::runtime_error when the beat tuple violates chart format constraints.
         * @note Uses 64-bit integer arithmetic to avoid precision loss while comparing beat positions.
         */
        [[nodiscard]] static BeatPosition fromTuple(
            std::int64_t whole,
            std::int64_t fractionNumerator,
            std::int64_t fractionDenominator);
    };

    /**
     * @brief Compares two beat positions for normalized equality.
     * @param lhs First beat position.
     * @param rhs Second beat position.
     * @return True when both positions represent the same musical beat.
     * @note Assumes both beat positions are already canonicalized by BeatPosition::fromTuple.
     */
    [[nodiscard]] bool operator==(const BeatPosition& lhs, const BeatPosition& rhs) noexcept;

    /**
     * @brief Orders two beat positions by musical time.
     * @param lhs First beat position.
     * @param rhs Second beat position.
     * @return True when lhs occurs before rhs.
     * @note Cross-multiplies with 64-bit values; expected chart tuple ranges should remain modest.
     */
    [[nodiscard]] bool operator<(const BeatPosition& lhs, const BeatPosition& rhs) noexcept;

    /**
     * @brief Chart metadata needed by the minimal gameplay slice.
     */
    struct ChartMetadata
    {
        /** @brief Display title of the charted song. */
        std::string title;

        /** @brief Display artist of the charted song. */
        std::string artist;

        /** @brief Chart author or creator name. */
        std::string creator;

        /** @brief Relative audio path declared by the chart metadata. */
        std::string audioPath;

        /** @brief Chart-defined audio offset in milliseconds. */
        int offsetMilliseconds = 0;
    };

    /**
     * @brief One BPM timing point in chart beat space.
     */
    struct TimingPoint
    {
        /** @brief Beat where this BPM becomes active. */
        BeatPosition beat{};

        /** @brief Beats per minute active from this timing point onward. */
        double bpm = 120.0;
    };

    /**
     * @brief One tap or hold note in chart beat space.
     */
    struct Note
    {
        /** @brief Beat where the note starts. */
        BeatPosition beat{};

        /** @brief Optional beat where a hold note ends. */
        std::optional<BeatPosition> endBeat;

        /** @brief Zero-based lane column containing the note. */
        int column = 0;

        /**
         * @brief Reports whether this note has an end beat and should be treated as a hold note.
         * @return True when endBeat is present.
         * @note Hold judgement is intentionally out of scope for this minimal slice.
         */
        [[nodiscard]] bool isHold() const noexcept;
    };

    /**
     * @brief Runtime chart data loaded from the documented JSON chart format.
     */
    struct Chart
    {
        /** @brief Metadata block copied from chart JSON. */
        ChartMetadata meta{};

        /** @brief Sorted BPM timing points. */
        std::vector<TimingPoint> timingPoints;

        /** @brief Sorted tap and hold notes. */
        std::vector<Note> notes;

        /** @brief Number of playable lanes accepted by this chart. */
        int laneCount = static_cast<int>(GAME_LANE_COUNT);
    };
}
