// RaythmDemo - Core Gameplay Runtime Driver Tests
// Verifies the Core seam that bridges platform input, gameplay session state, and render commands.
// Author: RatherHard
// Date: 2026-07-07

#include "Core/GameplayRuntimeDriver.hpp"

#include "Game/Chart.hpp"
#include "Game/GameplayView.hpp"
#include "Platform/EventPump.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    namespace Core = Raythm::Core;
    namespace Game = Raythm::Game;
    namespace Platform = Raythm::Platform;
    namespace Render = Raythm::Render;

    /** @brief Number of lane and judgement-line commands emitted before note commands. */
    constexpr std::size_t BASE_GAMEPLAY_COMMAND_COUNT = Game::GAME_LANE_COUNT + 1U;

    /**
     * @brief Records a failed expectation without aborting the current test function.
     * @param condition Boolean result produced by the assertion expression.
     * @param message Human-readable failure message printed when the condition is false.
     * @return The original condition so callers can accumulate pass/fail state.
     */
    bool expect(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << std::endl;
            return false;
        }

        return true;
    }

    /**
     * @brief Verifies a callable throws std::runtime_error.
     * @param callable Function object expected to throw.
     * @param message Failure message used when no runtime_error is thrown.
     * @return True when runtime_error is thrown.
     */
    template <typename Callable>
    bool expectRuntimeError(Callable callable, const std::string& message)
    {
        try
        {
            callable();
        }
        catch (const std::runtime_error&)
        {
            return true;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "FAILED: " << message << " threw unexpected exception: " << exception.what() << std::endl;
            return false;
        }

        std::cerr << "FAILED: " << message << std::endl;
        return false;
    }

    /**
     * @brief Compares two render colors exactly for deterministic test constants.
     * @param lhs First color.
     * @param rhs Second color.
     * @return True when all color channels match.
     */
    bool colorsEqual(const Render::Color& lhs, const Render::Color& rhs)
    {
        return lhs.red == rhs.red && lhs.green == rhs.green && lhs.blue == rhs.blue && lhs.alpha == rhs.alpha;
    }

    /**
     * @brief Creates a keyboard input event for runtime-driver tests.
     * @param type Engine-facing key event type.
     * @param scancode Physical SDL key code.
     * @param key Virtual SDL key code.
     * @param isRepeat True when the key press is an OS repeat.
     * @return Input event carrying the supplied keyboard payload.
     */
    Platform::InputEvent makeKeyEvent(
        Platform::InputEventType type,
        SDL_Scancode scancode,
        SDL_Keycode key,
        bool isRepeat = false,
        std::uint64_t timestampNanoseconds = 0)
    {
        Platform::InputEvent event{};
        event.type = type;
        event.scancode = scancode;
        event.key = key;
        event.isRepeat = isRepeat;
        event.timestampNanoseconds = timestampNanoseconds;
        return event;
    }

    /**
     * @brief Creates a runtime chart from supplied note data.
     * @param notes Notes to insert into the chart.
     * @return Runtime chart with a 120 BPM timing point.
     */
    Game::Chart makeRuntimeChart(const std::vector<Game::Note>& notes)
    {
        Game::Chart chart{};
        chart.meta.title = "Runtime Driver Test";
        chart.meta.artist = "Raythm";
        chart.meta.creator = "RatherHard";
        chart.meta.audioPath = "charts/test/test.ogg";
        chart.timingPoints.push_back({Game::BeatPosition{0, 1}, 120.0});
        chart.notes = notes;
        chart.laneCount = static_cast<int>(Game::GAME_LANE_COUNT);
        return chart;
    }

    /**
     * @brief Creates one tap note at a normalized beat position.
     * @param beat Normalized beat position.
     * @param lane Zero-based lane column.
     * @return Runtime tap note.
     */
    Game::Note makeTapNoteAtBeat(Game::BeatPosition beat, int lane)
    {
        Game::Note note{};
        note.beat = beat;
        note.column = lane;
        return note;
    }

    /**
     * @brief Creates one tap note at an absolute beat count.
     * @param beatNumerator Normalized beat numerator.
     * @param lane Zero-based lane column.
     * @return Runtime tap note.
     */
    Game::Note makeTapNote(std::int64_t beatNumerator, int lane)
    {
        Game::Note note{};
        note.beat = Game::BeatPosition{beatNumerator, 1};
        note.column = lane;
        return note;
    }

    /**
     * @brief Verifies the driver returns baseline and note commands for an unresolved chart note.
     * @return True when baseline command generation succeeds.
     */
    bool testRuntimeDriverBuildsBaselineAndNoteCommands()
    {
        Core::GameplayRuntimeDriver driver{makeRuntimeChart({makeTapNote(2, 1)})};
        const Platform::InputState inputState{};

        const std::vector<Render::RenderCommand> commands = driver.update(
            std::chrono::milliseconds{0}, inputState, 400, 300);

        bool passed = true;
        passed &= expect(commands.size() > BASE_GAMEPLAY_COMMAND_COUNT, "unresolved note should add a note command");
        passed &= expect(commands[0].bounds.width > 0, "lane command should have positive width");
        passed &= expect(commands[BASE_GAMEPLAY_COMMAND_COUNT].bounds.height > 0,
                         "note command should have positive height");
        return passed;
    }

    /**
     * @brief Verifies a default lane key press resolves the matching gameplay note.
     * @return True when the pressed input flows through mapper, session, and command generation.
     */
    bool testRuntimeDriverJudgesDefaultLaneKeyPress()
    {
        Core::GameplayRuntimeDriver driver{makeRuntimeChart({makeTapNote(2, 0)})};
        Platform::InputState inputState{};
        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_D, SDLK_D));

        const std::vector<Render::RenderCommand> commands = driver.update(
            std::chrono::milliseconds{1000}, inputState, 400, 300);

        bool passed = true;
        passed &= expect(commands.size() == BASE_GAMEPLAY_COMMAND_COUNT, "hit note should disappear from render commands");
        passed &= expect(driver.getGameplaySession().getJudgementCounts().criticalPerfect == 1,
                         "exact D key press should produce Critical Perfect");
        passed &= expect(driver.getGameplaySession().isFinished(), "single hit note should finish gameplay session");
        return passed;
    }

    /**
     * @brief Verifies held platform input changes the corresponding lane visual command.
     * @return True when held lane state reaches the gameplay snapshot and render commands.
     */
    bool testRuntimeDriverReflectsHeldLaneVisuals()
    {
        Core::GameplayRuntimeDriver driver{makeRuntimeChart({makeTapNote(2, 0)})};
        Platform::InputState inputState{};
        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_D, SDLK_D));
        inputState.beginFrame();

        const std::vector<Render::RenderCommand> commands = driver.update(
            std::chrono::milliseconds{500}, inputState, 400, 300);
        const Game::GameplayViewLayout layout = driver.getCurrentViewLayout();

        bool passed = true;
        passed &= expect(colorsEqual(commands[0].color, layout.pressedLaneColor),
                         "held D key should render lane 0 as pressed");
        passed &= expect(commands.size() > BASE_GAMEPLAY_COMMAND_COUNT,
                         "held-only lane should not judge and remove the pending note");
        return passed;
    }

    /**
     * @brief Verifies viewport changes rebuild the runtime view layout.
     * @return True when command geometry follows the supplied viewport.
     */
    bool testRuntimeDriverUpdatesGeometryAfterViewportResize()
    {
        Core::GameplayRuntimeDriver driver{makeRuntimeChart({makeTapNote(2, 2)})};
        const Platform::InputState inputState{};

        const std::vector<Render::RenderCommand> smallCommands = driver.update(
            std::chrono::milliseconds{0}, inputState, 400, 300);
        const Game::GameplayViewLayout smallLayout = driver.getCurrentViewLayout();
        const std::vector<Render::RenderCommand> largeCommands = driver.update(
            std::chrono::milliseconds{0}, inputState, 800, 600);
        const Game::GameplayViewLayout largeLayout = driver.getCurrentViewLayout();

        bool passed = true;
        passed &= expect(smallLayout.viewportWidth == 400 && largeLayout.viewportWidth == 800,
                         "runtime layout should track viewport width changes");
        passed &= expect(smallCommands[0].bounds.width != largeCommands[0].bounds.width,
                         "lane width should change after viewport resize");
        passed &= expect(smallCommands[Game::GAME_LANE_COUNT].bounds.y != largeCommands[Game::GAME_LANE_COUNT].bounds.y,
                         "judgement line y should change after viewport resize");
        return passed;
    }

    /**
     * @brief Verifies timestamped same-lane press events can resolve dense notes in one frame.
     * @return True when the driver forwards explicit press events to GameplaySession.
     */
    bool testRuntimeDriverUsesTimestampedPressEvents()
    {
        Core::GameplayRuntimeDriver driver{makeRuntimeChart({makeTapNote(2, 1), makeTapNoteAtBeat({41, 20}, 1)})};
        Platform::InputState inputState{};
        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_F, SDLK_F));
        const std::vector<Game::LanePressEvent> pressEvents{
            {1, std::chrono::milliseconds{1000}},
            {1, std::chrono::milliseconds{1025}}
        };

        const std::vector<Render::RenderCommand> commands = driver.update(
            std::chrono::milliseconds{1033}, inputState, 400, 300, pressEvents);

        bool passed = true;
        passed &= expect(commands.size() == BASE_GAMEPLAY_COMMAND_COUNT,
                         "two dense same-lane timestamped hits should remove both notes");
        passed &= expect(driver.getGameplaySession().getJudgementCounts().criticalPerfect == 2,
                         "timestamped dense same-lane hits should both be Critical Perfect");
        passed &= expect(driver.getGameplaySession().isFinished(), "both dense same-lane notes should finish");
        return passed;
    }

    /**
     * @brief Verifies platform input events are converted to lane press events only for bound non-repeat keys.
     * @return True when bound, repeat, and unbound key events are classified correctly.
     */
    bool testRuntimeDriverCreatesLanePressEventsForBoundKeys()
    {
        Core::GameplayRuntimeDriver driver{makeRuntimeChart({makeTapNote(2, 0)})};
        const std::optional<Game::LanePressEvent> pressEvent = driver.makeLanePressEvent(
            makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_J, SDLK_J),
            std::chrono::milliseconds{900});
        const std::optional<Game::LanePressEvent> repeatEvent = driver.makeLanePressEvent(
            makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_J, SDLK_J, true),
            std::chrono::milliseconds{900});
        const std::optional<Game::LanePressEvent> unboundEvent = driver.makeLanePressEvent(
            makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_A, SDLK_A),
            std::chrono::milliseconds{900});

        bool passed = true;
        passed &= expect(pressEvent.has_value(), "bound J key should create a lane press event");
        passed &= expect(pressEvent && pressEvent->lane == 2, "J key should map to lane 2");
        passed &= expect(pressEvent && pressEvent->songTime == std::chrono::milliseconds{900},
                         "lane press event should preserve sampled song time");
        passed &= expect(!repeatEvent.has_value(), "repeat key press should not create lane press event");
        passed &= expect(!unboundEvent.has_value(), "unbound key should not create lane press event");
        return passed;
    }

    /**
     * @brief Verifies input event timestamps are mapped back into song time relative to the frame sample.
     * @return True when delayed event processing does not shift judgement timing later.
     */
    bool testRuntimeDriverMapsEventTimestampToSongTime()
    {
        Core::GameplayRuntimeDriver driver{makeRuntimeChart({makeTapNote(2, 1)})};
        const Platform::InputEvent event = makeKeyEvent(
            Platform::InputEventType::KeyPressed,
            SDL_SCANCODE_F,
            SDLK_F,
            false,
            1'950'000'000ULL);
        const std::optional<Game::LanePressEvent> pressEvent = driver.makeLanePressEvent(
            event,
            std::chrono::milliseconds{1050},
            2'000'000'000ULL);

        bool passed = true;
        passed &= expect(pressEvent.has_value(), "timestamped F key should create a lane press event");
        passed &= expect(pressEvent && pressEvent->lane == 1, "F key should map to lane 1");
        passed &= expect(pressEvent && pressEvent->songTime == std::chrono::milliseconds{1000},
                         "event timestamp age should move the press back from frame song time");

        Platform::InputState inputState{};
        const std::vector<Render::RenderCommand> commands = driver.update(
            std::chrono::milliseconds{1050}, inputState, 400, 300, {*pressEvent});
        passed &= expect(commands.size() == BASE_GAMEPLAY_COMMAND_COUNT,
                         "timestamp-adjusted press should hit and remove the pending note");
        passed &= expect(driver.getGameplaySession().getJudgementCounts().criticalPerfect == 1,
                         "timestamp-adjusted press should judge at the exact target time");
        return passed;
    }

    /**
     * @brief Verifies timestamp mapping clamps future and very old input events into valid song time.
     * @return True when invalid timestamp age cannot underflow or exceed frame song time.
     */
    bool testRuntimeDriverClampsTimestampMappedSongTime()
    {
        Core::GameplayRuntimeDriver driver{makeRuntimeChart({makeTapNote(2, 1)})};
        const std::optional<Game::LanePressEvent> futureEvent = driver.makeLanePressEvent(
            makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_F, SDLK_F, false, 2'050'000'000ULL),
            std::chrono::milliseconds{1050},
            2'000'000'000ULL);
        const std::optional<Game::LanePressEvent> oldEvent = driver.makeLanePressEvent(
            makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_F, SDLK_F, false, 500'000'000ULL),
            std::chrono::milliseconds{1050},
            2'000'000'000ULL);
        const std::optional<Game::LanePressEvent> repeatEvent = driver.makeLanePressEvent(
            makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_F, SDLK_F, true, 1'950'000'000ULL),
            std::chrono::milliseconds{1050},
            2'000'000'000ULL);

        bool passed = true;
        passed &= expect(futureEvent && futureEvent->songTime == std::chrono::milliseconds{1050},
                         "future input timestamps should clamp to frame song time");
        passed &= expect(oldEvent && oldEvent->songTime == std::chrono::microseconds{0},
                         "old input timestamps should clamp to song start");
        passed &= expect(!repeatEvent.has_value(), "repeat key press should still be ignored by timestamp mapper");
        return passed;
    }

    /**
     * @brief Verifies invalid viewport dimensions are rejected before command generation.
     * @return True when invalid viewports throw runtime_error.
     */
    bool testRuntimeDriverRejectsInvalidViewport()
    {
        Core::GameplayRuntimeDriver driver{makeRuntimeChart({makeTapNote(2, 0)})};
        const Platform::InputState inputState{};

        bool passed = true;
        passed &= expectRuntimeError(
            [&driver, &inputState]()
            {
                (void)driver.update(std::chrono::milliseconds{0}, inputState, 0, 300);
            },
            "zero viewport width should throw");
        passed &= expectRuntimeError(
            [&driver, &inputState]()
            {
                (void)driver.update(std::chrono::milliseconds{0}, inputState, 400, 0);
            },
            "zero viewport height should throw");
        return passed;
    }

    /**
     * @brief Verifies late song time sweeps unresolved notes into Miss and removes them from commands.
     * @return True when no-input miss lifecycle is visible through the driver.
     */
    bool testRuntimeDriverSweepsMissedNotes()
    {
        Core::GameplayRuntimeDriver driver{makeRuntimeChart({makeTapNote(2, 3)})};
        const Platform::InputState inputState{};

        const std::vector<Render::RenderCommand> commands = driver.update(
            std::chrono::milliseconds{1081}, inputState, 400, 300);

        bool passed = true;
        passed &= expect(commands.size() == BASE_GAMEPLAY_COMMAND_COUNT,
                         "missed note should disappear from render commands");
        passed &= expect(driver.getGameplaySession().getJudgementCounts().miss == 1,
                         "late no-input update should produce one Miss");
        passed &= expect(driver.getGameplaySession().isFinished(), "missed single note should finish gameplay session");
        return passed;
    }
}

/**
 * @brief Runs all Core gameplay runtime driver tests.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Zero when all runtime-driver checks pass; nonzero when any check fails.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    bool allTestsPassed = true;
    allTestsPassed &= testRuntimeDriverBuildsBaselineAndNoteCommands();
    allTestsPassed &= testRuntimeDriverJudgesDefaultLaneKeyPress();
    allTestsPassed &= testRuntimeDriverReflectsHeldLaneVisuals();
    allTestsPassed &= testRuntimeDriverUpdatesGeometryAfterViewportResize();
    allTestsPassed &= testRuntimeDriverUsesTimestampedPressEvents();
    allTestsPassed &= testRuntimeDriverCreatesLanePressEventsForBoundKeys();
    allTestsPassed &= testRuntimeDriverMapsEventTimestampToSongTime();
    allTestsPassed &= testRuntimeDriverClampsTimestampMappedSongTime();
    allTestsPassed &= testRuntimeDriverRejectsInvalidViewport();
    allTestsPassed &= testRuntimeDriverSweepsMissedNotes();

    return allTestsPassed ? 0 : 1;
}
