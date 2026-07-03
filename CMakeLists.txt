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

# Find vcpkg-managed packages
find_package(SDL3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)

# Add executable
add_executable(RaythmDemo
    src/main.cpp
    src/miniaudio_impl.cpp
)

target_include_directories(RaythmDemo PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/miniaudio
)

# Link libraries
target_link_libraries(RaythmDemo PRIVATE
    volk::volk
    SDL3::SDL3
    nlohmann_json::nlohmann_json
)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    target_compile_definitions(RaythmDemo PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )

    if(MSVC)
        target_compile_options(RaythmDemo PRIVATE /utf-8)
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    target_link_libraries(RaythmDemo PRIVATE
        ${CMAKE_DL_LIBS}
        pthread
    )
endif()

# Print configuration summary
message(STATUS "========================================")
message(STATUS "RaythmDemo Configuration Summary")
message(STATUS "========================================")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Volk (Meta-loader): Found")
message(STATUS "========================================")
