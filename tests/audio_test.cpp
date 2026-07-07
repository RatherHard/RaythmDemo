// RaythmDemo - Audio Engine Tests
// Verifies minimal playback state and audio-clock semantics for rhythm gameplay.
// Author: RatherHard
// Date: 2026-07-04

#include "Audio/AudioEngine.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
    namespace Audio = Raythm::Audio;

    /** @brief Records a failed expectation without aborting the current test function. */
    bool expect(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << std::endl;
            return false;
        }

        return true;
    }

    /** @brief Compares two floating-point values with a small tolerance. */
    bool expectNear(double actual, double expected, double epsilon, const std::string& message)
    {
        const double difference = actual > expected ? actual - expected : expected - actual;
        return expect(difference <= epsilon, message);
    }

    /** @brief Manual monotonic clock used to make playback-time tests deterministic. */
    class ManualClock
    {
    public:
        /** @brief Returns the current synthetic time point. */
        Audio::AudioTimePoint now() const noexcept
        {
            return m_currentTime;
        }

        /** @brief Advances the synthetic clock by the supplied duration. */
        void advance(Audio::AudioDuration duration) noexcept
        {
            m_currentTime += duration;
        }

    private:
        /** @brief Current synthetic monotonic time. */
        Audio::AudioTimePoint m_currentTime{};
    };

    /** @brief Reads the project's canonical Ogg Vorbis chart audio asset for memory-backed playback tests. */
    std::vector<std::byte> readProjectOggAssetBytes()
    {
        const std::filesystem::path oggPath = std::filesystem::path(__FILE__).parent_path().parent_path() /
            "assets/charts/00001/00001.ogg";
        const std::uintmax_t fileSize = std::filesystem::file_size(oggPath);
        std::vector<std::byte> bytes(static_cast<std::size_t>(fileSize));
        std::ifstream input{oggPath, std::ios::binary};
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input)
        {
            return {};
        }
        return bytes;
    }

    /** @brief Verifies a test-clock engine starts stopped with no playback position. */
    bool testAudioEngineStartsStopped()
    {
        ManualClock clock{};
        Audio::AudioEngine engine{Audio::AudioEngineOptions::createClockOnly([&clock]() noexcept {
            return clock.now();
        })};

        bool passed = true;
        passed &= expect(engine.getState() == Audio::PlaybackState::Stopped, "new engine should start stopped");
        passed &= expectNear(engine.getPlaybackTimeSeconds(), 0.0, 0.000001,
                             "new engine playback time should be zero");
        passed &= expect(!engine.hasLoadedTrack(), "new engine should not report a loaded track");
        return passed;
    }

    /** @brief Verifies fake track playback advances from the injected clock while playing. */
    bool testPlaybackTimeAdvancesWhilePlaying()
    {
        ManualClock clock{};
        Audio::AudioEngine engine{Audio::AudioEngineOptions::createClockOnly([&clock]() noexcept {
            return clock.now();
        })};

        const bool loaded = engine.loadTestTrack(3.0);
        const bool started = engine.play();
        clock.advance(std::chrono::milliseconds(250));

        bool passed = true;
        passed &= expect(loaded, "test track should load successfully");
        passed &= expect(started, "play should succeed after loading a track");
        passed &= expect(engine.getState() == Audio::PlaybackState::Playing, "state should become Playing");
        passed &= expectNear(engine.getPlaybackTimeSeconds(), 0.25, 0.000001,
                             "playback time should follow elapsed playing time");
        return passed;
    }

    /** @brief Verifies pause freezes playback time and resume continues from the paused position. */
    bool testPauseFreezesAndResumeContinuesPlaybackTime()
    {
        ManualClock clock{};
        Audio::AudioEngine engine{Audio::AudioEngineOptions::createClockOnly([&clock]() noexcept {
            return clock.now();
        })};

        engine.loadTestTrack(5.0);
        engine.play();
        clock.advance(std::chrono::milliseconds(400));
        const bool paused = engine.pause();
        clock.advance(std::chrono::milliseconds(600));
        const double pausedTime = engine.getPlaybackTimeSeconds();
        const bool resumed = engine.play();
        clock.advance(std::chrono::milliseconds(100));

        bool passed = true;
        passed &= expect(paused, "pause should succeed while playing");
        passed &= expectNear(pausedTime, 0.4, 0.000001, "paused playback time should stay frozen");
        passed &= expect(resumed, "play should resume from paused state");
        passed &= expectNear(engine.getPlaybackTimeSeconds(), 0.5, 0.000001,
                             "resumed playback time should continue from paused position");
        return passed;
    }

    /** @brief Verifies stop returns playback to the beginning and makes repeated stop harmless. */
    bool testStopResetsPlaybackTime()
    {
        ManualClock clock{};
        Audio::AudioEngine engine{Audio::AudioEngineOptions::createClockOnly([&clock]() noexcept {
            return clock.now();
        })};

        engine.loadTestTrack(5.0);
        engine.play();
        clock.advance(std::chrono::seconds(1));
        const bool stopped = engine.stop();
        const bool stoppedAgain = engine.stop();

        bool passed = true;
        passed &= expect(stopped, "stop should succeed after playback starts");
        passed &= expect(stoppedAgain, "repeated stop should be harmless");
        passed &= expect(engine.getState() == Audio::PlaybackState::Stopped, "stop should set state to Stopped");
        passed &= expectNear(engine.getPlaybackTimeSeconds(), 0.0, 0.000001,
                             "stop should reset playback time to zero");
        return passed;
    }

    /** @brief Verifies repeated play while already playing does not restart the playback clock. */
    bool testRepeatedPlayKeepsPlaybackPosition()
    {
        ManualClock clock{};
        Audio::AudioEngine engine{Audio::AudioEngineOptions::createClockOnly([&clock]() noexcept {
            return clock.now();
        })};

        engine.loadTestTrack(5.0);
        engine.play();
        clock.advance(std::chrono::milliseconds(300));
        const bool playedAgain = engine.play();
        clock.advance(std::chrono::milliseconds(200));

        bool passed = true;
        passed &= expect(playedAgain, "repeated play should be harmless while already playing");
        passed &= expectNear(engine.getPlaybackTimeSeconds(), 0.5, 0.000001,
                             "repeated play should not restart playback time");
        return passed;
    }

    /** @brief Verifies playback reaches Ended when the loaded track duration is exhausted. */
    bool testPlaybackStateBecomesEndedAtTrackDuration()
    {
        ManualClock clock{};
        Audio::AudioEngine engine{Audio::AudioEngineOptions::createClockOnly([&clock]() noexcept {
            return clock.now();
        })};

        engine.loadTestTrack(1.0);
        engine.play();
        clock.advance(std::chrono::milliseconds(1500));

        bool passed = true;
        passed &= expect(engine.getState() == Audio::PlaybackState::Ended,
                         "playback should report Ended after track duration");
        passed &= expectNear(engine.getPlaybackTimeSeconds(), 1.0, 0.000001,
                             "ended playback time should clamp to track duration");
        return passed;
    }

    /** @brief Verifies invalid fake track durations are rejected before chrono conversion. */
    bool testInvalidTestTrackDurationsFail()
    {
        ManualClock clock{};
        Audio::AudioEngine engine{Audio::AudioEngineOptions::createClockOnly([&clock]() noexcept {
            return clock.now();
        })};

        const bool loadedNaN = engine.loadTestTrack(std::numeric_limits<double>::quiet_NaN());
        const bool loadedInfinite = engine.loadTestTrack(std::numeric_limits<double>::infinity());
        const bool loadedHuge = engine.loadTestTrack(48.0 * 60.0 * 60.0);

        bool passed = true;
        passed &= expect(!loadedNaN, "NaN fake track duration should fail");
        passed &= expect(!loadedInfinite, "infinite fake track duration should fail");
        passed &= expect(!loadedHuge, "oversized fake track duration should fail");
        passed &= expect(!engine.hasLoadedTrack(), "invalid fake track durations should not load a track");
        return passed;
    }

    /** @brief Verifies loading a missing file fails without changing the current stopped clock state. */
    bool testMissingFileLoadFailsWithoutDeviceDependency()
    {
        ManualClock clock{};
        Audio::AudioEngine engine{Audio::AudioEngineOptions::createClockOnly([&clock]() noexcept {
            return clock.now();
        })};

        const bool loaded = engine.loadMusicFile("assets/does-not-exist-for-audio-test.wav");

        bool passed = true;
        passed &= expect(!loaded, "missing music file should fail to load");
        passed &= expect(engine.getState() == Audio::PlaybackState::Stopped,
                         "failed load should leave engine stopped");
        passed &= expectNear(engine.getPlaybackTimeSeconds(), 0.0, 0.000001,
                             "failed load should leave playback time at zero");
        return passed;
    }

    /** @brief Verifies embedded NUL paths fail before reaching the C backend. */
    bool testEmbeddedNullFilePathFails()
    {
        Audio::AudioEngine engine{};
        const std::string filePathWithNull{"assets/allowed.wav\0hidden.wav", 29};

        const bool loaded = engine.loadMusicFile(filePathWithNull);

        bool passed = true;
        passed &= expect(!loaded, "embedded NUL music path should fail");
        passed &= expect(!engine.hasLoadedTrack(), "embedded NUL path should not load a track");
        return passed;
    }

    /** @brief Verifies real Ogg Vorbis music bytes load through the same memory-backed seam used by startup. */
    bool testProjectOggVorbisMemoryTrackLoadsWhenBackendAvailable()
    {
        Audio::AudioEngine engine{};
        if (!engine.isBackendAvailable())
        {
            std::cout << "SKIPPED: real audio backend unavailable; cannot verify Ogg Vorbis memory loading"
                      << std::endl;
            return true;
        }

        std::vector<std::byte> oggBytes = readProjectOggAssetBytes();
        const bool loaded = engine.loadMusicFromMemory(std::move(oggBytes));

        bool passed = true;
        passed &= expect(loaded, "project Ogg Vorbis music bytes should load successfully");
        passed &= expect(engine.hasLoadedTrack(), "valid Ogg Vorbis bytes should report a loaded track");
        passed &= expect(engine.getState() == Audio::PlaybackState::Stopped,
                         "successful Ogg Vorbis load should leave playback stopped");
        passed &= expectNear(engine.getPlaybackTimeSeconds(), 0.0, 0.000001,
                             "successful Ogg Vorbis load should not advance playback time");
        passed &= expect(engine.play(), "loaded Ogg Vorbis track should start playback");
        return passed;
    }

    /** @brief Verifies play without a loaded track fails instead of inventing playback. */
    bool testPlayWithoutTrackFails()
    {
        ManualClock clock{};
        Audio::AudioEngine engine{Audio::AudioEngineOptions::createClockOnly([&clock]() noexcept {
            return clock.now();
        })};

        const bool started = engine.play();

        bool passed = true;
        passed &= expect(!started, "play should fail without a loaded track");
        passed &= expect(engine.getState() == Audio::PlaybackState::Stopped,
                         "play without track should leave state stopped");
        return passed;
    }
}

/**
 * @brief Runs all Audio engine behavior tests.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Zero when all Audio checks pass; nonzero when any check fails.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    bool allTestsPassed = true;
    allTestsPassed &= testAudioEngineStartsStopped();
    allTestsPassed &= testPlaybackTimeAdvancesWhilePlaying();
    allTestsPassed &= testPauseFreezesAndResumeContinuesPlaybackTime();
    allTestsPassed &= testStopResetsPlaybackTime();
    allTestsPassed &= testRepeatedPlayKeepsPlaybackPosition();
    allTestsPassed &= testPlaybackStateBecomesEndedAtTrackDuration();
    allTestsPassed &= testInvalidTestTrackDurationsFail();
    allTestsPassed &= testMissingFileLoadFailsWithoutDeviceDependency();
    allTestsPassed &= testEmbeddedNullFilePathFails();
    allTestsPassed &= testProjectOggVorbisMemoryTrackLoadsWhenBackendAvailable();
    allTestsPassed &= testPlayWithoutTrackFails();

    return allTestsPassed ? 0 : 1;
}
