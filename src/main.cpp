// RaythmDemo - Main Entry Point
// Starts the Core application root that owns the SDL/Vulkan lifecycle.
// Author: RatherHard
// Date: 2026-07-04

#include "Core/Application.hpp"

/**
 * @brief Starts the RaythmDemo application executable.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Process exit code; zero indicates clean shutdown.
 * @note Runtime lifecycle, event routing, timing, and rendering are orchestrated by Core::Application.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    Raythm::Core::Application application{};
    return application.run();
}
