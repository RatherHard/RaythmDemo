// RaythmDemo - Render Strategy Tests
// Verifies GPU-independent swapchain selection policies and minimal 2D command clipping.
// Author: RatherHard
// Date: 2026-07-04

#include "Render/RenderStrategy.hpp"

#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
    namespace Render = Raythm::Render;

    /**
     * @brief Records a failed expectation without aborting the current test function.
     * @param condition Boolean result produced by the assertion expression.
     * @param message Human-readable failure message printed when the condition is false.
     * @return The original condition so callers can accumulate pass/fail state.
     */
    bool expect(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << std::endl;
            return false;
        }

        return true;
    }

    /**
     * @brief Verifies surface format selection prefers SRGB BGRA and falls back predictably.
     * @return True when format selection behaves as expected.
     */
    bool testSurfaceFormatSelection()
    {
        const std::vector<VkSurfaceFormatKHR> formats = {
            {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
        };
        const VkSurfaceFormatKHR selected = Render::chooseSurfaceFormat(formats);

        bool passed = true;
        passed &= expect(selected.format == VK_FORMAT_B8G8R8A8_SRGB, "should prefer BGRA SRGB surface format");
        passed &= expect(
            selected.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            "should preserve selected surface color space");

        const std::vector<VkSurfaceFormatKHR> fallbackFormats = {{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
        passed &= expect(
            Render::chooseSurfaceFormat(fallbackFormats).format == VK_FORMAT_R8G8B8A8_UNORM,
            "should fall back to first format when preferred format is absent");

        try
        {
            (void)Render::chooseSurfaceFormat({});
            passed &= expect(false, "empty surface formats should throw");
        }
        catch (const std::runtime_error&)
        {
            passed &= expect(true, "empty surface formats throw runtime_error");
        }

        return passed;
    }

    /**
     * @brief Verifies present mode selection honors vsync and low-latency preferences.
     * @return True when present mode selection behaves as expected.
     */
    bool testPresentModeSelection()
    {
        const std::vector<VkPresentModeKHR> modes = {
            VK_PRESENT_MODE_IMMEDIATE_KHR,
            VK_PRESENT_MODE_MAILBOX_KHR,
            VK_PRESENT_MODE_FIFO_KHR
        };

        bool passed = true;
        passed &= expect(
            Render::choosePresentMode(modes, true) == VK_PRESENT_MODE_FIFO_KHR,
            "vsync preference should choose FIFO");
        passed &= expect(
            Render::choosePresentMode(modes, false) == VK_PRESENT_MODE_MAILBOX_KHR,
            "non-vsync preference should choose MAILBOX when available");
        passed &= expect(
            Render::choosePresentMode({VK_PRESENT_MODE_FIFO_KHR}, false) == VK_PRESENT_MODE_FIFO_KHR,
            "non-vsync preference should fall back to FIFO");

        try
        {
            (void)Render::choosePresentMode({}, true);
            passed &= expect(false, "empty present modes should throw");
        }
        catch (const std::runtime_error&)
        {
            passed &= expect(true, "empty present modes throw runtime_error");
        }

        return passed;
    }

    /**
     * @brief Verifies swapchain extent selection handles fixed and variable surface extents.
     * @return True when extent selection behaves as expected.
     */
    bool testSwapchainExtentSelection()
    {
        VkSurfaceCapabilitiesKHR fixedCapabilities{};
        fixedCapabilities.currentExtent = {1920, 1080};

        VkSurfaceCapabilitiesKHR variableCapabilities{};
        variableCapabilities.currentExtent = {UINT32_MAX, UINT32_MAX};
        variableCapabilities.minImageExtent = {320, 240};
        variableCapabilities.maxImageExtent = {1280, 720};

        const VkExtent2D fixed = Render::chooseSwapchainExtent(fixedCapabilities, {640, 480});
        const VkExtent2D clampedHigh = Render::chooseSwapchainExtent(variableCapabilities, {2000, 1000});
        const VkExtent2D clampedLow = Render::chooseSwapchainExtent(variableCapabilities, {-1, 10});
        const VkExtent2D insideRange = Render::chooseSwapchainExtent(variableCapabilities, {800, 600});

        bool passed = true;
        passed &= expect(fixed.width == 1920 && fixed.height == 1080, "fixed surface extent should win");
        passed &= expect(clampedHigh.width == 1280 && clampedHigh.height == 720, "large drawable should clamp to max");
        passed &= expect(clampedLow.width == 320 && clampedLow.height == 240, "small drawable should clamp to min");
        passed &= expect(insideRange.width == 800 && insideRange.height == 600, "in-range drawable should be preserved");
        return passed;
    }

    /**
     * @brief Verifies queue family selection finds complete and incomplete queue support.
     * @return True when queue family selection behaves as expected.
     */
    bool testQueueFamilySelection()
    {
        const Render::QueueFamilySelection splitSelection = Render::chooseQueueFamilies(
            {0, VK_QUEUE_GRAPHICS_BIT, 0},
            {VK_FALSE, VK_FALSE, VK_TRUE});
        const Render::QueueFamilySelection sameSelection = Render::chooseQueueFamilies(
            {VK_QUEUE_GRAPHICS_BIT},
            {VK_TRUE});
        const Render::QueueFamilySelection incompleteSelection = Render::chooseQueueFamilies(
            {VK_QUEUE_COMPUTE_BIT},
            {VK_FALSE});

        bool passed = true;
        passed &= expect(splitSelection.isComplete(), "split graphics/present queues should be complete");
        passed &= expect(splitSelection.graphicsFamily == 1U, "graphics family should use first graphics queue");
        passed &= expect(splitSelection.presentFamily == 2U, "present family should use first present queue");
        passed &= expect(sameSelection.graphicsFamily == 0U && sameSelection.presentFamily == 0U, "same queue can satisfy both roles");
        passed &= expect(!incompleteSelection.isComplete(), "missing graphics or present support should be incomplete");
        return passed;
    }

    /**
     * @brief Verifies command rectangle clipping against framebuffer extents.
     * @return True when clipping accepts visible rectangles and rejects empty ones.
     */
    bool testRenderCommandClipping()
    {
        const VkExtent2D extent{100, 80};
        const auto inside = Render::clipRectToExtent({10, 12, 30, 20}, extent);
        const auto clipped = Render::clipRectToExtent({90, 70, 30, 30}, extent);
        const auto outside = Render::clipRectToExtent({110, 0, 10, 10}, extent);
        const auto empty = Render::clipRectToExtent({0, 0, 0, 10}, extent);
        const auto overflow = Render::clipRectToExtent({std::numeric_limits<int>::max(), 0, 100, 10}, extent);

        bool passed = true;
        passed &= expect(inside.has_value(), "inside rectangle should remain drawable");
        passed &= expect(inside->offset.x == 10 && inside->offset.y == 12, "inside rectangle offset should be preserved");
        passed &= expect(inside->extent.width == 30 && inside->extent.height == 20, "inside rectangle extent should be preserved");
        passed &= expect(clipped.has_value(), "partially visible rectangle should remain drawable");
        passed &= expect(clipped->extent.width == 10 && clipped->extent.height == 10, "partially visible rectangle should be clipped");
        passed &= expect(!outside.has_value(), "outside rectangle should be rejected");
        passed &= expect(!empty.has_value(), "empty rectangle should be rejected");
        passed &= expect(!overflow.has_value(), "overflow-edge rectangle should be rejected without wrapping");
        return passed;
    }
}

/**
 * @brief Runs GPU-independent Render strategy tests.
 * @param argc Number of command-line arguments supplied by the platform runtime.
 * @param argv Command-line argument values supplied by the platform runtime.
 * @return Zero when all pure Render strategy checks pass; nonzero otherwise.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    bool allTestsPassed = true;
    allTestsPassed &= testSurfaceFormatSelection();
    allTestsPassed &= testPresentModeSelection();
    allTestsPassed &= testSwapchainExtentSelection();
    allTestsPassed &= testQueueFamilySelection();
    allTestsPassed &= testRenderCommandClipping();

    return allTestsPassed ? 0 : 1;
}
