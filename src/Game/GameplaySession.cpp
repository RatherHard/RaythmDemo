// RaythmDemo - Game Gameplay Session Implementation
// Implements pure tap-note judgement and gameplay snapshot generation.
// Author: RatherHard
// Date: 2026-07-07

#include "Game/GameplaySession.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace Raythm::Game
{
    namespace
    {
        /** @brief Canonical zero beat position used for chart timing validation. */
        constexpr BeatPosition ZERO_BEAT{};

        /** @brief Minimum normalized note top retained in the generated visible-note snapshot. */
        constexpr float MIN_VISIBLE_TOP = -1.0F;

        /** @brief Maximum normalized note top retained in the generated visible-note snapshot. */
        constexpr float MAX_VISIBLE_TOP = 1.0F;

        /**
         * @brief Builds a consistent runtime validation error.
         * @param message Specific validation failure.
         * @return Runtime error with gameplay session context.
         * @note Keeps constructor validation failures distinguishable from chart loader failures.
         */
        std::runtime_error sessionError(const std::string& message)
        {
            return std::runtime_error("Invalid gameplay session: " + message);
        }

        /**
         * @brief Validates a normalized beat position supplied directly to the session.
         * @param beat Beat position to inspect.
         * @param context Error context for diagnostics.
         * @throws std::runtime_error when the beat cannot be converted safely.
         * @note ChartLoader creates canonical beats, but direct tests and future tools may construct Chart manually.
         */
        void validateBeatPosition(const BeatPosition& beat, const std::string& context)
        {
            if (beat.denominator <= 0)
            {
                throw sessionError(context + " denominator must be greater than zero");
            }
            if (beat.numerator < 0)
            {
                throw sessionError(context + " numerator must be non-negative");
            }
        }

        /**
         * @brief Converts a normalized beat position to a long double beat count.
         * @param beat Beat position to convert.
         * @return Beat count in quarter-note beat units.
         * @note Used only after session validation confirms the denominator is positive.
         */
        long double toBeatCount(const BeatPosition& beat) noexcept
        {
            return static_cast<long double>(beat.numerator) / static_cast<long double>(beat.denominator);
        }

        /**
         * @brief Converts floating point seconds to rounded microseconds.
         * @param seconds Duration in seconds.
         * @return Rounded duration in microseconds.
         * @throws std::runtime_error when the value is not finite or representable.
         * @note Conversion happens during session construction, not in the per-frame update loop.
         */
        std::chrono::microseconds secondsToMicroseconds(long double seconds)
        {
            if (!std::isfinite(static_cast<double>(seconds)) || seconds < 0.0L)
            {
                throw sessionError("resolved note time must be finite and non-negative");
            }

            constexpr long double MICROSECONDS_PER_SECOND = 1'000'000.0L;
            const long double microseconds = std::round(seconds * MICROSECONDS_PER_SECOND);
            if (microseconds > static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
            {
                throw sessionError("resolved note time exceeds the supported duration range");
            }

            return std::chrono::microseconds{static_cast<std::int64_t>(microseconds)};
        }

        /**
         * @brief Validates tap judgement and visual configuration.
         * @param config Gameplay session configuration to inspect.
         * @throws std::runtime_error when windows or visual values are invalid.
         * @note Enforces monotonic judgement windows so classification remains deterministic.
         */
        void validateConfig(const GameplaySessionConfig& config)
        {
            if (config.criticalPerfectWindow.count() < 0 ||
                config.normalPerfectWindow.count() < 0 ||
                config.greatWindow.count() < 0 ||
                config.badEarlyWindow.count() < 0)
            {
                throw sessionError("judgement windows must be non-negative");
            }
            if (config.criticalPerfectWindow > config.normalPerfectWindow ||
                config.normalPerfectWindow > config.greatWindow ||
                config.greatWindow > config.badEarlyWindow)
            {
                throw sessionError("judgement windows must be ordered from strict to lenient");
            }
            if (config.scrollLeadTime.count() <= 0)
            {
                throw sessionError("scrollLeadTime must be greater than zero");
            }
            if (!std::isfinite(config.judgementLineY) || config.judgementLineY < 0.0F || config.judgementLineY > 1.0F)
            {
                throw sessionError("judgementLineY must be finite and in range [0, 1]");
            }
            if (!std::isfinite(config.noteVisualHeight) || config.noteVisualHeight <= 0.0F || config.noteVisualHeight > 1.0F)
            {
                throw sessionError("noteVisualHeight must be finite and in range (0, 1]");
            }
        }

        /**
         * @brief Validates chart assumptions needed by the runtime session.
         * @param chart Chart to inspect.
         * @throws std::runtime_error when chart data violates session assumptions.
         * @note ChartLoader already performs deeper JSON validation; this protects direct test construction too.
         */
        void validateChart(const Chart& chart)
        {
            if (chart.laneCount != static_cast<int>(GAME_LANE_COUNT))
            {
                throw sessionError("only the current 4K lane count is supported");
            }
            if (chart.timingPoints.empty())
            {
                throw sessionError("timingPoints must not be empty");
            }
            if (!(chart.timingPoints.front().beat == ZERO_BEAT))
            {
                throw sessionError("first timing point must start at beat zero");
            }
            for (std::size_t index = 0; index < chart.timingPoints.size(); ++index)
            {
                const TimingPoint& timingPoint = chart.timingPoints[index];
                validateBeatPosition(timingPoint.beat, "timing point beat");
                if (!std::isfinite(timingPoint.bpm) || timingPoint.bpm <= 0.0)
                {
                    throw sessionError("timing point BPM must be finite and greater than zero");
                }
                if (index > 0 && !(chart.timingPoints[index - 1].beat < timingPoint.beat))
                {
                    throw sessionError("timingPoints must be strictly sorted by beat");
                }
            }
            for (const Note& note : chart.notes)
            {
                validateBeatPosition(note.beat, "note beat");
                if (note.endBeat.has_value())
                {
                    validateBeatPosition(*note.endBeat, "note endBeat");
                    if (!(note.beat < *note.endBeat))
                    {
                        throw sessionError("hold note endBeat must be greater than beat");
                    }
                }
                if (note.column < 0 || note.column >= chart.laneCount)
                {
                    throw sessionError("note column is outside the supported lane range");
                }
            }
        }

        /**
         * @brief Resolves a beat position to chart time before metadata offset.
         * @param beat Beat position to resolve.
         * @param timingPoints Sorted BPM timing points.
         * @return Absolute chart time in microseconds.
         * @throws std::runtime_error when the resolved time is outside supported range.
         * @note Integrates BPM segments in quarter-note beat units.
         */
        std::chrono::microseconds resolveBeatTime(
            const BeatPosition& beat,
            const std::vector<TimingPoint>& timingPoints)
        {
            const long double targetBeat = toBeatCount(beat);
            long double elapsedSeconds = 0.0L;

            for (std::size_t index = 0; index < timingPoints.size(); ++index)
            {
                const long double segmentBeat = toBeatCount(timingPoints[index].beat);
                const long double nextBeat = index + 1U < timingPoints.size()
                    ? toBeatCount(timingPoints[index + 1U].beat)
                    : targetBeat;
                if (targetBeat <= segmentBeat)
                {
                    break;
                }

                const long double segmentEndBeat = std::min(targetBeat, nextBeat);
                if (segmentEndBeat > segmentBeat)
                {
                    const long double beatLengthSeconds = 60.0L / static_cast<long double>(timingPoints[index].bpm);
                    elapsedSeconds += (segmentEndBeat - segmentBeat) * beatLengthSeconds;
                }
                if (targetBeat < nextBeat)
                {
                    break;
                }
            }

            return secondsToMicroseconds(elapsedSeconds);
        }

        /**
         * @brief Adds metadata offset to a resolved beat time.
         * @param beatTime Time resolved from BPM timing points.
         * @param offsetMilliseconds Chart metadata offset in milliseconds.
         * @return Target judgement time after offset application.
         * @throws std::runtime_error when the offset would make target time negative.
         * @note The initial convention is target time equals resolved beat time plus chart offset.
         */
        std::chrono::microseconds applyChartOffset(
            std::chrono::microseconds beatTime,
            int offsetMilliseconds)
        {
            const std::chrono::microseconds offset = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::milliseconds{offsetMilliseconds});
            if (offset.count() < 0 && beatTime < -offset)
            {
                throw sessionError("chart offset moves a note before time zero");
            }
            if (offset.count() > 0 && beatTime > std::chrono::microseconds::max() - offset)
            {
                throw sessionError("chart offset moves a note outside the supported duration range");
            }

            return beatTime + offset;
        }

        /**
         * @brief Checks whether lhs - rhs would fit in microseconds.
         * @param lhs Left duration operand.
         * @param rhs Right duration operand.
         * @return True when subtraction is representable.
         * @note Avoids undefined signed overflow when external clocks provide extreme values.
         */
        bool canSubtract(std::chrono::microseconds lhs, std::chrono::microseconds rhs) noexcept
        {
            if (rhs.count() > 0)
            {
                return lhs.count() >= std::chrono::microseconds::min().count() + rhs.count();
            }
            if (rhs.count() < 0)
            {
                return lhs.count() <= std::chrono::microseconds::max().count() + rhs.count();
            }

            return true;
        }

        /**
         * @brief Subtracts two durations after range validation.
         * @param lhs Left duration operand.
         * @param rhs Right duration operand.
         * @param context Error context for diagnostics.
         * @return Difference in microseconds.
         * @throws std::runtime_error when subtraction would overflow.
         * @note Keeps session update deterministic for malformed or extreme clock input.
         */
        std::chrono::microseconds checkedSubtract(
            std::chrono::microseconds lhs,
            std::chrono::microseconds rhs,
            const std::string& context)
        {
            if (!canSubtract(lhs, rhs))
            {
                throw sessionError(context + " duration difference is outside the supported range");
            }

            return lhs - rhs;
        }

        /**
         * @brief Checks whether two durations can be added safely.
         * @param lhs Left duration operand.
         * @param rhs Right duration operand.
         * @return True when addition is representable.
         * @note Used for judgement bound comparisons without overflow.
         */
        bool canAdd(std::chrono::microseconds lhs, std::chrono::microseconds rhs) noexcept
        {
            if (rhs.count() > 0)
            {
                return lhs.count() <= std::chrono::microseconds::max().count() - rhs.count();
            }
            if (rhs.count() < 0)
            {
                return lhs.count() >= std::chrono::microseconds::min().count() - rhs.count();
            }

            return true;
        }

        /**
         * @brief Adds two durations after range validation.
         * @param lhs Left duration operand.
         * @param rhs Right duration operand.
         * @param context Error context for diagnostics.
         * @return Sum in microseconds.
         * @throws std::runtime_error when addition would overflow.
         * @note Keeps note expiry comparisons inside defined signed arithmetic.
         */
        std::chrono::microseconds checkedAdd(
            std::chrono::microseconds lhs,
            std::chrono::microseconds rhs,
            const std::string& context)
        {
            if (!canAdd(lhs, rhs))
            {
                throw sessionError(context + " duration sum is outside the supported range");
            }

            return lhs + rhs;
        }

        /**
         * @brief Safely negates a duration.
         * @param value Duration to negate.
         * @return Negated duration.
         * @throws std::runtime_error when value is the minimum representable duration.
         * @note Prevents overflow in absolute delta classification.
         */
        std::chrono::microseconds checkedNegate(std::chrono::microseconds value)
        {
            if (value == std::chrono::microseconds::min())
            {
                throw sessionError("timing delta is outside the supported range");
            }

            return -value;
        }

        /**
         * @brief Classifies a signed tap timing delta.
         * @param delta Input time minus note target time.
         * @param config Validated session config.
         * @return Judgement category when the input should consume the note.
         * @note Inputs before the Bad window return empty and do not consume the note.
         */
        std::optional<JudgementResult> classifyTapDelta(
            std::chrono::microseconds delta,
            const GameplaySessionConfig& config)
        {
            const std::chrono::microseconds absoluteDelta = delta < std::chrono::microseconds{0} ? checkedNegate(delta) : delta;
            if (absoluteDelta <= config.criticalPerfectWindow)
            {
                return JudgementResult::CriticalPerfect;
            }
            if (absoluteDelta <= config.normalPerfectWindow)
            {
                return JudgementResult::NormalPerfect;
            }
            if (absoluteDelta <= config.greatWindow)
            {
                return JudgementResult::Great;
            }
            if (delta < std::chrono::microseconds{0} && checkedNegate(delta) <= config.badEarlyWindow)
            {
                return JudgementResult::Bad;
            }

            return std::nullopt;
        }

        /**
         * @brief Validates timestamped press events before mutating session state.
         * @param currentSongTime Current frame song time.
         * @param pressEvents Events gathered for this update.
         * @throws std::runtime_error when events are out of range, future-dated, or out of order per lane.
         * @note Requiring per-lane chronological order keeps dense same-lane judgement deterministic.
         */
        void validatePressEvents(
            std::chrono::microseconds currentSongTime,
            const std::vector<LanePressEvent>& pressEvents,
            const GameplaySessionConfig& config)
        {
            std::array<std::optional<std::chrono::microseconds>, GAME_LANE_COUNT> lastLanePressTimes{};
            for (const LanePressEvent& pressEvent : pressEvents)
            {
                if (pressEvent.lane < 0 || pressEvent.lane >= static_cast<int>(GAME_LANE_COUNT))
                {
                    throw sessionError("lane press event is outside the supported lane range");
                }
                if (pressEvent.songTime < std::chrono::microseconds{0} ||
                    !canAdd(pressEvent.songTime, config.badEarlyWindow))
                {
                    throw sessionError("lane press event time is outside the supported range");
                }
                if (pressEvent.songTime > currentSongTime)
                {
                    throw sessionError("lane press event time must not be later than currentSongTime");
                }

                std::optional<std::chrono::microseconds>& lastPressTime = lastLanePressTimes[static_cast<std::size_t>(pressEvent.lane)];
                if (lastPressTime.has_value() && pressEvent.songTime < *lastPressTime)
                {
                    throw sessionError("lane press events must be ordered by songTime within each lane");
                }
                lastPressTime = pressEvent.songTime;
            }
        }

        /**
         * @brief Reports whether a note has passed its late miss threshold.
         * @param currentSongTime Current song playback time.
         * @param targetTime Note target time.
         * @param greatWindow Late Great half-window.
         * @return True when the note should be marked Miss.
         * @throws std::runtime_error when targetTime + greatWindow would overflow.
         * @note Compares timestamps by adding the bounded window instead of subtracting first.
         */
        bool isPastMissWindow(
            std::chrono::microseconds currentSongTime,
            std::chrono::microseconds targetTime,
            std::chrono::microseconds greatWindow)
        {
            return currentSongTime > checkedAdd(targetTime, greatWindow, "miss window");
        }

    }

    GameplaySession::GameplaySession(const Chart& chart, const GameplaySessionConfig& config)
        : m_config(config)
    {
        validateConfig(m_config);
        validateChart(chart);

        m_notes.reserve(chart.notes.size());
        m_events.reserve(chart.notes.size());
        m_snapshot.visibleNotes.reserve(chart.notes.size());
        for (std::size_t index = 0; index < chart.notes.size(); ++index)
        {
            const Note& note = chart.notes[index];
            const std::chrono::microseconds beatTime = resolveBeatTime(note.beat, chart.timingPoints);
            m_notes.push_back({
                index,
                note.column,
                applyChartOffset(beatTime, chart.meta.offsetMilliseconds),
                note.isHold(),
                false
            });
        }

        std::sort(
            m_notes.begin(),
            m_notes.end(),
            [](const RuntimeNote& lhs, const RuntimeNote& rhs)
            {
                if (lhs.targetTime == rhs.targetTime)
                {
                    if (lhs.lane == rhs.lane)
                    {
                        return lhs.noteIndex < rhs.noteIndex;
                    }
                    return lhs.lane < rhs.lane;
                }
                return lhs.targetTime < rhs.targetTime;
            });

        LaneInputSnapshot input{};
        update(std::chrono::microseconds{0}, input);
    }

    void GameplaySession::update(std::chrono::microseconds currentSongTime, const LaneInputSnapshot& input)
    {
        std::vector<LanePressEvent> pressEvents;
        pressEvents.reserve(input.lanes.size());
        for (std::size_t lane = 0; lane < input.lanes.size(); ++lane)
        {
            if (input.lanes[lane].wasPressed)
            {
                pressEvents.push_back({static_cast<int>(lane), currentSongTime});
            }
        }

        update(currentSongTime, input, pressEvents);
    }

    void GameplaySession::update(
        std::chrono::microseconds currentSongTime,
        const LaneInputSnapshot& input,
        const std::vector<LanePressEvent>& pressEvents)
    {
        if (currentSongTime < std::chrono::microseconds{0} ||
            !canAdd(currentSongTime, m_config.badEarlyWindow))
        {
            throw sessionError("currentSongTime is outside the supported range");
        }

        validatePressEvents(currentSongTime, pressEvents, m_config);

        for (const LanePressEvent& pressEvent : pressEvents)
        {
            auto noteIterator = std::find_if(
                m_notes.begin(),
                m_notes.end(),
                [&pressEvent](const RuntimeNote& note)
                {
                    return !note.isResolved && note.lane == pressEvent.lane;
                });
            if (noteIterator == m_notes.end())
            {
                continue;
            }

            const std::chrono::microseconds delta = checkedSubtract(
                pressEvent.songTime,
                noteIterator->targetTime,
                "tap timing delta");
            const std::optional<JudgementResult> result = classifyTapDelta(delta, m_config);
            if (!result.has_value())
            {
                continue;
            }

            noteIterator->isResolved = true;
            switch (*result)
            {
            case JudgementResult::CriticalPerfect:
                ++m_counts.criticalPerfect;
                break;
            case JudgementResult::NormalPerfect:
                ++m_counts.normalPerfect;
                break;
            case JudgementResult::Great:
                ++m_counts.great;
                break;
            case JudgementResult::Bad:
                ++m_counts.bad;
                break;
            case JudgementResult::Miss:
                ++m_counts.miss;
                break;
            }

            if (*result == JudgementResult::Miss)
            {
                m_combo = 0;
            }
            else
            {
                ++m_combo;
            }

            JudgementEvent event{};
            event.lane = noteIterator->lane;
            event.noteIndex = noteIterator->noteIndex;
            event.result = *result;
            event.timingDelta = delta;
            m_events.push_back(event);
            m_lastJudgement = event;
        }

        for (RuntimeNote& note : m_notes)
        {
            if (!note.isResolved && isPastMissWindow(currentSongTime, note.targetTime, m_config.greatWindow))
            {
                const std::chrono::microseconds timingDelta = checkedSubtract(
                    currentSongTime,
                    note.targetTime,
                    "miss timing delta");
                note.isResolved = true;
                ++m_counts.miss;
                m_combo = 0;
                JudgementEvent event{};
                event.lane = note.lane;
                event.noteIndex = note.noteIndex;
                event.result = JudgementResult::Miss;
                event.timingDelta = timingDelta;
                m_events.push_back(event);
                m_lastJudgement = event;
            }
        }

        std::vector<VisibleNoteVisual> previousVisibleNotes = std::move(m_snapshot.visibleNotes);
        m_snapshot = GameplaySnapshot{};
        m_snapshot.visibleNotes = std::move(previousVisibleNotes);
        m_snapshot.visibleNotes.clear();
        m_snapshot.judgementLineY = m_config.judgementLineY;
        for (std::size_t lane = 0; lane < input.lanes.size(); ++lane)
        {
            m_snapshot.lanes[lane].isPressed = input.lanes[lane].isDown;
        }

        const float leadTime = static_cast<float>(m_config.scrollLeadTime.count());
        for (const RuntimeNote& note : m_notes)
        {
            if (note.isResolved)
            {
                continue;
            }

            const std::chrono::microseconds remainingDuration = checkedSubtract(
                note.targetTime,
                currentSongTime,
                "visible note timing delta");
            const float remaining = static_cast<float>(remainingDuration.count());
            const float progress = remaining / leadTime;
            const float top = m_config.judgementLineY - progress * m_config.judgementLineY;
            if (top + m_config.noteVisualHeight < MIN_VISIBLE_TOP || top > MAX_VISIBLE_TOP)
            {
                continue;
            }

            m_snapshot.visibleNotes.push_back({note.lane, top, m_config.noteVisualHeight, note.isHold});
        }
    }

    const GameplaySnapshot& GameplaySession::getSnapshot() const noexcept
    {
        return m_snapshot;
    }

    const JudgementCounts& GameplaySession::getJudgementCounts() const noexcept
    {
        return m_counts;
    }

    int GameplaySession::getCombo() const noexcept
    {
        return m_combo;
    }

    const std::vector<JudgementEvent>& GameplaySession::getJudgementEvents() const noexcept
    {
        return m_events;
    }

    std::optional<JudgementEvent> GameplaySession::consumeLastJudgement() noexcept
    {
        std::optional<JudgementEvent> event = m_lastJudgement;
        m_lastJudgement.reset();
        return event;
    }

    bool GameplaySession::isFinished() const noexcept
    {
        return std::all_of(
            m_notes.begin(),
            m_notes.end(),
            [](const RuntimeNote& note)
            {
                return note.isResolved;
            });
    }
}
