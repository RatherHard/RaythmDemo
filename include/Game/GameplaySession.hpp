// RaythmDemo - Game Gameplay Session
// Declares pure tap-note judgement and gameplay snapshot state.
// Author: RatherHard
// Date: 2026-07-07

#pragma once

#include "Game/Chart.hpp"
#include "Game/GameplaySnapshot.hpp"
#include "Game/LaneInput.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

namespace Raythm::Game
{
    /**
     * @brief Tap judgement categories produced by the initial gameplay session.
     */
    enum class JudgementResult
    {
        /** @brief Input landed inside the documented +/-20ms window. */
        CriticalPerfect,

        /** @brief Input landed inside the +/-40ms window but outside Critical Perfect. */
        NormalPerfect,

        /** @brief Input landed inside the +/-80ms window but outside Perfect. */
        Great,

        /** @brief Input landed inside the early-only [-120ms, -80ms) window. */
        Bad,

        /** @brief Note passed the late Great window without being hit. */
        Miss
    };

    /**
     * @brief Running counts for each tap judgement category.
     */
    struct JudgementCounts
    {
        /** @brief Number of Critical Perfect judgements. */
        int criticalPerfect = 0;

        /** @brief Number of Normal Perfect judgements. */
        int normalPerfect = 0;

        /** @brief Number of Great judgements. */
        int great = 0;

        /** @brief Number of Bad judgements. */
        int bad = 0;

        /** @brief Number of Miss judgements. */
        int miss = 0;
    };

    /**
     * @brief One resolved note judgement event.
     */
    struct JudgementEvent
    {
        /** @brief Zero-based lane that owned the judged note. */
        int lane = 0;

        /** @brief Stable index of the note in the source chart note array. */
        std::size_t noteIndex = 0;

        /** @brief Judgement category assigned to the note. */
        JudgementResult result = JudgementResult::Miss;

        /** @brief Signed input time delta relative to the note target time. */
        std::chrono::microseconds timingDelta{0};
    };

    /**
     * @brief One timestamped lane press captured during a gameplay frame.
     */
    struct LanePressEvent
    {
        /** @brief Zero-based lane that received a pressed edge. */
        int lane = 0;

        /** @brief Song time when the press occurred. */
        std::chrono::microseconds songTime{0};
    };

    /**
     * @brief Tunable constants for the first tap-only gameplay session.
     */
    struct GameplaySessionConfig
    {
        /** @brief Inclusive Critical Perfect half-window. */
        std::chrono::microseconds criticalPerfectWindow{std::chrono::milliseconds{20}};

        /** @brief Inclusive Normal Perfect half-window. */
        std::chrono::microseconds normalPerfectWindow{std::chrono::milliseconds{40}};

        /** @brief Inclusive Great half-window and late Miss threshold. */
        std::chrono::microseconds greatWindow{std::chrono::milliseconds{80}};

        /** @brief Inclusive early Bad start threshold. */
        std::chrono::microseconds badEarlyWindow{std::chrono::milliseconds{120}};

        /** @brief Time before target when a note reaches the top of the visible lane area. */
        std::chrono::microseconds scrollLeadTime{std::chrono::milliseconds{2000}};

        /** @brief Normalized y coordinate of the judgement line in generated snapshots. */
        float judgementLineY = 0.85F;

        /** @brief Normalized visual height assigned to tap-note heads. */
        float noteVisualHeight = 0.05F;
    };

    /**
     * @brief Pure gameplay state machine for the initial tap-note judgement loop.
     */
    class GameplaySession
    {
    public:
        /**
         * @brief Creates a deterministic gameplay session from loaded chart data.
         * @param chart Runtime chart data loaded by ChartLoader or tests.
         * @param config Judgement windows and visual timing configuration.
         * @throws std::runtime_error when chart or config values violate runtime assumptions.
         * @note Precomputes note target times so per-frame update work stays small and deterministic.
         */
        explicit GameplaySession(const Chart& chart, const GameplaySessionConfig& config = {});

        /**
         * @brief Advances judgement state with frame-level lane edges at the supplied song time.
         * @param currentSongTime Current song playback time used for judgement and miss sweeping.
         * @param input Semantic lane input snapshot for the current frame.
         * @note Each wasPressed lane is treated as one press event at currentSongTime.
         */
        void update(std::chrono::microseconds currentSongTime, const LaneInputSnapshot& input);

        /**
         * @brief Advances judgement state with timestamped lane press events captured during the frame.
         * @param currentSongTime Current song playback time used for snapshot generation and miss sweeping.
         * @param input Semantic lane input snapshot for the current frame.
         * @param pressEvents Timestamped lane press events gathered since the previous update.
         * @throws std::runtime_error when a press event lane is invalid or duration arithmetic would overflow.
         * @note Timestamped events preserve dense same-lane taps and late-frame inputs better than frame-level edges.
         */
        void update(
            std::chrono::microseconds currentSongTime,
            const LaneInputSnapshot& input,
            const std::vector<LanePressEvent>& pressEvents);

        /**
         * @brief Returns the latest view-facing gameplay snapshot.
         * @return Snapshot containing lane pressed states and unresolved visible notes.
         * @note The returned reference remains valid until the next update or session destruction.
         */
        [[nodiscard]] const GameplaySnapshot& getSnapshot() const noexcept;

        /**
         * @brief Returns cumulative judgement counts.
         * @return Counts for each supported judgement category.
         * @note Counts include both input-triggered hits and automatic misses.
         */
        [[nodiscard]] const JudgementCounts& getJudgementCounts() const noexcept;

        /**
         * @brief Returns the current combo count.
         * @return Number of consecutive non-Miss judgements since the last Miss.
         * @note Bad counts as a resolved hit in this tap-only slice; Miss resets combo to zero.
         */
        [[nodiscard]] int getCombo() const noexcept;

        /**
         * @brief Returns all judgement events recorded since session creation.
         * @return Ordered judgement event history.
         * @note Exposed for deterministic tests and later result-summary surfaces.
         */
        [[nodiscard]] const std::vector<JudgementEvent>& getJudgementEvents() const noexcept;

        /**
         * @brief Consumes the most recent judgement event if one exists.
         * @return Last event since the previous consume call, or empty when none exists.
         * @note This is intended for future UI feedback that only needs the latest judgement flash.
         */
        [[nodiscard]] std::optional<JudgementEvent> consumeLastJudgement() noexcept;

        /**
         * @brief Reports whether every chart note has been resolved as hit or missed.
         * @return True when no unresolved notes remain.
         * @note Empty charts are finished immediately.
         */
        [[nodiscard]] bool isFinished() const noexcept;

    private:
        /**
         * @brief Internal precomputed note state used by the session update loop.
         */
        struct RuntimeNote
        {
            /** @brief Stable index of this note in the source chart note array. */
            std::size_t noteIndex = 0;

            /** @brief Zero-based lane containing the note. */
            int lane = 0;

            /** @brief Audio/song time when the note head should be judged. */
            std::chrono::microseconds targetTime{0};

            /** @brief True when the source note is a hold; the head is judged like tap in this slice. */
            bool isHold = false;

            /** @brief True once the note has been hit or missed. */
            bool isResolved = false;
        };

        /** @brief Session configuration validated at construction. */
        GameplaySessionConfig m_config{};

        /** @brief Precomputed note states sorted by target time and lane. */
        std::vector<RuntimeNote> m_notes;

        /** @brief Latest generated gameplay view snapshot. */
        GameplaySnapshot m_snapshot{};

        /** @brief Cumulative judgement category counts. */
        JudgementCounts m_counts{};

        /** @brief Ordered judgement events emitted by this session. */
        std::vector<JudgementEvent> m_events;

        /** @brief Latest unconsumed judgement event for feedback surfaces. */
        std::optional<JudgementEvent> m_lastJudgement;

        /** @brief Consecutive non-Miss judgement count. */
        int m_combo = 0;
    };
}
