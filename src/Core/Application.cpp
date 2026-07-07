// RaythmDemo - Core Application Lifecycle Implementation
// Implements application startup, event routing, frame timing, rendering, and shutdown.
// Author: RatherHard
// Date: 2026-07-04

#include "Core/Application.hpp"

#include "Audio/AudioEngine.hpp"
#include "Core/GameplayRuntimeDriver.hpp"
#include "Game/ChartLoader.hpp"
#include "Platform/EventPump.hpp"
#include "Platform/InputState.hpp"
#include "Platform/Window.hpp"
#include "Render/Renderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Raythm::Core
{
    namespace
    {
        /** @brief Maximum platform events processed in one frame before rendering is allowed to proceed. */
        constexpr int MAX_EVENTS_PER_FRAME = 1024;

        /** @brief Maximum audio asset size accepted before invoking the decoder. */
        constexpr std::uintmax_t MAX_AUDIO_FILE_SIZE_BYTES = 256U * 1024U * 1024U;

        /** @brief Supported runtime music file extension for the initial checked-in asset slice. */
        constexpr const char* SUPPORTED_RUNTIME_AUDIO_EXTENSION = ".ogg";

        /** @brief Delay used when rendering is paused by a zero-sized drawable surface. */
        constexpr Uint32 PAUSED_RENDER_DELAY_MILLISECONDS = 10;

        /** @brief RAII owner for the SDL video subsystem initialized by Core. */
        class SdlVideoSession
        {
        public:
            /**
             * @brief Initializes SDL video for the application lifetime.
             * @note Throws are avoided here so Application can print SDL_GetError consistently.
             */
            SdlVideoSession() noexcept
                : m_initialized(SDL_Init(SDL_INIT_VIDEO))
            {
            }

            /** @brief Quits SDL video if this session initialized it. */
            ~SdlVideoSession()
            {
                if (m_initialized)
                {
                    SDL_QuitSubSystem(SDL_INIT_VIDEO);
                }
            }

            /** @brief Copying is disabled because this object owns subsystem shutdown. */
            SdlVideoSession(const SdlVideoSession&) = delete;

            /** @brief Copy assignment is disabled because this object owns subsystem shutdown. */
            SdlVideoSession& operator=(const SdlVideoSession&) = delete;

            /** @brief Reports whether SDL video initialization succeeded. */
            bool isInitialized() const noexcept
            {
                return m_initialized;
            }

        private:
            /** @brief True when SDL_Init accepted the video initialization request. */
            bool m_initialized = false;
        };

        /**
         * @brief Finds the first existing asset root candidate for a configured path.
         * @param configuredRoot Configured asset root, absolute or relative.
         * @return Existing directory used as the chart loader asset root.
         * @throws std::runtime_error when no candidate exists.
         * @note Relative roots are tried from the current working directory and the executable directory.
         */
        std::filesystem::path resolveRuntimeAssetRoot(const std::filesystem::path& configuredRoot)
        {
            if (configuredRoot.empty())
            {
                throw std::runtime_error("Gameplay startup failed: asset root is empty");
            }

            std::vector<std::filesystem::path> candidates;
            if (configuredRoot.is_absolute())
            {
                candidates.push_back(configuredRoot);
            }
            else
            {
                const char* basePath = SDL_GetBasePath();
                if (basePath != nullptr)
                {
                    candidates.push_back(std::filesystem::u8path(basePath) / configuredRoot);
                }
            }

            for (const std::filesystem::path& candidate : candidates)
            {
                std::error_code error;
                if (std::filesystem::is_directory(candidate, error) && !error)
                {
                    return candidate;
                }
            }

            throw std::runtime_error("Gameplay startup failed: unable to locate asset root '" + configuredRoot.string() + "'");
        }

        /**
         * @brief Tests whether a canonical candidate path is inside a canonical root path.
         * @param root Canonical root directory.
         * @param candidate Canonical candidate path.
         * @return True when candidate is equal to root or below it.
         */
        bool isPathInsideRoot(const std::filesystem::path& root, const std::filesystem::path& candidate)
        {
            auto rootIterator = root.begin();
            auto candidateIterator = candidate.begin();
            for (; rootIterator != root.end(); ++rootIterator, ++candidateIterator)
            {
                if (candidateIterator == candidate.end() || *rootIterator != *candidateIterator)
                {
                    return false;
                }
            }

            return true;
        }

        /**
         * @brief Resolves a chart-declared audio path under the configured asset root.
         * @param assetRoot Canonical asset root from ChartLoader.
         * @param audioPath Chart metadata audio path.
         * @return Canonical audio file path under assetRoot.
         * @throws std::runtime_error when the path is missing, escapes root, or is not a regular file.
         */
        std::filesystem::path resolveAudioAssetPath(
            const std::filesystem::path& assetRoot,
            const std::string& audioPath)
        {
            if (audioPath.empty() || audioPath.find('\0') != std::string::npos)
            {
                throw std::runtime_error("Gameplay startup failed: chart audio path is empty or invalid");
            }

            const std::filesystem::path requestedPath = std::filesystem::u8path(audioPath);
            const std::filesystem::path rootedPath = requestedPath.is_absolute() ? requestedPath : assetRoot / requestedPath;
            std::error_code error;
            const std::filesystem::path canonicalPath = std::filesystem::canonical(rootedPath, error);
            if (error)
            {
                throw std::runtime_error("Gameplay startup failed: unable to resolve audio file '" + audioPath + "'");
            }
            if (!isPathInsideRoot(assetRoot, canonicalPath))
            {
                throw std::runtime_error("Gameplay startup failed: audio file escapes the configured asset root");
            }
            if (!std::filesystem::is_regular_file(canonicalPath, error) || error)
            {
                throw std::runtime_error("Gameplay startup failed: audio path is not a regular file");
            }

            return canonicalPath;
        }

        /**
         * @brief Reads verified audio asset bytes without reopening the file by path after validation.
         * @param assetRoot Canonical asset root used for opened-file confinement checks.
         * @param audioPath Canonical audio path already confined to assetRoot.
         * @return Complete encoded audio bytes from the verified opened file.
         * @throws std::runtime_error when the opened object is unsafe, unsupported, or cannot be read.
         * @note Mirrors the chart loader's open-then-verify pattern to avoid decoder path-swap races.
         */
        std::vector<std::byte> readAudioAssetBytesNoFollow(
            const std::filesystem::path& assetRoot,
            const std::filesystem::path& audioPath)
        {
            if (audioPath.extension() != SUPPORTED_RUNTIME_AUDIO_EXTENSION)
            {
                throw std::runtime_error("Gameplay startup failed: unsupported audio file extension");
            }

#if defined(_WIN32)
            struct FileHandle
            {
                HANDLE handle = INVALID_HANDLE_VALUE;

                ~FileHandle()
                {
                    if (handle != INVALID_HANDLE_VALUE)
                    {
                        CloseHandle(handle);
                    }
                }
            };

            FileHandle file{
                CreateFileW(
                    audioPath.wstring().c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
                    nullptr)};
            if (file.handle == INVALID_HANDLE_VALUE)
            {
                throw std::runtime_error("Gameplay startup failed: unable to open audio file handle");
            }
            if (GetFileType(file.handle) != FILE_TYPE_DISK)
            {
                throw std::runtime_error("Gameplay startup failed: opened audio path is not a disk file");
            }

            BY_HANDLE_FILE_INFORMATION fileInformation{};
            if (GetFileInformationByHandle(file.handle, &fileInformation) == 0)
            {
                throw std::runtime_error("Gameplay startup failed: unable to inspect opened audio file");
            }
            if ((fileInformation.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
                (fileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                throw std::runtime_error("Gameplay startup failed: opened audio path is not a regular file");
            }

            std::wstring finalPathBuffer(MAX_PATH, L'\0');
            DWORD finalPathLength = 0;
            for (;;)
            {
                finalPathLength = GetFinalPathNameByHandleW(
                    file.handle,
                    finalPathBuffer.data(),
                    static_cast<DWORD>(finalPathBuffer.size()),
                    FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
                if (finalPathLength == 0)
                {
                    throw std::runtime_error("Gameplay startup failed: unable to verify opened audio path");
                }
                if (finalPathLength < finalPathBuffer.size())
                {
                    break;
                }

                finalPathBuffer.assign(static_cast<std::size_t>(finalPathLength) + 1U, L'\0');
            }
            finalPathBuffer.resize(finalPathLength);
            constexpr std::wstring_view WINDOWS_LONG_PATH_PREFIX = L"\\\\?\\";
            constexpr std::wstring_view WINDOWS_LONG_UNC_PREFIX = L"\\\\?\\UNC\\";
            if (finalPathBuffer.rfind(WINDOWS_LONG_UNC_PREFIX, 0) == 0)
            {
                finalPathBuffer.replace(0, WINDOWS_LONG_UNC_PREFIX.size(), L"\\\\");
            }
            else if (finalPathBuffer.rfind(WINDOWS_LONG_PATH_PREFIX, 0) == 0)
            {
                finalPathBuffer.erase(0, WINDOWS_LONG_PATH_PREFIX.size());
            }
            if (!isPathInsideRoot(assetRoot, std::filesystem::weakly_canonical(std::filesystem::path(finalPathBuffer))))
            {
                throw std::runtime_error("Gameplay startup failed: opened audio file escapes the configured asset root");
            }

            LARGE_INTEGER fileSize{};
            if (GetFileSizeEx(file.handle, &fileSize) == 0 || fileSize.QuadPart <= 0)
            {
                throw std::runtime_error("Gameplay startup failed: unable to inspect opened audio file size");
            }
            if (static_cast<std::uintmax_t>(fileSize.QuadPart) > MAX_AUDIO_FILE_SIZE_BYTES)
            {
                throw std::runtime_error("Gameplay startup failed: audio file exceeds the configured size limit");
            }

            std::vector<std::byte> contents(static_cast<std::size_t>(fileSize.QuadPart));
            std::size_t offset = 0;
            while (offset < contents.size())
            {
                const DWORD chunkSize = static_cast<DWORD>(std::min<std::size_t>(
                    contents.size() - offset,
                    static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
                DWORD bytesRead = 0;
                if (ReadFile(file.handle, contents.data() + offset, chunkSize, &bytesRead, nullptr) == 0 || bytesRead == 0)
                {
                    throw std::runtime_error("Gameplay startup failed: unable to read opened audio file");
                }

                offset += bytesRead;
            }

            return contents;
#elif defined(__linux__)
            struct FileDescriptor
            {
                int descriptor = -1;

                ~FileDescriptor()
                {
                    if (descriptor >= 0)
                    {
                        close(descriptor);
                    }
                }
            };

            FileDescriptor file{open(audioPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)};
            if (file.descriptor < 0)
            {
                throw std::runtime_error(std::string("Gameplay startup failed: unable to open audio file handle: ") + std::strerror(errno));
            }

            struct stat fileStat{};
            if (fstat(file.descriptor, &fileStat) != 0)
            {
                throw std::runtime_error(std::string("Gameplay startup failed: unable to inspect opened audio file: ") + std::strerror(errno));
            }
            if (!S_ISREG(fileStat.st_mode) || fileStat.st_size <= 0 ||
                static_cast<std::uintmax_t>(fileStat.st_size) > MAX_AUDIO_FILE_SIZE_BYTES)
            {
                throw std::runtime_error("Gameplay startup failed: opened audio file is unsupported");
            }

            const std::filesystem::path descriptorPath = "/proc/self/fd/" + std::to_string(file.descriptor);
            std::error_code finalPathError;
            const std::filesystem::path finalPath = std::filesystem::canonical(descriptorPath, finalPathError);
            if (finalPathError || !isPathInsideRoot(assetRoot, finalPath))
            {
                throw std::runtime_error("Gameplay startup failed: opened audio file escapes the configured asset root");
            }

            std::vector<std::byte> contents(static_cast<std::size_t>(fileStat.st_size));
            std::size_t offset = 0;
            while (offset < contents.size())
            {
                const ssize_t bytesRead = read(file.descriptor, contents.data() + offset, contents.size() - offset);
                if (bytesRead < 0 && errno == EINTR)
                {
                    continue;
                }
                if (bytesRead <= 0)
                {
                    throw std::runtime_error(std::string("Gameplay startup failed: unable to read opened audio file: ") + std::strerror(errno));
                }

                offset += static_cast<std::size_t>(bytesRead);
            }

            return contents;
#else
            std::error_code sizeError;
            const std::uintmax_t fileSize = std::filesystem::file_size(audioPath, sizeError);
            if (sizeError || fileSize == 0 || fileSize > MAX_AUDIO_FILE_SIZE_BYTES)
            {
                throw std::runtime_error("Gameplay startup failed: audio file size is outside the supported range");
            }

            std::ifstream file(audioPath, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Gameplay startup failed: unable to open audio file");
            }

            std::vector<std::byte> contents(static_cast<std::size_t>(fileSize));
            file.read(reinterpret_cast<char*>(contents.data()), static_cast<std::streamsize>(contents.size()));
            if (!file && file.gcount() != static_cast<std::streamsize>(contents.size()))
            {
                throw std::runtime_error("Gameplay startup failed: unable to read audio file");
            }

            return contents;
#endif
        }

        /**
         * @brief Verifies that encoded audio bytes start with the Ogg container capture pattern.
         * @param contents Encoded audio bytes read from the verified asset file.
         * @throws std::runtime_error when the bytes are too small or not Ogg data.
         * @note Filename extension is not trusted as the decoder-surface restriction.
         */
        void validateOggAudioSignature(const std::vector<std::byte>& contents)
        {
            constexpr std::byte OGG_BYTE_0{static_cast<unsigned char>('O')};
            constexpr std::byte OGG_BYTE_1{static_cast<unsigned char>('g')};
            constexpr std::byte OGG_BYTE_2{static_cast<unsigned char>('g')};
            constexpr std::byte OGG_BYTE_3{static_cast<unsigned char>('S')};
            if (contents.size() < 4U || contents[0] != OGG_BYTE_0 || contents[1] != OGG_BYTE_1 ||
                contents[2] != OGG_BYTE_2 || contents[3] != OGG_BYTE_3)
            {
                throw std::runtime_error("Gameplay startup failed: audio file is not an Ogg container");
            }
        }

        /**
         * @brief Converts AudioEngine playback seconds into GameplaySession microseconds.
         * @param playbackSeconds Playback cursor in seconds.
         * @return Playback cursor in microseconds.
         * @throws std::runtime_error when the value is not representable for gameplay timing.
         */
        std::chrono::microseconds playbackSecondsToMicroseconds(double playbackSeconds)
        {
            if (!std::isfinite(playbackSeconds) || playbackSeconds < 0.0)
            {
                throw std::runtime_error("Gameplay runtime failed: playback time is invalid");
            }

            constexpr long double MICROSECONDS_PER_SECOND = 1'000'000.0L;
            const long double microseconds = std::round(static_cast<long double>(playbackSeconds) * MICROSECONDS_PER_SECOND);
            const long double maxMicroseconds = static_cast<long double>(std::chrono::microseconds::max().count());
            if (!std::isfinite(microseconds) || microseconds > maxMicroseconds)
            {
                throw std::runtime_error("Gameplay runtime failed: playback time exceeds the supported range");
            }

            return std::chrono::microseconds{static_cast<std::chrono::microseconds::rep>(microseconds)};
        }

        /**
         * @brief Returns a positive viewport size for gameplay command generation.
         * @param window Platform window that owns the drawable surface.
         * @param fallbackWidth Fallback width from startup options when the platform reports zero size.
         * @param fallbackHeight Fallback height from startup options when the platform reports zero size.
         * @return Drawable size when available, otherwise logical window size or configured fallback size.
         * @throws std::runtime_error when no positive viewport can be produced.
         */
        std::pair<int, int> getGameplayViewportSize(
            const Platform::Window& window,
            int fallbackWidth,
            int fallbackHeight)
        {
            const auto [drawableWidth, drawableHeight] = window.getDrawableSize();
            if (drawableWidth > 0 && drawableHeight > 0)
            {
                return {drawableWidth, drawableHeight};
            }

            const auto [windowWidth, windowHeight] = window.getSize();
            if (windowWidth > 0 && windowHeight > 0)
            {
                return {windowWidth, windowHeight};
            }
            if (fallbackWidth > 0 && fallbackHeight > 0)
            {
                return {fallbackWidth, fallbackHeight};
            }

            throw std::runtime_error("Gameplay runtime failed: viewport dimensions are invalid");
        }

        /**
         * @brief Reports whether a renderer status means Core should delay and keep polling events.
         * @param status Renderer frame status returned by Render::Renderer.
         * @return True when the frame did not submit but runtime can continue.
         */
        bool shouldContinueAfterRenderStatus(Render::RendererFrameStatus status) noexcept
        {
            return status == Render::RendererFrameStatus::NoDrawable ||
                   status == Render::RendererFrameStatus::RecoveringSwapchain ||
                   status == Render::RendererFrameStatus::RenderStalled;
        }

        /**
         * @brief Reports whether a renderer status means runtime must fail instead of continuing.
         * @param status Renderer frame status returned by Render::Renderer.
         * @return True when Vulkan reported a fatal renderer condition.
         */
        bool isFatalRenderStatus(Render::RendererFrameStatus status) noexcept
        {
            return status == Render::RendererFrameStatus::DeviceLost || status == Render::RendererFrameStatus::FatalError;
        }

        /**
         * @brief Converts renderer frame status to concise diagnostics.
         * @param status Renderer frame status returned by Render::Renderer.
         * @return Static text describing the status.
         */
        const char* describeRenderStatus(Render::RendererFrameStatus status) noexcept
        {
            switch (status)
            {
            case Render::RendererFrameStatus::Submitted:
                return "submitted";
            case Render::RendererFrameStatus::NoDrawable:
                return "no drawable";
            case Render::RendererFrameStatus::RecoveringSwapchain:
                return "recovering swapchain";
            case Render::RendererFrameStatus::RenderStalled:
                return "render stalled";
            case Render::RendererFrameStatus::DeviceLost:
                return "device lost";
            case Render::RendererFrameStatus::FatalError:
                return "fatal error";
            default:
                return "unknown";
            }
        }

        /**
         * @brief Converts Core application options into Platform window options.
         * @param options Core application startup configuration.
         * @return Platform window creation options.
         */
        Platform::WindowOptions makeWindowOptions(const ApplicationOptions& options)
        {
            Platform::WindowOptions windowOptions{};
            windowOptions.title = options.windowTitle;
            windowOptions.width = options.windowWidth;
            windowOptions.height = options.windowHeight;
            windowOptions.startHidden = options.startHidden;
            windowOptions.resizable = options.resizable;
            windowOptions.borderlessFullscreen = options.borderlessFullscreen;
            windowOptions.vulkanSurface = options.vulkanSurface;
            return windowOptions;
        }
    }

    struct Application::Impl
    {
        /** @brief Platform window owned by the running application. */
        std::unique_ptr<Platform::Window> window;

        /** @brief Renderer owned by the running application and destroyed before the window. */
        std::unique_ptr<Render::Renderer> renderer;

        /** @brief SDL global event pump used by the running application. */
        Platform::EventPump eventPump{};

        /** @brief Frame-local input snapshot updated by translated input events. */
        Platform::InputState inputState{};

        /** @brief Audio playback engine used as the gameplay song clock. */
        std::unique_ptr<Audio::AudioEngine> audioEngine;

        /** @brief Runtime gameplay pipeline that converts input and song time into render commands. */
        std::unique_ptr<GameplayRuntimeDriver> gameplayRuntime;

        /** @brief True when audio was paused because the renderer has no drawable surface. */
        bool audioPausedForRender = false;
    };

    Application::Application() = default;

    Application::Application(const ApplicationOptions& options)
        : m_options(options)
    {
    }

    Application::~Application() = default;

    int Application::run()
    {
        if (m_state == ApplicationState::Running)
        {
            return APPLICATION_FAILURE_EXIT_CODE;
        }

        if (m_quitRequested)
        {
            m_state = ApplicationState::Stopped;
            return APPLICATION_SUCCESS_EXIT_CODE;
        }

        try
        {
            SdlVideoSession sdlVideoSession{};
            if (!sdlVideoSession.isInitialized())
            {
                std::cerr << "SDL video subsystem is unavailable: " << SDL_GetError() << std::endl;
                m_state = ApplicationState::Failed;
                return APPLICATION_FAILURE_EXIT_CODE;
            }

            Impl impl{};
            impl.window = std::make_unique<Platform::Window>(makeWindowOptions(m_options));
            impl.renderer = std::make_unique<Render::Renderer>(*impl.window);

            Game::ChartLoadOptions chartLoadOptions{};
            chartLoadOptions.assetRoot = resolveRuntimeAssetRoot(m_options.assetRoot);
            const Game::ChartLoader chartLoader{chartLoadOptions};
            const Game::Chart chart = chartLoader.loadFromFile(m_options.startupChartPath);
            const std::filesystem::path audioPath = resolveAudioAssetPath(chartLoader.getAssetRoot(), chart.meta.audioPath);
            std::vector<std::byte> audioBytes = readAudioAssetBytesNoFollow(chartLoader.getAssetRoot(), audioPath);
            validateOggAudioSignature(audioBytes);

            impl.audioEngine = std::make_unique<Audio::AudioEngine>();
            if (!impl.audioEngine->isBackendAvailable())
            {
                throw std::runtime_error("Gameplay startup failed: audio backend is unavailable");
            }
            if (!impl.audioEngine->loadMusicFromMemory(std::move(audioBytes)))
            {
                throw std::runtime_error("Gameplay startup failed: unable to load music track '" + chart.meta.audioPath + "'");
            }

            Game::GameplaySessionConfig sessionConfig{};
            sessionConfig.scrollLeadTime = std::chrono::milliseconds{500};
            impl.gameplayRuntime = std::make_unique<GameplayRuntimeDriver>(chart, sessionConfig);
            const auto [initialViewportWidth, initialViewportHeight] = getGameplayViewportSize(
                *impl.window,
                m_options.windowWidth,
                m_options.windowHeight);
            impl.renderer->submit2DCommands(impl.gameplayRuntime->update(
                std::chrono::microseconds{0},
                impl.inputState,
                initialViewportWidth,
                initialViewportHeight));

            /**
             * @brief Applies a translated window event to Core-owned platform and render state.
             * @note Focus transitions rebaseline input so stale held/edge state cannot survive startup focus races.
             */
            const auto handleRuntimeWindowEvent = [&impl](const Platform::WindowEvent& event) noexcept
            {
                impl.window->applyEvent(event);
                impl.renderer->handleWindowEvent(event);
                if (event.type == Platform::WindowEventType::FocusLost ||
                    event.type == Platform::WindowEventType::FocusGained)
                {
                    impl.inputState.clear();
                }
            };

            /**
             * @brief Drains startup window events after showing the window and before audio playback begins.
             * @note Focus requests are best-effort and bounded because operating systems may deny foreground activation.
             */
            const auto prepareStartupWindowInteraction = [&impl, &handleRuntimeWindowEvent]() noexcept
            {
                if (!impl.window->hasInputFocus())
                {
                    (void)impl.window->requestInputFocus();
                }

                SDL_PumpEvents();
                Platform::PlatformEvent event{};
                while (impl.eventPump.pollEvent(event, impl.window->getWindowId()))
                {
                    if (event.type == Platform::PlatformEventType::Window)
                    {
                        handleRuntimeWindowEvent(event.window);
                        if (impl.window->shouldClose())
                        {
                            break;
                        }
                    }
                }
            };

            impl.window->show();
            prepareStartupWindowInteraction();
            if (impl.window->shouldClose())
            {
                m_state = ApplicationState::Stopped;
                return APPLICATION_SUCCESS_EXIT_CODE;
            }

            if (!impl.audioEngine->play())
            {
                throw std::runtime_error("Gameplay startup failed: unable to start music playback");
            }

            m_timeSystem.reset();
            m_quitRequested = false;
            m_state = ApplicationState::Initialized;
            m_state = ApplicationState::Running;

            while (!m_quitRequested && !impl.window->shouldClose())
            {
                m_timeSystem.beginFrame();
                impl.inputState.beginFrame();

                // EventPump reads with SDL_PeepEvents(), which does not implicitly gather OS window messages.
                // Pump here each frame so Windows keeps receiving input, close, and responsiveness messages.
                SDL_PumpEvents();

                Platform::PlatformEvent event{};
                std::vector<Platform::InputEvent> frameInputEvents;
                std::vector<Game::LanePressEvent> pressEvents;
                for (int eventsProcessed = 0;
                     eventsProcessed < MAX_EVENTS_PER_FRAME &&
                     impl.eventPump.pollEvent(event, impl.window->getWindowId());
                     ++eventsProcessed)
                {
                    if (event.type == Platform::PlatformEventType::Window)
                    {
                        handleRuntimeWindowEvent(event.window);
                    }
                    else if (event.type == Platform::PlatformEventType::Input)
                    {
                        impl.inputState.handleInputEvent(event.input);
                        frameInputEvents.push_back(event.input);
                    }
                }

                const std::chrono::microseconds frameSongTime = playbackSecondsToMicroseconds(
                    impl.audioEngine->getPlaybackTimeSeconds());
                const std::uint64_t frameTimestampNanoseconds = SDL_GetTicksNS();
                for (const Platform::InputEvent& inputEvent : frameInputEvents)
                {
                    if (const std::optional<Game::LanePressEvent> pressEvent =
                            impl.gameplayRuntime->makeLanePressEvent(
                                inputEvent,
                                frameSongTime,
                                frameTimestampNanoseconds))
                    {
                        pressEvents.push_back(*pressEvent);
                    }
                }

                const auto [drawableWidth, drawableHeight] = impl.window->getDrawableSize();
                if (drawableWidth <= 0 || drawableHeight <= 0)
                {
                    const bool shouldPauseAudio = !impl.audioPausedForRender &&
                        impl.audioEngine->getState() == Audio::PlaybackState::Playing;
                    if (shouldPauseAudio && !impl.audioEngine->pause())
                    {
                        throw std::runtime_error("Gameplay runtime failed: unable to pause music while rendering is unavailable");
                    }
                    impl.audioPausedForRender = impl.audioPausedForRender || shouldPauseAudio;

                    const Render::RendererFrameStatus renderStatus = impl.renderer->renderFrame();
                    if (shouldContinueAfterRenderStatus(renderStatus))
                    {
                        SDL_Delay(PAUSED_RENDER_DELAY_MILLISECONDS);
                    }
                    else if (isFatalRenderStatus(renderStatus))
                    {
                        throw std::runtime_error(
                            std::string("Gameplay runtime failed: renderer cannot continue while drawable is unavailable: ") +
                            describeRenderStatus(renderStatus));
                    }
                    continue;
                }

                const auto [viewportWidth, viewportHeight] = getGameplayViewportSize(
                    *impl.window,
                    m_options.windowWidth,
                    m_options.windowHeight);
                impl.renderer->submit2DCommands(impl.gameplayRuntime->update(
                    frameSongTime,
                    impl.inputState,
                    viewportWidth,
                    viewportHeight,
                    pressEvents));

                const Render::RendererFrameStatus renderStatus = impl.renderer->renderFrame();
                if (renderStatus == Render::RendererFrameStatus::Submitted)
                {
                    if (impl.audioPausedForRender)
                    {
                        if (!impl.audioEngine->play())
                        {
                            throw std::runtime_error("Gameplay runtime failed: unable to resume music after rendering resumed");
                        }
                        impl.audioPausedForRender = false;
                    }
                }
                else if (shouldContinueAfterRenderStatus(renderStatus))
                {
                    SDL_Delay(PAUSED_RENDER_DELAY_MILLISECONDS);
                }
                else if (isFatalRenderStatus(renderStatus))
                {
                    throw std::runtime_error(
                        std::string("Gameplay runtime failed: renderer cannot continue: ") +
                        describeRenderStatus(renderStatus));
                }
            }

            m_state = ApplicationState::Stopping;
            const Render::RendererWaitStatus shutdownWaitStatus = impl.renderer->waitIdle();
            if (shutdownWaitStatus != Render::RendererWaitStatus::Ready)
            {
                std::cerr << "Renderer shutdown wait did not complete before timeout; leaking renderer and window handles to avoid unsafe Vulkan teardown." << std::endl;
                (void)impl.renderer.release();
                (void)impl.window.release();
                m_state = ApplicationState::Failed;
                return APPLICATION_FAILURE_EXIT_CODE;
            }
            impl.renderer.reset();
            impl.window.reset();
            m_state = ApplicationState::Stopped;
            return APPLICATION_SUCCESS_EXIT_CODE;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "RaythmDemo failed: " << exception.what() << std::endl;
            m_state = ApplicationState::Failed;
            return APPLICATION_FAILURE_EXIT_CODE;
        }
    }

    void Application::requestQuit() noexcept
    {
        m_quitRequested = true;
        if (m_state == ApplicationState::Created || m_state == ApplicationState::Initialized)
        {
            m_state = ApplicationState::Stopping;
        }
    }

    bool Application::isQuitRequested() const noexcept
    {
        return m_quitRequested;
    }

    ApplicationState Application::getState() const noexcept
    {
        return m_state;
    }

    const FrameTime& Application::getCurrentFrameTime() const noexcept
    {
        return m_timeSystem.getCurrentFrameTime();
    }
}
