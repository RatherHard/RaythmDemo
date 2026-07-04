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

enable_testing()

# Find vcpkg-managed packages
find_package(SDL3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)

# Add executable
add_library(RaythmDemoPlatform
    src/Platform/EventPump.cpp
    src/Platform/InputState.cpp
    src/Platform/Window.cpp
)

target_include_directories(RaythmDemoPlatform PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(RaythmDemoPlatform PUBLIC
    volk::volk
    SDL3::SDL3
)

add_library(RaythmDemoRender
    src/Render/Renderer.cpp
)

target_include_directories(RaythmDemoRender PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(RaythmDemoRender PUBLIC
    RaythmDemoPlatform
    volk::volk
)

add_executable(RaythmDemo
    src/main.cpp
)

target_link_libraries(RaythmDemo PRIVATE
    RaythmDemoRender
)

add_executable(RaythmDemoEnvironmentTest
    tests/environment_test.cpp
    src/miniaudio_impl.cpp
)

add_executable(RaythmDemoWindowTest
    tests/window_test.cpp
)

add_executable(RaythmDemoInputStateTest
    tests/input_state_test.cpp
)

add_executable(RaythmDemoRenderTest
    tests/render_test.cpp
)

target_include_directories(RaythmDemoEnvironmentTest PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/miniaudio
)

# Link libraries
target_link_libraries(RaythmDemoEnvironmentTest PRIVATE
    volk::volk
    SDL3::SDL3
    nlohmann_json::nlohmann_json
)

target_link_libraries(RaythmDemoWindowTest PRIVATE
    RaythmDemoPlatform
)

target_link_libraries(RaythmDemoInputStateTest PRIVATE
    RaythmDemoPlatform
)

target_link_libraries(RaythmDemoRenderTest PRIVATE
    RaythmDemoRender
)

add_test(NAME RaythmDemoEnvironmentTest COMMAND RaythmDemoEnvironmentTest)
add_test(NAME RaythmDemoWindowTest COMMAND RaythmDemoWindowTest)
add_test(NAME RaythmDemoInputStateTest COMMAND RaythmDemoInputStateTest)
add_test(NAME RaythmDemoRenderTest COMMAND RaythmDemoRenderTest)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    target_compile_definitions(RaythmDemoPlatform PUBLIC
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
    target_compile_definitions(RaythmDemoRender PUBLIC
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
    target_compile_definitions(RaythmDemo PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
    target_compile_definitions(RaythmDemoEnvironmentTest PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
    target_compile_definitions(RaythmDemoWindowTest PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
    target_compile_definitions(RaythmDemoInputStateTest PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
    target_compile_definitions(RaythmDemoRenderTest PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )

    if(MSVC)
        target_compile_options(RaythmDemoPlatform PUBLIC /utf-8)
        target_compile_options(RaythmDemoRender PUBLIC /utf-8)
        target_compile_options(RaythmDemo PRIVATE /utf-8)
        target_compile_options(RaythmDemoEnvironmentTest PRIVATE /utf-8)
        target_compile_options(RaythmDemoWindowTest PRIVATE /utf-8)
        target_compile_options(RaythmDemoInputStateTest PRIVATE /utf-8)
        target_compile_options(RaythmDemoRenderTest PRIVATE /utf-8)
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    target_link_libraries(RaythmDemoPlatform PUBLIC
        ${CMAKE_DL_LIBS}
        pthread
    )
    target_link_libraries(RaythmDemoEnvironmentTest PRIVATE
        ${CMAKE_DL_LIBS}
        pthread
    )
    target_link_libraries(RaythmDemoRenderTest PRIVATE
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
