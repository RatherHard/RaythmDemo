// RaythmDemo - Audio Engine
// Defines minimal music playback controls and playback-clock semantics for rhythm gameplay.
// Author: RatherHard
// Date: 2026-07-04

#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Raythm::Audio
{
    /** @brief Monotonic clock used by the Audio playback clock. */
    using AudioClock = std::chrono::steady_clock;

    /** @brief Monotonic time point sampled by the Audio playback clock. */
    using AudioTimePoint = AudioClock::time_point;

    /** @brief Duration type used for Audio playback timing. */
    using AudioDuration = AudioClock::duration;

    /** @brief Callable that returns the current monotonic audio time point. */
    using AudioNowFunction = std::function<AudioTimePoint()>;

    /** @brief User-visible playback state for the currently loaded music track. */
    enum class PlaybackState
    {
        /** @brief No active playback; the playback cursor is at the beginning. */
        Stopped,

        /** @brief Playback is actively advancing. */
        Playing,

        /** @brief Playback is loaded and paused at a stable cursor position. */
        Paused,

        /** @brief Playback reached the loaded track duration. */
        Ended
    };

    /** @brief Startup options for AudioEngine. */
    struct AudioEngineOptions
    {
        /** @brief Creates a clock-only engine that skips host audio device initialization. */
        bool useClockOnlyBackend = false;

        /** @brief Clock function used to derive fallback playback time. */
        AudioNowFunction nowFunction{};

        /**
         * @brief Creates options for deterministic tests that do not touch audio hardware.
         * @param nowFunction Clock function used by the test engine.
         * @return Options configured for a clock-only AudioEngine.
         */
        static AudioEngineOptions createClockOnly(AudioNowFunction nowFunction);
    };

    /** @brief RAII owner for one music track and the playback clock exposed to Game. */
    class AudioEngine
    {
    public:
        /** @brief Creates an AudioEngine backed by miniaudio and the steady clock. */
        AudioEngine();

        /**
         * @brief Creates an AudioEngine from explicit startup options.
         * @param options Audio startup configuration and optional test clock.
         */
        explicit AudioEngine(const AudioEngineOptions& options);

        /** @brief Stops playback and releases miniaudio resources. */
        ~AudioEngine();

        /** @brief Copying is disabled because AudioEngine owns backend handles. */
        AudioEngine(const AudioEngine&) = delete;

        /** @brief Copy assignment is disabled because AudioEngine owns backend handles. */
        AudioEngine& operator=(const AudioEngine&) = delete;

        /** @brief Moving is disabled until Audio ownership needs transfer semantics. */
        AudioEngine(AudioEngine&&) = delete;

        /** @brief Move assignment is disabled until Audio ownership needs transfer semantics. */
        AudioEngine& operator=(AudioEngine&&) = delete;

        /**
         * @brief Loads a local music file as the active track.
         * @param filePath UTF-8 path to an audio file supported by miniaudio.
         * @return True when the file is loaded and ready for playback.
         * @note A failed load leaves the previous stopped state intact and reports false.
         */
        bool loadMusicFile(const std::string& filePath);

        /**
         * @brief Loads a music track from verified encoded file bytes.
         * @param encodedAudioBytes Complete encoded audio file contents supported by miniaudio.
         * @return True when the bytes are loaded and ready for playback.
         * @note A failed load leaves the previous stopped state intact and reports false.
         */
        bool loadMusicFromMemory(std::vector<std::byte>&& encodedAudioBytes);

        /**
         * @brief Loads a deterministic silent test track without touching audio hardware.
         * @param durationSeconds Duration exposed by the fake playback clock.
         * @return True when the duration is valid and the test track is loaded.
         * @note This is intended for CTest and future Game logic tests, not production playback.
         */
        bool loadTestTrack(double durationSeconds) noexcept;

        /**
         * @brief Starts or resumes playback of the loaded track.
         * @return True when playback entered or remained in Playing state.
         * @note Calling play after Ended restarts from the beginning.
         */
        bool play() noexcept;

        /**
         * @brief Pauses active playback while preserving the current playback time.
         * @return True when playback is paused or was already paused.
         * @note Calling pause while stopped or ended leaves state unchanged and returns false.
         */
        bool pause() noexcept;

        /**
         * @brief Stops playback and rewinds the active track to the beginning.
         * @return True when stop completed.
         * @note Repeated stop calls are harmless and keep playback time at zero.
         */
        bool stop() noexcept;

        /** @brief Reports whether an active track has been loaded. */
        bool hasLoadedTrack() const noexcept;

        /** @brief Returns the current user-visible playback state. */
        PlaybackState getState() const noexcept;

        /** @brief Reports whether the real miniaudio backend initialized successfully. */
        bool isBackendAvailable() const noexcept;

        /** @brief Returns the current playback cursor in seconds. */
        double getPlaybackTimeSeconds() const noexcept;

        /** @brief Returns the loaded track duration in seconds, or zero when unknown/unloaded. */
        double getTrackDurationSeconds() const noexcept;

    private:
        /** @brief Hidden implementation that keeps miniaudio out of the public API. */
        struct Impl;

        /** @brief Owned implementation state. */
        std::unique_ptr<Impl> m_impl;
    };
}
