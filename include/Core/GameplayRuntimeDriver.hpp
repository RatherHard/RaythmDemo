// RaythmDemo - Core Gameplay Runtime Driver
// Declares the Core seam that turns platform input and song time into gameplay render commands.
// Author: RatherHard
// Date: 2026-07-07

#pragma once

#include "Game/Chart.hpp"
#include "Game/GameplaySession.hpp"
#include "Game/GameplayView.hpp"
#include "Game/LaneInput.hpp"
#include "Platform/InputState.hpp"
#include "Render/RenderStrategy.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace Raythm::Core
{
    /**
     * @brief Bridges platform input, gameplay state, and gameplay view command generation for one runtime chart.
     */
    class GameplayRuntimeDriver
    {
    public:
        /**
         * @brief Creates a runtime driver from an already loaded chart.
         * @param chart Runtime chart data used to initialize the gameplay session.
         * @param sessionConfig Judgement and snapshot configuration for the gameplay session.
         * @param baseLayout Base gameplay view layout whose proportions are scaled to the active viewport.
         * @throws std::runtime_error when chart, session config, key map, or view layout values are invalid.
         * @note The driver does not own audio, renderer, window, or platform event polling.
         */
        explicit GameplayRuntimeDriver(
            const Game::Chart& chart,
            const Game::GameplaySessionConfig& sessionConfig = {},
            const Game::GameplayViewLayout& baseLayout = {});

        /**
         * @brief Advances gameplay for one frame and builds render commands for the current viewport.
         * @param currentSongTime Audio playback time for the current frame.
         * @param inputState Platform input state after this frame's input events have been applied.
         * @param viewportWidth Current drawable or window viewport width in pixels.
         * @param viewportHeight Current drawable or window viewport height in pixels.
         * @param pressEvents Timestamped lane press events captured while polling this frame's input.
         * @return Render commands representing the latest gameplay snapshot.
         * @throws std::runtime_error when viewport dimensions are invalid or gameplay update fails.
         * @note The returned command batch is intended to replace Renderer pending 2D commands each frame.
         */
        [[nodiscard]] std::vector<Render::RenderCommand> update(
            std::chrono::microseconds currentSongTime,
            const Platform::InputState& inputState,
            int viewportWidth,
            int viewportHeight,
            const std::vector<Game::LanePressEvent>& pressEvents = {});

        /**
         * @brief Converts a platform key press event into a timestamped lane press event when it is bound.
         * @param inputEvent Platform input event observed during event polling.
         * @param frameSongTime Audio song time sampled for the current frame.
         * @param frameTimestampNanoseconds Platform timestamp sampled near frameSongTime.
         * @return Lane press event when the input is a non-repeat bound key press; otherwise empty.
         * @note Event timestamp age is subtracted from frameSongTime so delayed event processing does not shift input late.
         */
        [[nodiscard]] std::optional<Game::LanePressEvent> makeLanePressEvent(
            const Platform::InputEvent& inputEvent,
            std::chrono::microseconds frameSongTime,
            std::uint64_t frameTimestampNanoseconds) const noexcept;

        /**
         * @brief Converts a platform key press event into a timestamped lane press event when it is bound.
         * @param inputEvent Platform input event observed during event polling.
         * @param songTime Song time sampled close to the input event handling point.
         * @return Lane press event when the input is a non-repeat bound key press; otherwise empty.
         * @note Compatibility overload for tests and callers that already have an event-time song timestamp.
         */
        [[nodiscard]] std::optional<Game::LanePressEvent> makeLanePressEvent(
            const Platform::InputEvent& inputEvent,
            std::chrono::microseconds songTime) const noexcept;

        /**
         * @brief Returns the gameplay session owned by the driver.
         * @return Current gameplay session state.
         * @note Exposed for tests and future UI surfaces that need judgement counts or session completion state.
         */
        [[nodiscard]] const Game::GameplaySession& getGameplaySession() const noexcept;

        /**
         * @brief Returns the view layout currently used for command generation.
         * @return Current gameplay view layout scaled to the latest viewport.
         * @note Exposed for deterministic tests and diagnostics.
         */
        [[nodiscard]] const Game::GameplayViewLayout& getCurrentViewLayout() const noexcept;

    private:
        /**
         * @brief Builds a viewport-scaled layout from the base layout proportions.
         * @param baseLayout Reference layout that defines proportions and colors.
         * @param viewportWidth Target viewport width in pixels.
         * @param viewportHeight Target viewport height in pixels.
         * @return View layout scaled to fit the target viewport.
         * @throws std::runtime_error when viewport dimensions are invalid.
         * @note This keeps the first playable slice resize-aware without adding renderer dependencies to Game.
         */
        [[nodiscard]] static Game::GameplayViewLayout makeLayoutForViewport(
            const Game::GameplayViewLayout& baseLayout,
            int viewportWidth,
            int viewportHeight);

        /**
         * @brief Recreates the view adapter when the active viewport changes.
         * @param viewportWidth Current viewport width in pixels.
         * @param viewportHeight Current viewport height in pixels.
         * @throws std::runtime_error when the scaled layout is invalid.
         */
        void refreshViewLayout(int viewportWidth, int viewportHeight);

        /** @brief Maps platform key state into semantic four-lane gameplay input. */
        Game::LaneInputMapper m_inputMapper{};

        /** @brief Gameplay judgement and snapshot state for the loaded chart. */
        Game::GameplaySession m_session;

        /** @brief Reference layout used to derive viewport-scaled layouts. */
        Game::GameplayViewLayout m_baseLayout{};

        /** @brief View layout currently used by the adapter. */
        Game::GameplayViewLayout m_currentLayout{};

        /** @brief Pure adapter that converts gameplay snapshots into render commands. */
        Game::GameplayViewAdapter m_viewAdapter;

        /** @brief Last viewport width used to build the current adapter. */
        int m_viewportWidth = 0;

        /** @brief Last viewport height used to build the current adapter. */
        int m_viewportHeight = 0;
    };
}
