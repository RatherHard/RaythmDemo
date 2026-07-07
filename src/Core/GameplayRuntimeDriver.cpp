// RaythmDemo - Core Gameplay Runtime Driver Implementation
// Implements the Core gameplay frame pipeline from platform input to render commands.
// Author: RatherHard
// Date: 2026-07-07

#include "Core/GameplayRuntimeDriver.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace Raythm::Core
{
    namespace
    {
        /** @brief Largest viewport dimension accepted by the runtime driver before view layout validation. */
        constexpr int MAX_RUNTIME_VIEWPORT_DIMENSION = 16'384;

        /** @brief Minimum horizontal margin retained around the scaled gameplay playfield. */
        constexpr double MIN_HORIZONTAL_MARGIN_RATIO = 0.05;

        /** @brief Minimum vertical margin retained around the scaled gameplay playfield. */
        constexpr double MIN_VERTICAL_MARGIN_RATIO = 0.05;

        /** @brief Fraction of viewport width used by the gameplay playfield before clamping. */
        constexpr double PLAYFIELD_WIDTH_RATIO = 0.3125;

        /** @brief Fraction of viewport height used by the gameplay playfield before clamping. */
        constexpr double PLAYFIELD_HEIGHT_RATIO = 0.8666666666666667;

        /**
         * @brief Rounds a positive layout value to the nearest integer while preserving visibility.
         * @param value Floating point layout value.
         * @return Rounded positive integer layout value.
         * @note Values are clamped to at least one pixel for dimensions and spacing that must remain visible.
         */
        int roundPositiveLayoutValue(double value) noexcept
        {
            return std::max(1, static_cast<int>(std::lround(value)));
        }

        /**
         * @brief Computes a positive playfield dimension inside the viewport and margin budget.
         * @param desired Desired dimension derived from viewport ratio.
         * @param viewport Target viewport dimension.
         * @param minimumMargin Minimum margin on each side.
         * @return Playfield dimension clamped inside the viewport.
         */
        int computePlayfieldDimension(double desired, int viewport, int minimumMargin) noexcept
        {
            const int maximumDimension = std::max(1, viewport - minimumMargin * 2);
            return std::clamp(roundPositiveLayoutValue(desired), 1, maximumDimension);
        }
        /**
         * @brief Converts a non-negative nanosecond duration to microseconds without rounding forward.
         * @param nanoseconds Platform timestamp delta in nanoseconds.
         * @return Duration truncated to whole microseconds.
         */
        std::chrono::microseconds nanosecondsToMicroseconds(std::uint64_t nanoseconds) noexcept
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::nanoseconds{nanoseconds});
        }

        /**
         * @brief Maps an event timestamp into a valid song time using a same-frame song time sample.
         * @param frameSongTime Song time sampled for the current frame.
         * @param eventTimestampNanoseconds Timestamp carried by the input event.
         * @param frameTimestampNanoseconds Platform timestamp sampled near frameSongTime.
         * @return Event song time clamped between song start and frameSongTime.
         */
        std::chrono::microseconds mapEventTimestampToSongTime(
            std::chrono::microseconds frameSongTime,
            std::uint64_t eventTimestampNanoseconds,
            std::uint64_t frameTimestampNanoseconds) noexcept
        {
            if (eventTimestampNanoseconds >= frameTimestampNanoseconds)
            {
                return frameSongTime;
            }

            const std::chrono::microseconds eventAge = nanosecondsToMicroseconds(
                frameTimestampNanoseconds - eventTimestampNanoseconds);
            if (eventAge >= frameSongTime)
            {
                return std::chrono::microseconds{0};
            }

            return frameSongTime - eventAge;
        }
    }

    GameplayRuntimeDriver::GameplayRuntimeDriver(
        const Game::Chart& chart,
        const Game::GameplaySessionConfig& sessionConfig,
        const Game::GameplayViewLayout& baseLayout)
        : m_session(chart, sessionConfig),
          m_baseLayout(baseLayout),
          m_currentLayout(baseLayout),
          m_viewAdapter(baseLayout)
    {
    }

    std::vector<Render::RenderCommand> GameplayRuntimeDriver::update(
        std::chrono::microseconds currentSongTime,
        const Platform::InputState& inputState,
        int viewportWidth,
        int viewportHeight,
        const std::vector<Game::LanePressEvent>& pressEvents)
    {
        refreshViewLayout(viewportWidth, viewportHeight);
        const Game::LaneInputSnapshot laneInput = m_inputMapper.sample(inputState);
        if (pressEvents.empty())
        {
            m_session.update(currentSongTime, laneInput);
        }
        else
        {
            m_session.update(currentSongTime, laneInput, pressEvents);
        }
        return m_viewAdapter.buildCommands(m_session.getSnapshot());
    }

    std::optional<Game::LanePressEvent> GameplayRuntimeDriver::makeLanePressEvent(
        const Platform::InputEvent& inputEvent,
        std::chrono::microseconds frameSongTime,
        std::uint64_t frameTimestampNanoseconds) const noexcept
    {
        const std::chrono::microseconds eventSongTime = mapEventTimestampToSongTime(
            frameSongTime,
            inputEvent.timestampNanoseconds,
            frameTimestampNanoseconds);
        return makeLanePressEvent(inputEvent, eventSongTime);
    }

    std::optional<Game::LanePressEvent> GameplayRuntimeDriver::makeLanePressEvent(
        const Platform::InputEvent& inputEvent,
        std::chrono::microseconds songTime) const noexcept
    {
        if (inputEvent.type != Platform::InputEventType::KeyPressed || inputEvent.isRepeat)
        {
            return std::nullopt;
        }

        const Game::LaneKeyMap& keyMap = m_inputMapper.getKeyMap();
        for (std::size_t lane = 0; lane < keyMap.laneScancodes.size(); ++lane)
        {
            if (keyMap.laneScancodes[lane] == inputEvent.scancode)
            {
                return Game::LanePressEvent{static_cast<int>(lane), songTime};
            }
        }

        return std::nullopt;
    }

    const Game::GameplaySession& GameplayRuntimeDriver::getGameplaySession() const noexcept
    {
        return m_session;
    }

    const Game::GameplayViewLayout& GameplayRuntimeDriver::getCurrentViewLayout() const noexcept
    {
        return m_currentLayout;
    }

    Game::GameplayViewLayout GameplayRuntimeDriver::makeLayoutForViewport(
        const Game::GameplayViewLayout& baseLayout,
        int viewportWidth,
        int viewportHeight)
    {
        if (viewportWidth <= 0 || viewportHeight <= 0)
        {
            throw std::runtime_error("Invalid gameplay runtime viewport: dimensions must be positive");
        }
        if (viewportWidth > MAX_RUNTIME_VIEWPORT_DIMENSION || viewportHeight > MAX_RUNTIME_VIEWPORT_DIMENSION)
        {
            throw std::runtime_error("Invalid gameplay runtime viewport: dimensions exceed the supported range");
        }

        Game::GameplayViewLayout layout = baseLayout;
        layout.viewportWidth = viewportWidth;
        layout.viewportHeight = viewportHeight;

        const int minimumHorizontalMargin = roundPositiveLayoutValue(
            static_cast<double>(viewportWidth) * MIN_HORIZONTAL_MARGIN_RATIO);
        const int minimumVerticalMargin = roundPositiveLayoutValue(
            static_cast<double>(viewportHeight) * MIN_VERTICAL_MARGIN_RATIO);
        layout.playfieldWidth = computePlayfieldDimension(
            static_cast<double>(viewportWidth) * PLAYFIELD_WIDTH_RATIO,
            viewportWidth,
            minimumHorizontalMargin);
        layout.playfieldHeight = computePlayfieldDimension(
            static_cast<double>(viewportHeight) * PLAYFIELD_HEIGHT_RATIO,
            viewportHeight,
            minimumVerticalMargin);
        layout.playfieldLeft = (viewportWidth - layout.playfieldWidth) / 2;
        layout.playfieldTop = (viewportHeight - layout.playfieldHeight) / 2;

        const double widthScale = static_cast<double>(layout.playfieldWidth) /
            static_cast<double>(std::max(1, baseLayout.playfieldWidth));
        const double heightScale = static_cast<double>(layout.playfieldHeight) /
            static_cast<double>(std::max(1, baseLayout.playfieldHeight));
        const double uniformScale = std::max(0.25, std::min(widthScale, heightScale));
        layout.laneGap = std::max(0, static_cast<int>(std::lround(static_cast<double>(baseLayout.laneGap) * uniformScale)));
        layout.judgementLineThickness = roundPositiveLayoutValue(
            static_cast<double>(baseLayout.judgementLineThickness) * uniformScale);

        return layout;
    }

    void GameplayRuntimeDriver::refreshViewLayout(int viewportWidth, int viewportHeight)
    {
        if (viewportWidth == m_viewportWidth && viewportHeight == m_viewportHeight)
        {
            return;
        }

        m_currentLayout = makeLayoutForViewport(m_baseLayout, viewportWidth, viewportHeight);
        m_viewAdapter = Game::GameplayViewAdapter{m_currentLayout};
        m_viewportWidth = viewportWidth;
        m_viewportHeight = viewportHeight;
    }
}
