// RaythmDemo - Audio Engine Implementation
// Implements minimal miniaudio playback ownership and deterministic playback-clock semantics.
// Author: RatherHard
// Date: 2026-07-04

#include "Audio/AudioEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>

#include "miniaudio.h"

namespace Raythm::Audio
{
    namespace
    {
        /** @brief Maximum fake track duration accepted by the public test seam. */
        constexpr double MAX_TEST_TRACK_DURATION_SECONDS = 24.0 * 60.0 * 60.0;

        /** @brief Converts seconds to the Audio duration type without relying on C++23 chrono helpers. */
        AudioDuration secondsToDuration(double seconds)
        {
            return std::chrono::duration_cast<AudioDuration>(std::chrono::duration<double>(seconds));
        }

        /** @brief Converts an Audio duration to seconds. */
        double durationToSeconds(AudioDuration duration)
        {
            return std::chrono::duration<double>(duration).count();
        }

        /** @brief RAII deleter for initialized miniaudio decoders that are not yet owned by AudioEngine::Impl. */
        struct DecoderDeleter
        {
            /** @brief Releases decoder C-side resources before deleting the wrapper object. */
            void operator()(ma_decoder* decoder) const noexcept
            {
                if (decoder != nullptr)
                {
                    ma_decoder_uninit(decoder);
                    delete decoder;
                }
            }
        };

        /** @brief Returns the default monotonic audio clock sample. */
        AudioTimePoint defaultNow() noexcept
        {
            return AudioClock::now();
        }
    }

    AudioEngineOptions AudioEngineOptions::createClockOnly(AudioNowFunction nowFunction)
    {
        AudioEngineOptions options{};
        options.useClockOnlyBackend = true;
        options.nowFunction = std::move(nowFunction);
        return options;
    }

    struct AudioEngine::Impl
    {
        /** @brief Creates backend state from engine options. */
        explicit Impl(const AudioEngineOptions& options)
            : useClockOnlyBackend(options.useClockOnlyBackend),
              nowFunction(options.nowFunction ? options.nowFunction : AudioNowFunction(defaultNow)),
              lastClockSample(defaultNow())
        {
            if (!useClockOnlyBackend)
            {
                const ma_result result = ma_engine_init(nullptr, &engine);
                isEngineInitialized = result == MA_SUCCESS;
            }
        }

        /** @brief Releases the current sound and backend engine in dependency order. */
        ~Impl()
        {
            unloadTrack();
            if (isEngineInitialized)
            {
                ma_engine_uninit(&engine);
            }
        }

        /** @brief Copying is disabled because miniaudio handles have unique ownership. */
        Impl(const Impl&) = delete;

        /** @brief Copy assignment is disabled because miniaudio handles have unique ownership. */
        Impl& operator=(const Impl&) = delete;

        /** @brief Returns the current audio clock sample. */
        AudioTimePoint now() const noexcept
        {
            try
            {
                lastClockSample = nowFunction();
                return lastClockSample;
            }
            catch (...)
            {
                return lastClockSample;
            }
        }

        /** @brief Releases any currently loaded track and resets public playback state. */
        void unloadTrack() noexcept
        {
            if (isSoundInitialized && sound)
            {
                ma_sound_uninit(sound.get());
                sound.reset();
                isSoundInitialized = false;
            }
            if (isMemoryDecoderInitialized && memoryDecoder)
            {
                ma_decoder_uninit(memoryDecoder.get());
                memoryDecoder.reset();
                isMemoryDecoderInitialized = false;
            }
            encodedAudioBytes.clear();

            hasTrack = false;
            state = PlaybackState::Stopped;
            pausedOffset = AudioDuration::zero();
            duration = AudioDuration::zero();
            startedAt = now();
        }

        /** @brief Returns the sample rate reported by the active sound data source, or zero when unavailable. */
        ma_uint32 getSoundSampleRate() const noexcept
        {
            if (!isSoundInitialized || !sound)
            {
                return 0;
            }

            ma_uint32 sampleRate = 0;
            if (ma_sound_get_data_format(sound.get(), nullptr, nullptr, &sampleRate, nullptr, 0) != MA_SUCCESS)
            {
                return 0;
            }
            return sampleRate;
        }

        /** @brief Returns the current real miniaudio cursor, or zero when unavailable. */
        AudioDuration getSoundCursorDuration() const noexcept
        {
            if (!isSoundInitialized || !isEngineInitialized || !sound)
            {
                return AudioDuration::zero();
            }

            ma_uint64 cursorInFrames = 0;
            const ma_uint32 sampleRate = getSoundSampleRate();
            if (sampleRate == 0 || ma_sound_get_cursor_in_pcm_frames(sound.get(), &cursorInFrames) != MA_SUCCESS)
            {
                return AudioDuration::zero();
            }

            return secondsToDuration(static_cast<double>(cursorInFrames) / static_cast<double>(sampleRate));
        }

        /** @brief Updates cached duration from the active sound when the source reports a known length. */
        void refreshSoundDuration() noexcept
        {
            if (!isSoundInitialized || !sound)
            {
                duration = AudioDuration::zero();
                return;
            }

            ma_uint64 lengthInFrames = 0;
            const ma_uint32 sampleRate = getSoundSampleRate();
            if (sampleRate > 0 && ma_sound_get_length_in_pcm_frames(sound.get(), &lengthInFrames) == MA_SUCCESS)
            {
                duration = secondsToDuration(static_cast<double>(lengthInFrames) / static_cast<double>(sampleRate));
            }
            else
            {
                duration = AudioDuration::zero();
            }
        }

        /** @brief Returns elapsed playback duration before state-specific clamping. */
        AudioDuration calculateRawPlaybackDuration() const
        {
            if (!hasTrack)
            {
                return AudioDuration::zero();
            }

            if (isSoundInitialized)
            {
                return getSoundCursorDuration();
            }

            if (state == PlaybackState::Playing)
            {
                return pausedOffset + (now() - startedAt);
            }

            if (state == PlaybackState::Ended)
            {
                return duration;
            }

            return pausedOffset;
        }

        /** @brief Refreshes Ended state when playback reaches the end of a real or deterministic track. */
        void refreshEndedState() noexcept
        {
            if (state != PlaybackState::Playing)
            {
                return;
            }

            if (isSoundInitialized && sound && ma_sound_at_end(sound.get()))
            {
                state = PlaybackState::Ended;
                pausedOffset = duration > AudioDuration::zero() ? duration : getSoundCursorDuration();
                return;
            }

            if (duration <= AudioDuration::zero())
            {
                return;
            }

            if (calculateRawPlaybackDuration() >= duration)
            {
                state = PlaybackState::Ended;
                pausedOffset = duration;
            }
        }

        /** @brief True when no host audio device should be opened. */
        bool useClockOnlyBackend = false;

        /** @brief True when ma_engine_init succeeded. */
        bool isEngineInitialized = false;

        /** @brief True when sound owns a loaded miniaudio track. */
        bool isSoundInitialized = false;

        /** @brief True when memoryDecoder owns an encoded in-memory track data source. */
        bool isMemoryDecoderInitialized = false;

        /** @brief True when any real or test track is available for playback. */
        bool hasTrack = false;

        /** @brief Current user-visible playback state. */
        mutable PlaybackState state = PlaybackState::Stopped;

        /** @brief Clock offset accumulated before the current Playing run. */
        mutable AudioDuration pausedOffset = AudioDuration::zero();

        /** @brief Known track duration, or zero when unavailable. */
        AudioDuration duration = AudioDuration::zero();

        /** @brief Clock sample when the current Playing run started. */
        mutable AudioTimePoint startedAt{};

        /** @brief Clock function used for deterministic time semantics. */
        AudioNowFunction nowFunction{};

        /** @brief Last safe clock sample used if an injected test clock throws. */
        mutable AudioTimePoint lastClockSample{};

        /** @brief miniaudio top-level engine used for real playback. */
        ma_engine engine{};

        /** @brief Single loaded music sound owned by the engine. */
        std::unique_ptr<ma_sound> sound{};

        /** @brief Encoded audio bytes backing the in-memory decoder data source. */
        std::vector<std::byte> encodedAudioBytes{};

        /** @brief Decoder used as a stable in-memory data source for verified audio assets. */
        std::unique_ptr<ma_decoder> memoryDecoder{};
    };

    AudioEngine::AudioEngine()
        : AudioEngine(AudioEngineOptions{})
    {
    }

    AudioEngine::AudioEngine(const AudioEngineOptions& options)
        : m_impl(std::make_unique<Impl>(options))
    {
    }

    AudioEngine::~AudioEngine() = default;

    bool AudioEngine::loadMusicFile(const std::string& filePath)
    {
        if (filePath.empty() || filePath.find('\0') != std::string::npos || m_impl->useClockOnlyBackend ||
            !m_impl->isEngineInitialized)
        {
            return false;
        }

        auto newSound = std::make_unique<ma_sound>();
        const ma_result loadResult = ma_sound_init_from_file(
            &m_impl->engine,
            filePath.c_str(),
            MA_SOUND_FLAG_STREAM,
            nullptr,
            nullptr,
            newSound.get()
        );

        if (loadResult != MA_SUCCESS)
        {
            return false;
        }

        m_impl->unloadTrack();
        m_impl->sound = std::move(newSound);
        m_impl->isSoundInitialized = true;
        m_impl->hasTrack = true;
        m_impl->state = PlaybackState::Stopped;
        m_impl->pausedOffset = AudioDuration::zero();
        m_impl->startedAt = m_impl->now();
        m_impl->refreshSoundDuration();

        return true;
    }

    bool AudioEngine::loadMusicFromMemory(std::vector<std::byte>&& encodedAudioBytes)
    {
        if (encodedAudioBytes.empty() || m_impl->useClockOnlyBackend || !m_impl->isEngineInitialized)
        {
            return false;
        }

        std::vector<std::byte> newEncodedAudioBytes = std::move(encodedAudioBytes);
        auto decoderStorage = std::make_unique<ma_decoder>();
        ma_decoder_config decoderConfig = ma_decoder_config_init_default();
        const ma_result decoderResult = ma_decoder_init_memory(
            newEncodedAudioBytes.data(),
            newEncodedAudioBytes.size(),
            &decoderConfig,
            decoderStorage.get());
        if (decoderResult != MA_SUCCESS)
        {
            return false;
        }
        std::unique_ptr<ma_decoder, DecoderDeleter> newDecoder{decoderStorage.release()};

        auto newSound = std::make_unique<ma_sound>();
        const ma_result soundResult = ma_sound_init_from_data_source(
            &m_impl->engine,
            newDecoder.get(),
            0,
            nullptr,
            newSound.get());
        if (soundResult != MA_SUCCESS)
        {
            return false;
        }

        m_impl->unloadTrack();
        m_impl->encodedAudioBytes = std::move(newEncodedAudioBytes);
        m_impl->memoryDecoder.reset(newDecoder.release());
        m_impl->isMemoryDecoderInitialized = true;
        m_impl->sound = std::move(newSound);
        m_impl->isSoundInitialized = true;
        m_impl->hasTrack = true;
        m_impl->state = PlaybackState::Stopped;
        m_impl->pausedOffset = AudioDuration::zero();
        m_impl->startedAt = m_impl->now();
        m_impl->refreshSoundDuration();

        return true;
    }

    bool AudioEngine::loadTestTrack(double durationSeconds) noexcept
    {
        if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0 ||
            durationSeconds > MAX_TEST_TRACK_DURATION_SECONDS)
        {
            return false;
        }

        m_impl->unloadTrack();
        m_impl->hasTrack = true;
        m_impl->state = PlaybackState::Stopped;
        m_impl->pausedOffset = AudioDuration::zero();
        m_impl->duration = secondsToDuration(durationSeconds);
        m_impl->startedAt = m_impl->now();
        return true;
    }

    bool AudioEngine::play() noexcept
    {
        if (!m_impl->hasTrack)
        {
            return false;
        }

        m_impl->refreshEndedState();
        if (m_impl->state == PlaybackState::Ended)
        {
            m_impl->pausedOffset = AudioDuration::zero();
            if (m_impl->isSoundInitialized)
            {
                ma_sound_seek_to_pcm_frame(m_impl->sound.get(), 0);
            }
        }

        if (m_impl->state == PlaybackState::Playing)
        {
            return true;
        }

        if (m_impl->isSoundInitialized)
        {
            if (m_impl->state == PlaybackState::Stopped)
            {
                ma_sound_seek_to_pcm_frame(m_impl->sound.get(), 0);
            }

            const ma_result startResult = ma_sound_start(m_impl->sound.get());
            if (startResult != MA_SUCCESS)
            {
                return false;
            }
        }

        m_impl->startedAt = m_impl->now();
        m_impl->state = PlaybackState::Playing;
        return true;
    }

    bool AudioEngine::pause() noexcept
    {
        m_impl->refreshEndedState();
        if (m_impl->state == PlaybackState::Paused)
        {
            return true;
        }

        if (m_impl->state != PlaybackState::Playing)
        {
            return false;
        }

        const AudioDuration rawPlaybackDuration = m_impl->calculateRawPlaybackDuration();
        m_impl->pausedOffset = m_impl->duration > AudioDuration::zero()
            ? std::min(rawPlaybackDuration, m_impl->duration)
            : rawPlaybackDuration;
        if (m_impl->isSoundInitialized)
        {
            ma_sound_stop(m_impl->sound.get());
        }
        m_impl->state = PlaybackState::Paused;
        return true;
    }

    bool AudioEngine::stop() noexcept
    {
        if (m_impl->isSoundInitialized)
        {
            ma_sound_stop(m_impl->sound.get());
            ma_sound_seek_to_pcm_frame(m_impl->sound.get(), 0);
        }

        m_impl->pausedOffset = AudioDuration::zero();
        m_impl->startedAt = m_impl->now();
        m_impl->state = PlaybackState::Stopped;
        return true;
    }

    bool AudioEngine::hasLoadedTrack() const noexcept
    {
        return m_impl->hasTrack;
    }

    bool AudioEngine::isBackendAvailable() const noexcept
    {
        return m_impl->useClockOnlyBackend || m_impl->isEngineInitialized;
    }

    PlaybackState AudioEngine::getState() const noexcept
    {
        m_impl->refreshEndedState();
        return m_impl->state;
    }

    double AudioEngine::getPlaybackTimeSeconds() const noexcept
    {
        m_impl->refreshEndedState();
        const AudioDuration rawDuration = m_impl->calculateRawPlaybackDuration();
        const AudioDuration clampedDuration = m_impl->duration > AudioDuration::zero()
            ? std::min(rawDuration, m_impl->duration)
            : rawDuration;
        return durationToSeconds(clampedDuration);
    }

    double AudioEngine::getTrackDurationSeconds() const noexcept
    {
        return durationToSeconds(m_impl->duration);
    }
}
