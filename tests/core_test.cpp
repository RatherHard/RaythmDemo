// RaythmDemo - Core Infrastructure Tests
// Verifies frame timing and application lifecycle state used by the main loop.
// Author: RatherHard
// Date: 2026-07-04

#include "Core/Application.hpp"
#include "Core/Time.hpp"

#include <chrono>
#include <iostream>
#include <string>

namespace
{
    namespace Core = Raythm::Core;

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

    /** @brief Manual monotonic clock used to make time-system tests deterministic. */
    class ManualClock
    {
    public:
        /** @brief Returns the current synthetic time point. */
        Core::TimePoint now() const noexcept
        {
            return m_currentTime;
        }

        /** @brief Advances the synthetic clock by the supplied duration. */
        void advance(Core::Duration duration) noexcept
        {
            m_currentTime += duration;
        }

    private:
        /** @brief Current synthetic monotonic time. */
        Core::TimePoint m_currentTime{};
    };

    /** @brief Verifies the first frame starts with zero delta and frame index zero. */
    bool testTimeSystemStartsAtFrameZero()
    {
        ManualClock clock{};
        Core::TimeSystem timeSystem([&clock]() noexcept {
            return clock.now();
        });

        const Core::FrameTime frameTime = timeSystem.beginFrame();

        bool passed = true;
        passed &= expect(frameTime.frameIndex == 0, "first frame index should be zero");
        passed &= expectNear(frameTime.deltaSeconds, 0.0, 0.000001, "first frame delta should be zero");
        passed &= expectNear(frameTime.elapsedSeconds, 0.0, 0.000001, "first frame elapsed time should be zero");
        passed &= expect(!frameTime.wasDeltaClamped, "first frame delta should not be clamped");
        return passed;
    }

    /** @brief Verifies delta, elapsed time, and frame counter advance from a monotonic clock. */
    bool testTimeSystemAdvancesDeltaElapsedAndFrameCounter()
    {
        ManualClock clock{};
        Core::TimeSystem timeSystem([&clock]() noexcept {
            return clock.now();
        });

        timeSystem.beginFrame();
        clock.advance(std::chrono::milliseconds(16));
        const Core::FrameTime secondFrame = timeSystem.beginFrame();
        clock.advance(std::chrono::milliseconds(8));
        const Core::FrameTime thirdFrame = timeSystem.beginFrame();

        bool passed = true;
        passed &= expect(secondFrame.frameIndex == 1, "second frame index should be one");
        passed &= expectNear(secondFrame.deltaSeconds, 0.016, 0.000001, "second frame delta should be 16 ms");
        passed &= expectNear(secondFrame.elapsedSeconds, 0.016, 0.000001, "elapsed time should include first advance");
        passed &= expect(thirdFrame.frameIndex == 2, "third frame index should be two");
        passed &= expectNear(thirdFrame.deltaSeconds, 0.008, 0.000001, "third frame delta should be 8 ms");
        passed &= expectNear(thirdFrame.elapsedSeconds, 0.024, 0.000001, "elapsed time should accumulate all advances");
        return passed;
    }

    /** @brief Verifies long stalls are clamped for future simulation consumers. */
    bool testTimeSystemClampsLargeDelta()
    {
        ManualClock clock{};
        Core::TimeSystem timeSystem([&clock]() noexcept {
            return clock.now();
        });

        timeSystem.beginFrame();
        clock.advance(std::chrono::seconds(2));
        const Core::FrameTime frameTime = timeSystem.beginFrame();

        bool passed = true;
        passed &= expect(frameTime.frameIndex == 1, "clamped frame should still advance frame index");
        passed &= expectNear(frameTime.deltaSeconds, Core::DEFAULT_MAX_DELTA_SECONDS, 0.000001,
                             "large delta should clamp to default maximum");
        passed &= expectNear(frameTime.elapsedSeconds, 2.0, 0.000001,
                             "elapsed time should preserve unclamped monotonic duration");
        passed &= expect(frameTime.wasDeltaClamped, "large delta should report clamped state");
        return passed;
    }

    /** @brief Verifies reset starts frame timing from the current clock sample. */
    bool testTimeSystemResetRestartsFrameCounter()
    {
        ManualClock clock{};
        Core::TimeSystem timeSystem([&clock]() noexcept {
            return clock.now();
        });

        timeSystem.beginFrame();
        clock.advance(std::chrono::milliseconds(10));
        timeSystem.beginFrame();
        clock.advance(std::chrono::milliseconds(5));
        timeSystem.reset();
        const Core::FrameTime frameTime = timeSystem.beginFrame();

        bool passed = true;
        passed &= expect(frameTime.frameIndex == 0, "reset frame index should return to zero");
        passed &= expectNear(frameTime.deltaSeconds, 0.0, 0.000001, "reset first delta should be zero");
        passed &= expectNear(frameTime.elapsedSeconds, 0.0, 0.000001, "reset elapsed time should be zero");
        return passed;
    }

    /** @brief Verifies a pre-run quit request exits without bootstrapping SDL or Vulkan resources. */
    bool testApplicationRunHonorsPreRunQuitRequest()
    {
        Core::Application application{};
        application.requestQuit();

        const int result = application.run();

        bool passed = true;
        passed &= expect(result == Core::APPLICATION_SUCCESS_EXIT_CODE,
                         "pre-run requestQuit should make run return success");
        passed &= expect(application.getState() == Core::ApplicationState::Stopped,
                         "pre-run requestQuit should leave application stopped");
        return passed;
    }

    /** @brief Verifies Application defaults are suitable for the current SDL/Vulkan main loop. */
    bool testApplicationOptionsDefaultsMatchCurrentMainLoop()
    {
        const Core::ApplicationOptions options{};

        bool passed = true;
        passed &= expect(options.windowTitle == "RaythmDemo", "default window title should match executable");
        passed &= expect(options.windowWidth == 1280, "default window width should be 1280");
        passed &= expect(options.windowHeight == 720, "default window height should be 720");
        passed &= expect(options.startHidden, "default application window should start hidden before renderer init");
        passed &= expect(options.resizable, "default application window should be resizable");
        passed &= expect(options.vulkanSurface, "default application window should support Vulkan surfaces");
        passed &= expect(options.assetRoot == "assets", "default asset root should use checked-in runtime assets");
        passed &= expect(options.startupChartPath == "charts/00001/00001.json",
                         "default startup chart should use checked-in sample chart");
        return passed;
    }

    /** @brief Verifies explicit quit requests transition the application toward a clean stop. */
    bool testApplicationRequestQuitChangesState()
    {
        Core::Application application{};
        bool passed = true;

        passed &= expect(application.getState() == Core::ApplicationState::Created,
                         "new application should start in Created state");
        passed &= expect(!application.isQuitRequested(), "new application should not request quit");

        application.requestQuit();
        passed &= expect(application.isQuitRequested(), "requestQuit should set quit flag");
        passed &= expect(application.getState() == Core::ApplicationState::Stopping,
                         "requestQuit before run should move application to Stopping");
        return passed;
    }
}

/**
 * @brief Runs all Core infrastructure tests.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Zero when all Core checks pass; nonzero when any check fails.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    bool allTestsPassed = true;
    allTestsPassed &= testTimeSystemStartsAtFrameZero();
    allTestsPassed &= testTimeSystemAdvancesDeltaElapsedAndFrameCounter();
    allTestsPassed &= testTimeSystemClampsLargeDelta();
    allTestsPassed &= testTimeSystemResetRestartsFrameCounter();
    allTestsPassed &= testApplicationRunHonorsPreRunQuitRequest();
    allTestsPassed &= testApplicationOptionsDefaultsMatchCurrentMainLoop();
    allTestsPassed &= testApplicationRequestQuitChangesState();

    return allTestsPassed ? 0 : 1;
}
