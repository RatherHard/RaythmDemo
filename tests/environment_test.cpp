// RaythmDemo - Development Environment Test
// Verifies that core third-party dependencies initialize and perform basic operations.
// Author: RatherHard
// Date: 2026-07-04

#include <iostream>
#include <string>

// Test headers
#include "miniaudio.h"
#include <volk.h>
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Verifies that the miniaudio engine can be initialized and shut down.
 * @return True when miniaudio reports successful engine lifecycle operations.
 * @note This smoke test exercises only library availability, not real-time audio latency.
 */
bool testMiniaudio()
{
    std::cout << "\n=== Testing miniaudio ===" << std::endl;

    ma_result result;
    ma_engine engine;

    result = ma_engine_init(nullptr, &engine);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Failed to initialize miniaudio engine." << std::endl;
        return false;
    }

    std::cout << "miniaudio engine initialized successfully" << std::endl;

    ma_engine_uninit(&engine);
    std::cout << "miniaudio engine uninitialized successfully" << std::endl;

    return true;
}

/**
 * @brief Verifies that volk can load Vulkan entry points and query the instance version.
 * @return True when volk initialization succeeds and Vulkan version querying is callable.
 * @note volkInitialize must remain the first Vulkan call in this test.
 */
bool testVulkan()
{
    std::cout << "\n=== Testing Vulkan/volk ===" << std::endl;

    VkResult result = volkInitialize();
    if (result != VK_SUCCESS)
    {
        std::cerr << "Failed to initialize volk. Error code: " << result << std::endl;
        return false;
    }

    std::cout << "volk initialized successfully" << std::endl;

    uint32_t instanceVersion = 0;
    vkEnumerateInstanceVersion(&instanceVersion);

    uint32_t major = VK_VERSION_MAJOR(instanceVersion);
    uint32_t minor = VK_VERSION_MINOR(instanceVersion);
    uint32_t patch = VK_VERSION_PATCH(instanceVersion);

    std::cout << "Vulkan version: " << major << "." << minor << "." << patch << std::endl;

    return true;
}

/**
 * @brief Verifies that SDL3 can initialize video/audio subsystems and create a hidden test window.
 * @return True when SDL initialization and window lifecycle operations succeed.
 * @note The window is hidden so the smoke test can run without interrupting the developer desktop.
 */
bool testSDL3()
{
    std::cout << "\n=== Testing SDL3 ===" << std::endl;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        std::cerr << "Failed to initialize SDL3: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "SDL3 initialized successfully" << std::endl;

    // Record the linked SDL version so environment issues are visible in test output.
    int version = SDL_GetVersion();
    int major = SDL_VERSIONNUM_MAJOR(version);
    int minor = SDL_VERSIONNUM_MINOR(version);
    int patch = SDL_VERSIONNUM_MICRO(version);

    std::cout << "SDL3 version: " << major << "." << minor << "." << patch << std::endl;

    // Create a hidden window to validate the platform video backend without flashing UI.
    SDL_Window* window = SDL_CreateWindow(
        "RaythmDemo - Environment Test",
        640, 480,
        SDL_WINDOW_HIDDEN
    );

    if (!window)
    {
        std::cerr << "Failed to create SDL3 window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    std::cout << "SDL3 window created successfully" << std::endl;

    SDL_DestroyWindow(window);
    SDL_Quit();

    return true;
}

/**
 * @brief Verifies that nlohmann/json can create, serialize, parse, and read project-shaped data.
 * @return True when the JSON round trip succeeds without throwing a nlohmann/json exception.
 */
bool testJSON()
{
    std::cout << "\n=== Testing nlohmann/json ===" << std::endl;

    try
    {
        // Model a small project metadata payload to exercise nested objects and arrays.
        json testObject = {
            {"project", "RaythmDemo"},
            {"language", "C++20"},
            {"dependencies", {
                {"audio", "miniaudio"},
                {"graphics", "Vulkan"},
                {"window", "SDL3"}
            }},
            {"version", {1, 0, 0}}
        };

        std::cout << "JSON object created successfully" << std::endl;

        // Serialize and parse to verify both directions of the dependency's common path.
        std::string jsonString = testObject.dump(2);
        std::cout << "JSON serialization:\n" << jsonString << std::endl;

        json parsed = json::parse(jsonString);
        std::cout << "JSON parsing successful" << std::endl;

        std::string projectName = parsed["project"];
        std::cout << "Project name: " << projectName << std::endl;

        return true;
    }
    catch (const json::exception& e)
    {
        std::cerr << "JSON test failed: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Runs all development-environment smoke tests.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Zero when all dependency checks pass; nonzero when any check fails.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================" << std::endl;
    std::cout << "RaythmDemo - Development Environment Test" << std::endl;
    std::cout << "========================================" << std::endl;

    bool allTestsPassed = true;

    allTestsPassed &= testMiniaudio();
    allTestsPassed &= testVulkan();
    allTestsPassed &= testSDL3();
    allTestsPassed &= testJSON();

    std::cout << "\n========================================" << std::endl;
    if (allTestsPassed)
    {
        std::cout << "All tests passed!" << std::endl;
        std::cout << "Development environment is configured correctly." << std::endl;
    }
    else
    {
        std::cout << "Some tests failed." << std::endl;
        std::cout << "Please check your development environment setup." << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return allTestsPassed ? 0 : 1;
}
