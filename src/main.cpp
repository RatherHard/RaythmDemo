// RaythmDemo - Main Entry Point
// Provides the current executable entry stub while subsystems are brought online.
// Author: RatherHard
// Date: 2026-07-04

#include <iostream>

/**
 * @brief Starts the RaythmDemo application executable.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Process exit code; zero indicates the entry stub ran successfully.
 * @note Startup is currently limited to a smoke message until the game loop is implemented.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "RaythmDemo application entry point" << std::endl;
    return 0;
}
