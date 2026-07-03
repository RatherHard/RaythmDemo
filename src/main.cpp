// RaythmDemo - Development Environment Test
// Tests miniaudio, Vulkan/volk, SDL3, and nlohmann/json
// Author: RatherHard
// Date: 2026-07-03

#include <iostream>
#include <string>

// Test headers
#include "miniaudio.h"
#include <volk.h>
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Test miniaudio
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

// Test Vulkan/volk
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

// Test SDL3
bool testSDL3()
{
    std::cout << "\n=== Testing SDL3 ===" << std::endl;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        std::cerr << "Failed to initialize SDL3: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "SDL3 initialized successfully" << std::endl;

    // Get SDL version
    int version = SDL_GetVersion();
    int major = SDL_VERSIONNUM_MAJOR(version);
    int minor = SDL_VERSIONNUM_MINOR(version);
    int patch = SDL_VERSIONNUM_MICRO(version);

    std::cout << "SDL3 version: " << major << "." << minor << "." << patch << std::endl;

    // Create a test window
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

// Test nlohmann/json
bool testJSON()
{
    std::cout << "\n=== Testing nlohmann/json ===" << std::endl;

    try
    {
        // Create a test JSON object
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

        // Serialize to string
        std::string jsonString = testObject.dump(2);
        std::cout << "JSON serialization:\n" << jsonString << std::endl;

        // Parse from string
        json parsed = json::parse(jsonString);
        std::cout << "JSON parsing successful" << std::endl;

        // Access values
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

int main(int argc, char* argv[])
{
    std::cout << "========================================" << std::endl;
    std::cout << "RaythmDemo - Development Environment Test" << std::endl;
    std::cout << "========================================" << std::endl;

    bool allTestsPassed = true;

    // Run all tests
    allTestsPassed &= testMiniaudio();
    allTestsPassed &= testVulkan();
    allTestsPassed &= testSDL3();
    allTestsPassed &= testJSON();

    // Print summary
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
