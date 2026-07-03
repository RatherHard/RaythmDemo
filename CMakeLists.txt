cmake_minimum_required(VERSION 3.20)

project(RaythmDemo
    VERSION 0.1.0
    DESCRIPTION "Cross-platform rhythm game built with C++20"
    LANGUAGES CXX
)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# Find vcpkg-managed packages
find_package(SDL3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Add source directory (to be created later)
# add_subdirectory(src)

# Print configuration summary
message(STATUS "========================================")
message(STATUS "RaythmDemo Configuration Summary")
message(STATUS "========================================")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Vulkan Include: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan Library: ${Vulkan_LIBRARIES}")
message(STATUS "========================================")
