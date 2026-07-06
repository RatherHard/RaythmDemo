// RaythmDemo - Game Module Tests
// Verifies chart loading, semantic lane input mapping, and gameplay render command adaptation.
// Author: RatherHard
// Date: 2026-07-05

#include "Game/ChartLoader.hpp"
#include "Game/GameplayView.hpp"
#include "Game/LaneInput.hpp"
#include "Platform/EventPump.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    namespace Game = Raythm::Game;
    namespace Platform = Raythm::Platform;
    namespace Render = Raythm::Render;

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
     * @brief Creates a keyboard input event for mapper tests.
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
        bool isRepeat = false
    )
    {
        Platform::InputEvent event{};
        event.type = type;
        event.scancode = scancode;
        event.key = key;
        event.isRepeat = isRepeat;
        return event;
    }

    /**
     * @brief Returns a minimal valid chart JSON document.
     * @return Chart JSON with one BPM and one tap note.
     */
    std::string makeMinimalChartJson()
    {
        return R"json({
            "meta": {
                "title": "Phase 8 Test",
                "artist": "Raythm",
                "creator": "RatherHard",
                "path": "audio/test.ogg",
                "offset": 12
            },
            "time": [
                { "beat": [0, 0, 1], "bpm": 120.0 }
            ],
            "note": [
                { "beat": [1, 0, 1], "column": 2 }
            ]
        })json";
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
     * @brief Verifies minimal chart metadata, timing point, and tap note parsing.
     * @return True when the valid chart is parsed correctly.
     */
    bool testChartLoaderParsesMinimalChart()
    {
        const Game::Chart chart = Game::ChartLoader{}.loadFromJsonText(makeMinimalChartJson());

        bool passed = true;
        passed &= expect(chart.meta.title == "Phase 8 Test", "chart title should be preserved");
        passed &= expect(chart.meta.artist == "Raythm", "chart artist should be preserved");
        passed &= expect(chart.meta.creator == "RatherHard", "chart creator should be preserved");
        passed &= expect(chart.meta.audioPath == "audio/test.ogg", "chart audio path should be preserved");
        passed &= expect(chart.meta.offsetMilliseconds == 12, "chart offset should be preserved");
        passed &= expect(chart.timingPoints.size() == 1U, "minimal chart should have one timing point");
        passed &= expect(chart.timingPoints[0].beat == Game::BeatPosition::fromTuple(0, 0, 1), "first BPM beat should be zero");
        passed &= expect(chart.timingPoints[0].bpm == 120.0, "BPM should be preserved");
        passed &= expect(chart.notes.size() == 1U, "minimal chart should have one note");
        passed &= expect(chart.notes[0].beat == Game::BeatPosition::fromTuple(1, 0, 1), "tap note beat should preserve measure semantics");
        passed &= expect(chart.notes[0].column == 2, "tap note column should be preserved");
        passed &= expect(!chart.notes[0].isHold(), "tap note should not report hold");
        return passed;
    }

    /**
     * @brief Verifies hold notes preserve their end beat.
     * @return True when hold note data is loaded.
     */
    bool testChartLoaderParsesHoldNote()
    {
        const std::string json = R"json({
            "meta": { "title": "Hold", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0 },
            "time": [{ "beat": [0, 0, 1], "bpm": 128 }],
            "note": [{ "beat": [2, 0, 1], "endbeat": [3, 1, 2], "column": 1 }]
        })json";
        const Game::Chart chart = Game::ChartLoader{}.loadFromJsonText(json);

        bool passed = true;
        passed &= expect(chart.notes.size() == 1U, "hold chart should have one note");
        passed &= expect(chart.notes[0].isHold(), "note with endbeat should report hold");
        passed &= expect(chart.notes[0].endBeat == Game::BeatPosition::fromTuple(3, 1, 2), "hold end beat should be preserved");
        return passed;
    }

    /**
     * @brief Verifies converted .mc chart output remains compatible with the runtime loader contract.
     * @return True when converter-style metadata, timing, tap, and hold notes parse correctly.
     */
    bool testChartLoaderParsesConvertedMcChartShape()
    {
        const std::string json = R"json({
            "meta": {
                "title": "Converted Song",
                "artist": "Converted Artist",
                "creator": "Converted Charter",
                "path": "charts/00001/1627802685.ogg",
                "offset": 289
            },
            "time": [
                { "beat": [0, 0, 1], "bpm": 190.00255573050448 }
            ],
            "note": [
                { "beat": [9, 0, 3], "column": 0 },
                { "beat": [9, 0, 3], "endbeat": [10, 0, 3], "column": 3 }
            ]
        })json";
        const Game::Chart chart = Game::ChartLoader{}.loadFromJsonText(json);

        bool passed = true;
        passed &= expect(chart.meta.audioPath == "charts/00001/1627802685.ogg", "converted audio path should be accepted");
        passed &= expect(chart.meta.offsetMilliseconds == 289, "converted offset should be accepted");
        passed &= expect(chart.timingPoints.size() == 1U, "converted chart should keep one BPM entry");
        passed &= expect(chart.notes.size() == 2U, "converted chart should keep playable notes only");
        passed &= expect(!chart.notes[0].isHold(), "converted tap note should remain tap");
        passed &= expect(chart.notes[1].isHold(), "converted hold note should preserve endbeat");
        return passed;
    }

    /**
     * @brief Verifies equivalent beat tuple forms canonicalize to the same position.
     * @return True when equivalent beat forms compare equal and duplicates are detected.
     */
    bool testBeatPositionNormalization()
    {
        bool passed = true;
        passed &= expect(
            Game::BeatPosition::fromTuple(1, 0, 1) == Game::BeatPosition{4, 1},
            "whole measure should normalize to four beats");
        passed &= expect(
            Game::BeatPosition::fromTuple(1, 1, 2) == Game::BeatPosition{6, 1},
            "half measure should normalize to two beats after the measure start");
        passed &= expect(
            Game::BeatPosition::fromTuple(1, 2, 4) == Game::BeatPosition::fromTuple(1, 1, 2),
            "equivalent beat tuple forms should compare equal");
        passed &= expect(
            Game::BeatPosition::fromTuple(1, 0, 1) < Game::BeatPosition::fromTuple(1, 1, 2),
            "beat ordering should use normalized rational values");
        return passed;
    }

    /**
     * @brief Verifies loader sorts timing points and notes by normalized beat position.
     * @return True when chart arrays are ordered in runtime data.
     */
    bool testChartLoaderSortsTimingPointsAndNotes()
    {
        const std::string json = R"json({
            "meta": { "title": "Sort", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0 },
            "time": [
                { "beat": [2, 0, 1], "bpm": 180 },
                { "beat": [0, 0, 1], "bpm": 120 },
                { "beat": [1, 0, 1], "bpm": 150 }
            ],
            "note": [
                { "beat": [3, 0, 1], "column": 1 },
                { "beat": [1, 0, 1], "column": 3 },
                { "beat": [1, 0, 1], "column": 0 }
            ]
        })json";
        const Game::Chart chart = Game::ChartLoader{}.loadFromJsonText(json);

        bool passed = true;
        passed &= expect(chart.timingPoints[0].beat == Game::BeatPosition::fromTuple(0, 0, 1), "timing point 0 should sort first");
        passed &= expect(chart.timingPoints[1].beat == Game::BeatPosition::fromTuple(1, 0, 1), "timing point 1 should sort second");
        passed &= expect(chart.timingPoints[2].beat == Game::BeatPosition::fromTuple(2, 0, 1), "timing point 2 should sort third");
        passed &= expect(chart.notes[0].column == 0, "same-beat notes should sort by column");
        passed &= expect(chart.notes[1].column == 3, "same-beat notes should keep deterministic column ordering");
        passed &= expect(chart.notes[2].beat == Game::BeatPosition::fromTuple(3, 0, 1), "later note should sort last");
        return passed;
    }

    /**
     * @brief Verifies malformed chart documents are rejected.
     * @return True when expected validation failures throw runtime_error.
     */
    bool testChartLoaderRejectsInvalidCharts()
    {
        const Game::ChartLoader loader{};
        bool passed = true;
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText("[]"); }, "non-object chart root should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({"time":[],"note":[]})json"); }, "missing meta should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({"meta":{},"note":[]})json"); }, "missing time should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({"meta":{},"time":[]})json"); }, "missing note should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({
            "meta": { "title": "Bad", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0 },
            "time": [],
            "note": []
        })json"); }, "empty time should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({
            "meta": { "title": "Bad", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0 },
            "time": [{ "beat": [0, 0], "bpm": 120 }],
            "note": []
        })json"); }, "invalid beat array size should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({
            "meta": { "title": "Bad", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0 },
            "time": [{ "beat": [0, 0, 1], "bpm": 120 }],
            "note": [{ "beat": [0, 1, 1], "column": 0 }]
        })json"); }, "beat numerator equal to denominator should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({
            "meta": { "title": "Bad", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0 },
            "time": [{ "beat": [1, 0, 1], "bpm": 120 }],
            "note": []
        })json"); }, "first BPM not at zero should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({
            "meta": { "title": "Bad", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0 },
            "time": [{ "beat": [0, 0, 1], "bpm": 0 }],
            "note": []
        })json"); }, "non-positive BPM should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({
            "meta": { "title": "Bad", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0 },
            "time": [{ "beat": [0, 0, 1], "bpm": 120 }, { "beat": [0, 0, 2], "bpm": 130 }],
            "note": []
        })json"); }, "duplicate normalized BPM beat should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({
            "meta": { "title": "Bad", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0 },
            "time": [{ "beat": [0, 0, 1], "bpm": 120 }],
            "note": [{ "beat": [1, 0, 1], "column": 4 }]
        })json"); }, "out-of-range column should throw");
        passed &= expectRuntimeError([&loader]() { (void)loader.loadFromJsonText(R"json({
            "meta": { "title": "Bad", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0 },
            "time": [{ "beat": [0, 0, 1], "bpm": 120 }],
            "note": [{ "beat": [1, 0, 1], "endbeat": [1, 0, 1], "column": 0 }]
        })json"); }, "endbeat not greater than beat should throw");
        return passed;
    }

    /**
     * @brief Verifies extra fields are ignored and chart files can be loaded from disk.
     * @return True when tolerant parsing and file loading work.
     */
    bool testChartLoaderIgnoresUnknownFieldsAndLoadsFile()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path directory = std::filesystem::temp_directory_path()
            / ("raythm_game_chart_loader_test_" + std::to_string(suffix));
        const std::filesystem::path path = directory / "root" / "chart.json";
        const std::filesystem::path outsidePath = directory / "outside.json";

        struct TempDirectoryCleanup
        {
            std::filesystem::path directory;

            ~TempDirectoryCleanup()
            {
                std::error_code error;
                std::filesystem::remove_all(directory, error);
            }
        } cleanup{directory};

        std::filesystem::create_directories(path.parent_path());
        bool passed = true;
        {
            std::ofstream file(path, std::ios::out | std::ios::trunc);
            file << R"json({
                "meta": { "title": "File", "artist": "A", "creator": "C", "path": "a.ogg", "offset": 0, "extra": true },
                "time": [{ "beat": [0, 0, 1], "bpm": 120, "extra": true }],
                "note": [{ "beat": [1, 0, 1], "column": 0, "extra": true }],
                "unknown": 123
            })json";
        }

        Game::ChartLoadOptions loadOptions{};
        loadOptions.assetRoot = path.parent_path();
        const Game::Chart chart = Game::ChartLoader{loadOptions}.loadFromFile(path.filename().string());
        passed &= expect(chart.meta.title == "File", "loadFromFile should parse chart metadata");
        passed &= expect(chart.notes.size() == 1U, "unknown fields should not prevent note parsing");
        passed &= expectRuntimeError(
            [&path]()
            {
                Game::ChartLoadOptions options{};
                options.assetRoot = path.parent_path();
                options.maxFileSizeBytes = 8;
                (void)Game::ChartLoader{options}.loadFromFile(path.filename().string());
            },
            "file larger than configured limit should throw");
        passed &= expectRuntimeError(
            []()
            {
                Game::ChartLoadOptions options{};
                options.maxFileSizeBytes = 8;
                (void)Game::ChartLoader{options}.loadFromJsonText(makeMinimalChartJson());
            },
            "JSON text larger than configured limit should throw");
        {
            std::ofstream outsideFile(outsidePath, std::ios::out | std::ios::trunc);
            outsideFile << makeMinimalChartJson();
        }
        passed &= expectRuntimeError(
            [&path]()
            {
                Game::ChartLoadOptions options{};
                options.assetRoot = path.parent_path();
                (void)Game::ChartLoader{options}.loadFromFile(std::string("../outside.json"));
            },
            "path escaping configured asset root should throw");
        return passed;
    }

    /**
     * @brief Verifies default lane bindings and mapping validation.
     * @return True when default and invalid key maps behave as expected.
     */
    bool testLaneInputMapperBindings()
    {
        const Game::LaneInputMapper mapper{};
        const Game::LaneKeyMap defaultMap = mapper.getKeyMap();

        bool passed = true;
        passed &= expect(defaultMap.laneScancodes[0] == SDL_SCANCODE_D, "lane 0 should default to D");
        passed &= expect(defaultMap.laneScancodes[1] == SDL_SCANCODE_F, "lane 1 should default to F");
        passed &= expect(defaultMap.laneScancodes[2] == SDL_SCANCODE_J, "lane 2 should default to J");
        passed &= expect(defaultMap.laneScancodes[3] == SDL_SCANCODE_K, "lane 3 should default to K");

        Game::LaneKeyMap duplicateMap = defaultMap;
        duplicateMap.laneScancodes[1] = SDL_SCANCODE_D;
        passed &= expectRuntimeError([duplicateMap]() { (void)Game::LaneInputMapper{duplicateMap}; }, "duplicate lane binding should throw");

        Game::LaneKeyMap unknownMap = defaultMap;
        unknownMap.laneScancodes[2] = SDL_SCANCODE_UNKNOWN;
        passed &= expectRuntimeError([unknownMap]() { (void)Game::LaneInputMapper{unknownMap}; }, "unknown lane binding should throw");
        return passed;
    }

    /**
     * @brief Verifies lane snapshots forward held, pressed, released, and repeat semantics from InputState.
     * @return True when mapped lane state matches platform input state.
     */
    bool testLaneInputMapperSamplesInputState()
    {
        Platform::InputState inputState{};
        const Game::LaneInputMapper mapper{};
        bool passed = true;

        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_D, SDLK_D));
        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_J, SDLK_J));
        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_A, SDLK_A));
        Game::LaneInputSnapshot snapshot = mapper.sample(inputState);
        passed &= expect(snapshot.lanes[0].isDown && snapshot.lanes[0].wasPressed, "lane 0 should reflect D press");
        passed &= expect(snapshot.lanes[2].isDown && snapshot.lanes[2].wasPressed, "lane 2 should reflect J press");
        passed &= expect(!snapshot.lanes[1].isDown && !snapshot.lanes[3].isDown, "unpressed lanes should stay empty");

        inputState.beginFrame();
        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_D, SDLK_D, true));
        snapshot = mapper.sample(inputState);
        passed &= expect(snapshot.lanes[0].isDown, "repeat should keep lane down");
        passed &= expect(!snapshot.lanes[0].wasPressed, "repeat should not retrigger lane press");

        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyReleased, SDL_SCANCODE_F, SDLK_F));
        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyPressed, SDL_SCANCODE_F, SDLK_F));
        inputState.handleInputEvent(makeKeyEvent(Platform::InputEventType::KeyReleased, SDL_SCANCODE_F, SDLK_F));
        snapshot = mapper.sample(inputState);
        passed &= expect(!snapshot.lanes[1].isDown, "same-frame tap lane should finish up");
        passed &= expect(snapshot.lanes[1].wasPressed, "same-frame tap lane should preserve pressed edge");
        passed &= expect(snapshot.lanes[1].wasReleased, "same-frame tap lane should preserve released edge");
        return passed;
    }

    /**
     * @brief Verifies empty gameplay snapshots render lane backgrounds and judgement line.
     * @return True when baseline command output matches layout.
     */
    bool testGameplayViewAdapterBuildsBaselineCommands()
    {
        Game::GameplayViewLayout layout{};
        layout.viewportWidth = 400;
        layout.viewportHeight = 300;
        layout.playfieldLeft = 40;
        layout.playfieldTop = 20;
        layout.playfieldWidth = 200;
        layout.playfieldHeight = 240;
        layout.laneGap = 4;
        layout.judgementLineThickness = 6;
        const Game::GameplayViewAdapter adapter{layout};

        Game::GameplaySnapshot snapshot{};
        snapshot.lanes[2].isPressed = true;
        const auto commands = adapter.buildCommands(snapshot);

        bool passed = true;
        passed &= expect(commands.size() == 5U, "empty snapshot should produce four lanes and judgement line");
        passed &= expect(commands[0].bounds.x == 40 && commands[0].bounds.width == 47, "lane 0 bounds should match layout");
        passed &= expect(colorsEqual(commands[2].color, layout.pressedLaneColor), "pressed lane should use pressed color");
        passed &= expect(commands[4].bounds.y == 221 && commands[4].bounds.height == 6, "judgement line should use normalized y");
        return passed;
    }

    /**
     * @brief Verifies visible notes convert to clipped render rectangles.
     * @return True when note commands match deterministic geometry and colors.
     */
    bool testGameplayViewAdapterBuildsNoteCommands()
    {
        Game::GameplayViewLayout layout{};
        layout.viewportWidth = 400;
        layout.viewportHeight = 300;
        layout.playfieldLeft = 40;
        layout.playfieldTop = 20;
        layout.playfieldWidth = 200;
        layout.playfieldHeight = 240;
        layout.laneGap = 4;
        const Game::GameplayViewAdapter adapter{layout};

        Game::GameplaySnapshot snapshot{};
        snapshot.visibleNotes.push_back({1, 0.25F, 0.10F, false});
        snapshot.visibleNotes.push_back({3, -0.05F, 0.10F, true});
        snapshot.visibleNotes.push_back({0, 1.10F, 0.10F, false});
        const auto commands = adapter.buildCommands(snapshot);

        bool passed = true;
        passed &= expect(commands.size() == 7U, "two visible notes should be emitted after five baseline commands");
        passed &= expect(commands[5].bounds.x == 91 && commands[5].bounds.y == 80, "tap note should use lane 1 and normalized top");
        passed &= expect(commands[5].bounds.width == 47 && commands[5].bounds.height == 24, "tap note size should match lane and normalized height");
        passed &= expect(colorsEqual(commands[5].color, layout.tapNoteColor), "tap note should use tap color");
        passed &= expect(commands[6].bounds.y == 20 && commands[6].bounds.height == 12, "partially visible note should clip to playfield top");
        passed &= expect(colorsEqual(commands[6].color, layout.holdNoteColor), "hold note should use hold color");
        return passed;
    }

    /**
     * @brief Verifies invalid gameplay view input is rejected.
     * @return True when invalid layout and snapshot values throw runtime_error.
     */
    bool testGameplayViewAdapterRejectsInvalidInput()
    {
        bool passed = true;
        Game::GameplayViewLayout invalidLayout{};
        invalidLayout.playfieldWidth = 0;
        passed &= expectRuntimeError([invalidLayout]() { (void)Game::GameplayViewAdapter{invalidLayout}; }, "invalid layout should throw");

        const Game::GameplayViewAdapter adapter{};
        Game::GameplaySnapshot invalidLane{};
        invalidLane.visibleNotes.push_back({4, 0.1F, 0.1F, false});
        passed &= expectRuntimeError([&adapter, invalidLane]() { (void)adapter.buildCommands(invalidLane); }, "invalid lane should throw");

        Game::GameplaySnapshot invalidHeight{};
        invalidHeight.visibleNotes.push_back({0, 0.1F, -0.1F, false});
        passed &= expectRuntimeError([&adapter, invalidHeight]() { (void)adapter.buildCommands(invalidHeight); }, "negative note height should throw");

        Game::GameplaySnapshot invalidNumber{};
        invalidNumber.judgementLineY = std::nanf("");
        passed &= expectRuntimeError([&adapter, invalidNumber]() { (void)adapter.buildCommands(invalidNumber); }, "NaN judgement line should throw");
        return passed;
    }
}

/**
 * @brief Runs all Game module tests.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Zero when all Game checks pass; nonzero when any check fails.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    bool allTestsPassed = true;
    allTestsPassed &= testChartLoaderParsesMinimalChart();
    allTestsPassed &= testChartLoaderParsesHoldNote();
    allTestsPassed &= testChartLoaderParsesConvertedMcChartShape();
    allTestsPassed &= testBeatPositionNormalization();
    allTestsPassed &= testChartLoaderSortsTimingPointsAndNotes();
    allTestsPassed &= testChartLoaderRejectsInvalidCharts();
    allTestsPassed &= testChartLoaderIgnoresUnknownFieldsAndLoadsFile();
    allTestsPassed &= testLaneInputMapperBindings();
    allTestsPassed &= testLaneInputMapperSamplesInputState();
    allTestsPassed &= testGameplayViewAdapterBuildsBaselineCommands();
    allTestsPassed &= testGameplayViewAdapterBuildsNoteCommands();
    allTestsPassed &= testGameplayViewAdapterRejectsInvalidInput();

    return allTestsPassed ? 0 : 1;
}
