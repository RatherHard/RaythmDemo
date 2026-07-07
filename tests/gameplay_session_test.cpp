// RaythmDemo - Gameplay Session Tests
// Verifies pure tap judgement, note lifecycle, and snapshot generation.
// Author: RatherHard
// Date: 2026-07-07

#include "Game/GameplaySession.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    namespace Game = Raythm::Game;

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
     * @brief Counts every judgement event category in a result counter.
     * @param counts Judgement counts to inspect.
     * @return Total resolved note count represented by the counter.
     */
    int getTotalJudgementCount(const Game::JudgementCounts& counts)
    {
        return counts.criticalPerfect + counts.normalPerfect + counts.great + counts.bad + counts.miss;
    }

    /**
     * @brief Creates a lane input snapshot with one pressed lane edge.
     * @param lane Zero-based lane to press.
     * @param isHeld True when the lane should also render as held.
     * @return Semantic lane input snapshot for one update.
     */
    Game::LaneInputSnapshot makePressedLaneSnapshot(int lane, bool isHeld = true)
    {
        Game::LaneInputSnapshot input{};
        input.lanes.at(static_cast<std::size_t>(lane)).isDown = isHeld;
        input.lanes.at(static_cast<std::size_t>(lane)).wasPressed = true;
        return input;
    }

    /**
     * @brief Creates a lane input snapshot with one held lane and no pressed edge.
     * @param lane Zero-based lane to hold.
     * @return Semantic lane input snapshot without a tap edge.
     */
    Game::LaneInputSnapshot makeHeldLaneSnapshot(int lane)
    {
        Game::LaneInputSnapshot input{};
        input.lanes.at(static_cast<std::size_t>(lane)).isDown = true;
        return input;
    }

    /**
     * @brief Creates a test chart from prevalidated runtime note data.
     * @param notes Notes to insert into the chart.
     * @param offsetMilliseconds Chart metadata offset in milliseconds.
     * @param timingPoints Timing points to use, or a default 120 BPM point when empty.
     * @return Runtime chart suitable for pure gameplay session tests.
     */
    Game::Chart makeRuntimeChart(
        const std::vector<Game::Note>& notes,
        int offsetMilliseconds = 0,
        std::vector<Game::TimingPoint> timingPoints = {})
    {
        if (timingPoints.empty())
        {
            timingPoints.push_back({Game::BeatPosition{0, 1}, 120.0});
        }

        Game::Chart chart{};
        chart.meta.title = "Session Test";
        chart.meta.artist = "Raythm";
        chart.meta.creator = "RatherHard";
        chart.meta.audioPath = "audio/test.ogg";
        chart.meta.offsetMilliseconds = offsetMilliseconds;
        chart.timingPoints = timingPoints;
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
     * @brief Creates one hold note whose head can be judged in tap-only session tests.
     * @param beatNumerator Normalized start beat numerator.
     * @param lane Zero-based lane column.
     * @return Runtime hold note.
     */
    Game::Note makeHoldNote(std::int64_t beatNumerator, int lane)
    {
        Game::Note note = makeTapNote(beatNumerator, lane);
        note.endBeat = Game::BeatPosition{beatNumerator + 1, 1};
        return note;
    }

    /**
     * @brief Creates a single-tap gameplay session at a 1000ms target time.
     * @param lane Zero-based lane containing the note.
     * @return Session with one unresolved tap note.
     */
    Game::GameplaySession makeSingleTapSession(int lane = 1)
    {
        return Game::GameplaySession{makeRuntimeChart({makeTapNote(2, lane)})};
    }

    /**
     * @brief Updates a session with one lane press at a millisecond timestamp.
     * @param session Session to update.
     * @param timeMilliseconds Current song time in milliseconds.
     * @param lane Zero-based pressed lane.
     */
    void pressLaneAt(Game::GameplaySession& session, int timeMilliseconds, int lane)
    {
        session.update(std::chrono::milliseconds{timeMilliseconds}, makePressedLaneSnapshot(lane));
    }

    /**
     * @brief Updates a session with timestamped lane press events at a frame boundary.
     * @param session Session to update.
     * @param frameTimeMilliseconds Current frame song time in milliseconds.
     * @param events Timestamped lane press events to feed.
     */
    void updateWithPressEvents(
        Game::GameplaySession& session,
        int frameTimeMilliseconds,
        const std::vector<Game::LanePressEvent>& events)
    {
        Game::LaneInputSnapshot input{};
        for (const Game::LanePressEvent& event : events)
        {
            input.lanes.at(static_cast<std::size_t>(event.lane)).isDown = true;
            input.lanes.at(static_cast<std::size_t>(event.lane)).wasPressed = true;
        }

        session.update(std::chrono::milliseconds{frameTimeMilliseconds}, input, events);
    }

    /**
     * @brief Verifies the last session event has the expected judgement result.
     * @param session Session whose event history is inspected.
     * @param result Expected judgement category.
     * @param message Failure message used when the result differs.
     * @return True when the last event exists and matches.
     */
    bool expectLastJudgement(
        const Game::GameplaySession& session,
        Game::JudgementResult result,
        const std::string& message)
    {
        const std::vector<Game::JudgementEvent>& events = session.getJudgementEvents();
        if (events.empty())
        {
            std::cerr << "FAILED: " << message << " produced no judgement event" << std::endl;
            return false;
        }

        return expect(events.back().result == result, message);
    }

    /**
     * @brief Verifies a single-tap session classifies one press as expected.
     * @param deltaMilliseconds Input delta relative to the 1000ms target.
     * @param expected Expected tap judgement category.
     * @param message Failure message used when classification differs.
     * @return True when exactly one matching event is emitted.
     */
    bool expectSingleTapDeltaJudgement(
        int deltaMilliseconds,
        Game::JudgementResult expected,
        const std::string& message)
    {
        Game::GameplaySession session = makeSingleTapSession();
        pressLaneAt(session, 1000 + deltaMilliseconds, 1);

        bool passed = true;
        passed &= expect(session.getJudgementEvents().size() == 1U, message + " should emit one event");
        passed &= expectLastJudgement(session, expected, message);
        passed &= expect(session.isFinished(), message + " should resolve the only note");
        return passed;
    }

    /**
     * @brief Verifies an empty chart session is stable and finished without input.
     * @return True when an empty chart produces no judgements and no notes.
     */
    bool testGameplaySessionNoOpWithEmptyChart()
    {
        Game::GameplaySession session{makeRuntimeChart({})};
        session.update(std::chrono::milliseconds{250}, Game::LaneInputSnapshot{});
        session.update(std::chrono::milliseconds{1000}, makePressedLaneSnapshot(0));

        bool passed = true;
        passed &= expect(session.isFinished(), "empty session should be finished immediately");
        passed &= expect(session.getJudgementEvents().empty(), "empty session should not emit judgements");
        passed &= expect(session.getSnapshot().visibleNotes.empty(), "empty session should not expose visible notes");
        return passed;
    }

    /**
     * @brief Verifies exact tap timing produces Critical Perfect and consumes the note.
     * @return True when a target-time press resolves the note once.
     */
    bool testGameplaySessionJudgesCriticalPerfectOnExactHit()
    {
        Game::GameplaySession session = makeSingleTapSession();
        pressLaneAt(session, 1000, 1);

        bool passed = true;
        passed &= expectLastJudgement(session, Game::JudgementResult::CriticalPerfect, "exact hit should be Critical Perfect");
        passed &= expect(session.getJudgementCounts().criticalPerfect == 1, "Critical Perfect count should increment");
        passed &= expect(session.getCombo() == 1, "combo should increment after a hit");
        passed &= expect(session.getSnapshot().visibleNotes.empty(), "hit note should disappear from snapshot");
        passed &= expect(session.isFinished(), "single hit note session should finish");
        return passed;
    }

    /**
     * @brief Verifies tap judgement timing window boundaries.
     * @return True when documented boundary values map to expected categories.
     */
    bool testGameplaySessionJudgesTapWindowBoundaries()
    {
        bool passed = true;
        passed &= expectSingleTapDeltaJudgement(-20, Game::JudgementResult::CriticalPerfect, "-20ms should be Critical Perfect");
        passed &= expectSingleTapDeltaJudgement(20, Game::JudgementResult::CriticalPerfect, "+20ms should be Critical Perfect");
        passed &= expectSingleTapDeltaJudgement(-21, Game::JudgementResult::NormalPerfect, "-21ms should be Normal Perfect");
        passed &= expectSingleTapDeltaJudgement(-40, Game::JudgementResult::NormalPerfect, "-40ms should be Normal Perfect");
        passed &= expectSingleTapDeltaJudgement(21, Game::JudgementResult::NormalPerfect, "+21ms should be Normal Perfect");
        passed &= expectSingleTapDeltaJudgement(40, Game::JudgementResult::NormalPerfect, "+40ms should be Normal Perfect");
        passed &= expectSingleTapDeltaJudgement(-41, Game::JudgementResult::Great, "-41ms should be Great");
        passed &= expectSingleTapDeltaJudgement(-80, Game::JudgementResult::Great, "-80ms should be Great");
        passed &= expectSingleTapDeltaJudgement(41, Game::JudgementResult::Great, "+41ms should be Great");
        passed &= expectSingleTapDeltaJudgement(80, Game::JudgementResult::Great, "+80ms should be Great");
        passed &= expectSingleTapDeltaJudgement(-120, Game::JudgementResult::Bad, "-120ms should be Bad");
        passed &= expectSingleTapDeltaJudgement(-81, Game::JudgementResult::Bad, "-81ms should be Bad");
        return passed;
    }

    /**
     * @brief Verifies too-early inputs do not consume notes and late expiry produces Miss.
     * @return True when early and miss lifecycle rules are enforced.
     */
    bool testGameplaySessionHandlesEarlyInputAndAutoMiss()
    {
        Game::GameplaySession earlySession = makeSingleTapSession();
        pressLaneAt(earlySession, 879, 1);
        bool passed = true;
        passed &= expect(earlySession.getJudgementEvents().empty(), "-121ms press should not emit judgement");
        passed &= expect(!earlySession.isFinished(), "too-early press should leave note pending");
        pressLaneAt(earlySession, 1000, 1);
        passed &= expectLastJudgement(earlySession, Game::JudgementResult::CriticalPerfect, "pending note should still be hittable later");

        Game::GameplaySession missSession = makeSingleTapSession();
        missSession.update(std::chrono::milliseconds{1081}, Game::LaneInputSnapshot{});
        passed &= expectLastJudgement(missSession, Game::JudgementResult::Miss, "+81ms should auto-miss unresolved note");
        passed &= expect(missSession.getJudgementCounts().miss == 1, "Miss count should increment");
        passed &= expect(missSession.getCombo() == 0, "Miss should reset combo");
        passed &= expect(missSession.isFinished(), "missed single note should finish session");
        return passed;
    }

    /**
     * @brief Verifies wrong lanes and held-only lanes do not trigger tap hits.
     * @return True when only matching pressed edges consume notes.
     */
    bool testGameplaySessionRequiresMatchingPressedLaneEdge()
    {
        Game::GameplaySession wrongLaneSession = makeSingleTapSession(2);
        pressLaneAt(wrongLaneSession, 1000, 1);
        bool passed = true;
        passed &= expect(wrongLaneSession.getJudgementEvents().empty(), "wrong lane should not judge note");
        passed &= expect(!wrongLaneSession.isFinished(), "wrong-lane press should not consume note");
        pressLaneAt(wrongLaneSession, 1000, 2);
        passed &= expectLastJudgement(wrongLaneSession, Game::JudgementResult::CriticalPerfect, "correct lane should judge note");

        Game::GameplaySession heldOnlySession = makeSingleTapSession(0);
        heldOnlySession.update(std::chrono::milliseconds{1000}, makeHeldLaneSnapshot(0));
        passed &= expect(heldOnlySession.getJudgementEvents().empty(), "held-only lane should not judge tap note");
        passed &= expect(heldOnlySession.getSnapshot().lanes[0].isPressed, "held-only lane should still affect visual snapshot");
        pressLaneAt(heldOnlySession, 1000, 0);
        passed &= expectLastJudgement(heldOnlySession, Game::JudgementResult::CriticalPerfect, "pressed edge should judge after held-only frame");
        return passed;
    }

    /**
     * @brief Verifies resolved notes are not judged twice.
     * @return True when repeated updates do not duplicate events for the same note.
     */
    bool testGameplaySessionDoesNotDoubleJudgeSameNote()
    {
        Game::GameplaySession session = makeSingleTapSession();
        const Game::LaneInputSnapshot input = makePressedLaneSnapshot(1);
        session.update(std::chrono::milliseconds{1000}, input);
        session.update(std::chrono::milliseconds{1000}, input);

        bool passed = true;
        passed &= expect(session.getJudgementEvents().size() == 1U, "same note should emit exactly one judgement");
        passed &= expect(session.getJudgementCounts().criticalPerfect == 1, "Critical Perfect count should remain one");
        return passed;
    }

    /**
     * @brief Verifies same-lane notes resolve in chronological order.
     * @return True when two notes on one lane are consumed independently.
     */
    bool testGameplaySessionJudgesSameLaneNotesInOrder()
    {
        Game::GameplaySession session{makeRuntimeChart({makeTapNote(2, 1), makeTapNote(4, 1)})};
        pressLaneAt(session, 1000, 1);
        bool passed = true;
        passed &= expect(session.getJudgementEvents().size() == 1U, "first same-lane press should emit one event");
        passed &= expect(session.getJudgementEvents().back().noteIndex == 0U, "first event should target first note");
        passed &= expect(!session.isFinished(), "second note should remain pending");
        pressLaneAt(session, 2000, 1);
        passed &= expect(session.getJudgementEvents().size() == 2U, "second same-lane press should emit another event");
        passed &= expect(session.getJudgementEvents().back().noteIndex == 1U, "second event should target second note");
        passed &= expect(session.isFinished(), "both same-lane notes should resolve");
        return passed;
    }

    /**
     * @brief Verifies chord notes on different lanes can judge in one update.
     * @return True when same-time notes remain lane-independent.
     */
    bool testGameplaySessionSupportsChordJudgementAcrossDifferentLanes()
    {
        Game::GameplaySession session{makeRuntimeChart({makeTapNote(2, 0), makeTapNote(2, 3)})};
        Game::LaneInputSnapshot input{};
        input.lanes[0].isDown = true;
        input.lanes[0].wasPressed = true;
        input.lanes[3].isDown = true;
        input.lanes[3].wasPressed = true;
        session.update(std::chrono::milliseconds{1000}, input);

        bool passed = true;
        passed &= expect(session.getJudgementEvents().size() == 2U, "chord press should judge both lanes");
        passed &= expect(session.getJudgementCounts().criticalPerfect == 2, "both chord notes should be Critical Perfect");
        passed &= expect(session.getCombo() == 2, "combo should count both chord hits");
        passed &= expect(session.isFinished(), "chord session should finish after both hits");
        return passed;
    }

    /**
     * @brief Verifies chart offset shifts the note target time.
     * @return True when positive offset makes notes occur earlier in audio playback time.
     */
    bool testGameplaySessionAppliesChartOffsetToTargetTime()
    {
        Game::GameplaySession session{makeRuntimeChart({makeTapNote(2, 1)}, 50)};
        pressLaneAt(session, 829, 1);
        bool passed = true;
        passed &= expect(session.getJudgementEvents().empty(), "pre-offset target before Bad window should be too early and not judge");
        pressLaneAt(session, 950, 1);
        passed &= expectLastJudgement(session, Game::JudgementResult::CriticalPerfect, "offset target should judge exactly at shifted time");
        return passed;
    }

    /**
     * @brief Verifies BPM changes are used when resolving note target times.
     * @return True when notes before and after a timing point judge at computed times.
     */
    bool testGameplaySessionConvertsBeatToHitTimeUsingTimingPoints()
    {
        std::vector<Game::TimingPoint> timingPoints{
            {Game::BeatPosition{0, 1}, 120.0},
            {Game::BeatPosition{4, 1}, 60.0}
        };
        Game::GameplaySession session{makeRuntimeChart({makeTapNote(2, 0), makeTapNote(5, 1)}, 0, timingPoints)};

        pressLaneAt(session, 1000, 0);
        bool passed = true;
        passed &= expectLastJudgement(session, Game::JudgementResult::CriticalPerfect, "pre-BPM-change note should hit at 1000ms");
        pressLaneAt(session, 3000, 1);
        passed &= expectLastJudgement(session, Game::JudgementResult::CriticalPerfect, "post-BPM-change note should hit at 3000ms");
        passed &= expect(session.isFinished(), "both BPM-segment notes should resolve");
        return passed;
    }

    /**
     * @brief Verifies hold-note heads use tap judgement in the first session slice.
     * @return True when the hold head judges and appears as a hold visual while pending.
     */
    bool testGameplaySessionTreatsHoldHeadAsTapInInitialSlice()
    {
        Game::GameplaySession session{makeRuntimeChart({makeHoldNote(2, 2)})};
        bool passed = true;
        passed &= expect(session.getSnapshot().visibleNotes.size() == 1U, "pending hold should appear in snapshot");
        passed &= expect(session.getSnapshot().visibleNotes[0].isHold, "pending hold should preserve hold visual flag");
        pressLaneAt(session, 1000, 2);
        passed &= expectLastJudgement(session, Game::JudgementResult::CriticalPerfect, "hold head should judge as tap");
        passed &= expect(session.getSnapshot().visibleNotes.empty(), "judged hold head should disappear in tap-only slice");
        return passed;
    }

    /**
     * @brief Verifies snapshot data tracks unresolved notes and lane held state.
     * @return True when visible notes disappear only after resolution.
     */
    bool testGameplaySessionSnapshotTracksVisibleNotesAndPressedLanes()
    {
        Game::GameplaySession session{makeRuntimeChart({makeTapNote(2, 0), makeTapNote(4, 1)})};
        bool passed = true;
        passed &= expect(session.getSnapshot().visibleNotes.size() == 2U, "initial snapshot should include unresolved notes");
        session.update(std::chrono::milliseconds{1000}, makePressedLaneSnapshot(0));
        passed &= expect(session.getSnapshot().lanes[0].isPressed, "pressed lane should be reflected in snapshot");
        passed &= expect(session.getSnapshot().visibleNotes.size() == 1U, "hit note should be removed from snapshot");
        passed &= expect(session.getSnapshot().visibleNotes[0].lane == 1, "remaining visible note should stay on lane 1");
        return passed;
    }

    /**
     * @brief Verifies combo increases on hits and resets on miss.
     * @return True when combo state follows hit and miss lifecycle.
     */
    bool testGameplaySessionComboIncrementsAndResetsOnMiss()
    {
        Game::GameplaySession session{makeRuntimeChart({makeTapNote(2, 0), makeTapNote(4, 1), makeTapNote(6, 2)})};
        pressLaneAt(session, 1000, 0);
        pressLaneAt(session, 2000, 1);
        bool passed = true;
        passed &= expect(session.getCombo() == 2, "combo should increment across consecutive hits");
        session.update(std::chrono::milliseconds{3081}, Game::LaneInputSnapshot{});
        passed &= expectLastJudgement(session, Game::JudgementResult::Miss, "third note should miss after late window");
        passed &= expect(session.getCombo() == 0, "miss should reset combo");
        passed &= expect(getTotalJudgementCount(session.getJudgementCounts()) == 3, "all three notes should be counted");
        return passed;
    }

    /**
     * @brief Verifies timestamped press events can resolve dense same-lane notes in one frame.
     * @return True when multiple press events on one lane judge multiple pending notes.
     */
    bool testGameplaySessionTimestampedEventsJudgeDenseSameLaneNotes()
    {
        Game::GameplaySession session{makeRuntimeChart({makeTapNote(2, 1), makeTapNoteAtBeat({41, 20}, 1)})};
        updateWithPressEvents(
            session,
            1033,
            {
                {1, std::chrono::milliseconds{1000}},
                {1, std::chrono::milliseconds{1025}}
            });

        bool passed = true;
        passed &= expect(session.getJudgementEvents().size() == 2U, "two timestamped same-lane presses should emit two events");
        passed &= expect(session.getJudgementCounts().criticalPerfect == 2, "dense same-lane notes should judge as exact hits");
        passed &= expect(session.isFinished(), "dense same-lane notes should both resolve");
        return passed;
    }

    /**
     * @brief Verifies timestamped events are judged before frame-boundary miss sweeping.
     * @return True when a late-but-valid event processed after the frame boundary still hits.
     */
    bool testGameplaySessionTimestampedEventCanHitBeforeFrameBoundaryMiss()
    {
        Game::GameplaySession session = makeSingleTapSession(1);
        updateWithPressEvents(session, 1081, {{1, std::chrono::milliseconds{1079}}});

        bool passed = true;
        passed &= expectLastJudgement(session, Game::JudgementResult::Great, "+79ms timestamped press should hit before +81ms frame miss sweep");
        passed &= expect(session.getJudgementCounts().miss == 0, "valid timestamped press should not also miss");
        passed &= expect(session.isFinished(), "timestamped hit should resolve note");
        return passed;
    }

    /**
     * @brief Verifies invalid timestamped event batches are rejected before mutating session state.
     * @return True when future-dated and out-of-order events throw without consuming notes.
     */
    bool testGameplaySessionRejectsInvalidTimestampedEventBatches()
    {
        bool passed = true;
        Game::GameplaySession futureEventSession = makeSingleTapSession(1);
        passed &= expectRuntimeError(
            [&futureEventSession]()
            {
                updateWithPressEvents(futureEventSession, 1000, {{1, std::chrono::milliseconds{1001}}});
            },
            "future timestamped press event should throw");
        passed &= expect(futureEventSession.getJudgementEvents().empty(), "future event rejection should not mutate session events");

        Game::GameplaySession unorderedEventSession{makeRuntimeChart({makeTapNote(2, 1), makeTapNoteAtBeat({41, 20}, 1)})};
        passed &= expectRuntimeError(
            [&unorderedEventSession]()
            {
                updateWithPressEvents(
                    unorderedEventSession,
                    1033,
                    {
                        {1, std::chrono::milliseconds{1025}},
                        {1, std::chrono::milliseconds{1000}}
                    });
            },
            "out-of-order same-lane timestamped press events should throw");
        passed &= expect(unorderedEventSession.getJudgementEvents().empty(), "out-of-order event rejection should not mutate session events");
        return passed;
    }

    /**
     * @brief Verifies session construction rejects invalid direct chart data and config.
     * @return True when invalid runtime inputs throw runtime_error.
     */
    bool testGameplaySessionRejectsInvalidRuntimeInputs()
    {
        bool passed = true;
        Game::Chart invalidLaneChart = makeRuntimeChart({makeTapNote(2, 0)});
        invalidLaneChart.laneCount = 5;
        passed &= expectRuntimeError([invalidLaneChart]() { (void)Game::GameplaySession{invalidLaneChart}; }, "invalid lane count should throw");

        Game::Chart unsortedTimingChart = makeRuntimeChart({makeTapNote(2, 0)});
        unsortedTimingChart.timingPoints = {{Game::BeatPosition{0, 1}, 120.0}, {Game::BeatPosition{0, 1}, 130.0}};
        passed &= expectRuntimeError([unsortedTimingChart]() { (void)Game::GameplaySession{unsortedTimingChart}; }, "duplicate timing point beats should throw");

        Game::Chart zeroDenominatorChart = makeRuntimeChart({makeTapNote(2, 0)});
        zeroDenominatorChart.notes[0].beat.denominator = 0;
        passed &= expectRuntimeError([zeroDenominatorChart]() { (void)Game::GameplaySession{zeroDenominatorChart}; }, "zero note beat denominator should throw");

        Game::Chart negativeBeatChart = makeRuntimeChart({makeTapNote(-1, 0)});
        passed &= expectRuntimeError([negativeBeatChart]() { (void)Game::GameplaySession{negativeBeatChart}; }, "negative note beat should throw");

        Game::Chart invalidHoldChart = makeRuntimeChart({makeHoldNote(2, 0)});
        invalidHoldChart.notes[0].endBeat = Game::BeatPosition{1, 1};
        passed &= expectRuntimeError([invalidHoldChart]() { (void)Game::GameplaySession{invalidHoldChart}; }, "hold endBeat before beat should throw");

        Game::GameplaySessionConfig invalidConfig{};
        invalidConfig.scrollLeadTime = std::chrono::microseconds{0};
        passed &= expectRuntimeError(
            [invalidConfig]() { (void)Game::GameplaySession{makeRuntimeChart({makeTapNote(2, 0)}), invalidConfig}; },
            "invalid scroll lead time should throw");
        return passed;
    }

    /**
     * @brief Verifies extreme update times are rejected instead of overflowing duration arithmetic.
     * @return True when overflow-prone song times throw runtime_error.
     */
    bool testGameplaySessionRejectsExtremeUpdateTimes()
    {
        bool passed = true;
        Game::GameplaySession earlyExtremeSession = makeSingleTapSession();
        passed &= expectRuntimeError(
            [&earlyExtremeSession]()
            {
                earlyExtremeSession.update(std::chrono::microseconds::min(), makePressedLaneSnapshot(1));
            },
            "minimum song time should throw before duration overflow");

        Game::GameplaySession lateExtremeSession = makeSingleTapSession();
        passed &= expectRuntimeError(
            [&lateExtremeSession]()
            {
                lateExtremeSession.update(std::chrono::microseconds::max(), Game::LaneInputSnapshot{});
            },
            "maximum song time should throw before duration overflow");
        return passed;
    }
}

/**
 * @brief Runs all gameplay session tests.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Zero when all gameplay session checks pass; nonzero when any check fails.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    bool allTestsPassed = true;
    allTestsPassed &= testGameplaySessionNoOpWithEmptyChart();
    allTestsPassed &= testGameplaySessionJudgesCriticalPerfectOnExactHit();
    allTestsPassed &= testGameplaySessionJudgesTapWindowBoundaries();
    allTestsPassed &= testGameplaySessionHandlesEarlyInputAndAutoMiss();
    allTestsPassed &= testGameplaySessionRequiresMatchingPressedLaneEdge();
    allTestsPassed &= testGameplaySessionDoesNotDoubleJudgeSameNote();
    allTestsPassed &= testGameplaySessionJudgesSameLaneNotesInOrder();
    allTestsPassed &= testGameplaySessionSupportsChordJudgementAcrossDifferentLanes();
    allTestsPassed &= testGameplaySessionAppliesChartOffsetToTargetTime();
    allTestsPassed &= testGameplaySessionConvertsBeatToHitTimeUsingTimingPoints();
    allTestsPassed &= testGameplaySessionTreatsHoldHeadAsTapInInitialSlice();
    allTestsPassed &= testGameplaySessionSnapshotTracksVisibleNotesAndPressedLanes();
    allTestsPassed &= testGameplaySessionComboIncrementsAndResetsOnMiss();
    allTestsPassed &= testGameplaySessionTimestampedEventsJudgeDenseSameLaneNotes();
    allTestsPassed &= testGameplaySessionTimestampedEventCanHitBeforeFrameBoundaryMiss();
    allTestsPassed &= testGameplaySessionRejectsInvalidTimestampedEventBatches();
    allTestsPassed &= testGameplaySessionRejectsInvalidRuntimeInputs();
    allTestsPassed &= testGameplaySessionRejectsExtremeUpdateTimes();

    return allTestsPassed ? 0 : 1;
}
