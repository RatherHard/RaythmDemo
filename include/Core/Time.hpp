// RaythmDemo - Core Time System
// Defines monotonic frame timing snapshots for application lifecycle orchestration.
// Author: RatherHard
// Date: 2026-07-04

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

namespace Raythm::Core
{
    /** @brief Monotonic clock used by the Core timing system. */
    using Clock = std::chrono::steady_clock;

    /** @brief Monotonic time point sampled from the Core clock. */
    using TimePoint = Clock::time_point;

    /** @brief Duration type used for Core frame timing. */
    using Duration = Clock::duration;

    /** @brief Callable that returns the current monotonic time point. */
    using NowFunction = std::function<TimePoint()>;

    /** @brief Maximum variable-frame delta exposed to simulation by default. */
    constexpr double DEFAULT_MAX_DELTA_SECONDS = 0.1;

    /**
     * @brief Immutable snapshot of timing values sampled at the start of one application frame.
     */
    struct FrameTime
    {
        /** @brief Zero-based frame index since the last time-system reset. */
        std::uint64_t frameIndex = 0;

        /** @brief Delta time in seconds exposed to simulation after clamping. */
        double deltaSeconds = 0.0;

        /** @brief Raw monotonic delta in seconds before clamping. */
        double rawDeltaSeconds = 0.0;

        /** @brief Total monotonic elapsed time in seconds since reset. */
        double elapsedSeconds = 0.0;

        /** @brief True when deltaSeconds was capped by the configured maximum delta. */
        bool wasDeltaClamped = false;
    };

    /**
     * @brief Produces monotonic per-frame timing snapshots for the application loop.
     */
    class TimeSystem
    {
    public:
        /**
         * @brief Creates a time system using std::chrono::steady_clock.
         * @note The first beginFrame call returns zero delta to avoid a startup spike.
         */
        TimeSystem();

        /**
         * @brief Creates a time system using an injected monotonic clock function.
         * @param nowFunction Callable used to sample current monotonic time.
         * @param maxDeltaSeconds Maximum delta exposed to simulation.
         * @note Injecting the clock keeps Core timing tests deterministic without sleeping.
         */
        explicit TimeSystem(NowFunction nowFunction, double maxDeltaSeconds = DEFAULT_MAX_DELTA_SECONDS);

        /**
         * @brief Samples the clock and returns timing values for the frame about to run.
         * @return Immutable frame timing snapshot.
         * @note Call once at the start of each application loop iteration.
         */
        FrameTime beginFrame();

        /**
         * @brief Resets frame index, elapsed time, and previous sample to the current clock value.
         * @note The next beginFrame call is treated as frame zero with zero delta.
         */
        void reset();

        /** @brief Returns the most recent frame timing snapshot. */
        const FrameTime& getCurrentFrameTime() const noexcept;

    private:
        /** @brief Clock function used for monotonic samples. */
        NowFunction m_nowFunction;

        /** @brief Maximum delta exposed to simulation after stalls. */
        double m_maxDeltaSeconds = DEFAULT_MAX_DELTA_SECONDS;

        /** @brief Time point sampled at reset. */
        TimePoint m_startTime{};

        /** @brief Previous frame start time point. */
        TimePoint m_previousFrameTime{};

        /** @brief Most recent immutable frame snapshot. */
        FrameTime m_currentFrameTime{};

        /** @brief Next zero-based frame index to publish. */
        std::uint64_t m_nextFrameIndex = 0;

        /** @brief True until the first beginFrame after construction or reset. */
        bool m_isFirstFrame = true;
    };
}
