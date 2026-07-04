// RaythmDemo - Core Time System Implementation
// Implements monotonic frame timing snapshots for the application loop.
// Author: RatherHard
// Date: 2026-07-04

#include "Core/Time.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace Raythm::Core
{
    namespace
    {
        /** @brief Returns the current std::chrono steady-clock sample. */
        TimePoint getSteadyClockNow()
        {
            return Clock::now();
        }

        /** @brief Converts a steady-clock duration to fractional seconds. */
        double toSeconds(Duration duration) noexcept
        {
            return std::chrono::duration<double>(duration).count();
        }
    }

    TimeSystem::TimeSystem()
        : TimeSystem(getSteadyClockNow)
    {
    }

    TimeSystem::TimeSystem(NowFunction nowFunction, double maxDeltaSeconds)
        : m_nowFunction(std::move(nowFunction)),
          m_maxDeltaSeconds(maxDeltaSeconds)
    {
        if (!m_nowFunction)
        {
            throw std::invalid_argument("TimeSystem requires a valid monotonic clock function.");
        }

        if (m_maxDeltaSeconds <= 0.0)
        {
            throw std::invalid_argument("TimeSystem maximum delta must be greater than zero.");
        }

        reset();
    }

    FrameTime TimeSystem::beginFrame()
    {
        const TimePoint currentTime = m_nowFunction();
        double rawDeltaSeconds = 0.0;
        double deltaSeconds = 0.0;
        bool wasDeltaClamped = false;

        if (!m_isFirstFrame)
        {
            rawDeltaSeconds = std::max(0.0, toSeconds(currentTime - m_previousFrameTime));
            deltaSeconds = std::min(rawDeltaSeconds, m_maxDeltaSeconds);
            wasDeltaClamped = deltaSeconds < rawDeltaSeconds;
        }

        m_currentFrameTime = FrameTime{
            m_nextFrameIndex,
            deltaSeconds,
            rawDeltaSeconds,
            std::max(0.0, toSeconds(currentTime - m_startTime)),
            wasDeltaClamped
        };

        m_previousFrameTime = currentTime;
        m_isFirstFrame = false;
        ++m_nextFrameIndex;

        return m_currentFrameTime;
    }

    void TimeSystem::reset()
    {
        const TimePoint currentTime = m_nowFunction();
        m_startTime = currentTime;
        m_previousFrameTime = currentTime;
        m_currentFrameTime = {};
        m_nextFrameIndex = 0;
        m_isFirstFrame = true;
    }

    const FrameTime& TimeSystem::getCurrentFrameTime() const noexcept
    {
        return m_currentFrameTime;
    }
}
