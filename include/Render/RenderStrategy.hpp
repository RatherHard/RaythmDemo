// RaythmDemo - Render Strategy Helpers
// Declares GPU-independent Vulkan selection policies and minimal 2D render command types.
// Author: RatherHard
// Date: 2026-07-04

#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <volk.h>

namespace Raythm::Render
{
    /**
     * @brief Result of bounded Vulkan waits used to keep the runtime event pump responsive.
     */
    enum class RendererWaitStatus
    {
        /** @brief The waited GPU work completed before the timeout. */
        Ready,

        /** @brief The bounded wait elapsed before completion. */
        TimedOut,

        /** @brief Vulkan reported device loss while waiting. */
        DeviceLost,

        /** @brief Vulkan reported another unrecoverable wait failure. */
        FatalError
    };

    /**
     * @brief High-level renderer frame status consumed by Core runtime orchestration.
     */
    enum class RendererFrameStatus
    {
        /** @brief A frame was submitted and presented normally. */
        Submitted,

        /** @brief Rendering was skipped because the drawable surface has no positive size. */
        NoDrawable,

        /** @brief Swapchain recovery was requested or completed without a fatal error. */
        RecoveringSwapchain,

        /** @brief A bounded render wait timed out; the caller should keep pumping events. */
        RenderStalled,

        /** @brief Vulkan reported device loss. */
        DeviceLost,

        /** @brief Vulkan reported another unrecoverable render failure. */
        FatalError
    };

    /**
     * @brief RGBA color used by clear and simple 2D draw commands.
     */
    struct Color
    {
        /** @brief Red channel in linear floating-point color space. */
        float red = 1.0F;

        /** @brief Green channel in linear floating-point color space. */
        float green = 1.0F;

        /** @brief Blue channel in linear floating-point color space. */
        float blue = 1.0F;

        /** @brief Alpha channel in linear floating-point color space. */
        float alpha = 1.0F;
    };

    /**
     * @brief Integer rectangle in swapchain pixel coordinates.
     */
    struct Rect2D
    {
        /** @brief Left coordinate in pixels. */
        int x = 0;

        /** @brief Top coordinate in pixels. */
        int y = 0;

        /** @brief Rectangle width in pixels. */
        int width = 0;

        /** @brief Rectangle height in pixels. */
        int height = 0;
    };

    /**
     * @brief One minimal 2D draw command supported by the renderer.
     */
    struct RenderCommand
    {
        /** @brief Pixel rectangle to fill with color. */
        Rect2D bounds{};

        /** @brief Fill color applied to the rectangle. */
        Color color{};
    };

    /**
     * @brief Queue family choice produced by pure queue selection policy.
     */
    struct QueueFamilySelection
    {
        /** @brief Queue family index that supports graphics commands, if available. */
        std::optional<std::uint32_t> graphicsFamily;

        /** @brief Queue family index that supports presentation, if available. */
        std::optional<std::uint32_t> presentFamily;

        /**
         * @brief Reports whether both required queue families were selected.
         * @return True when graphics and present families are both available.
         */
        [[nodiscard]] bool isComplete() const noexcept;
    };

    /**
     * @brief Selects the preferred surface format from the supported list.
     * @param formats Surface formats reported by Vulkan.
     * @return Preferred SRGB BGRA format when present, otherwise the first supported format.
     * @throws std::runtime_error when no formats are supplied.
     */
    [[nodiscard]] VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);

    /**
     * @brief Selects the preferred present mode from the supported list.
     * @param presentModes Present modes reported by Vulkan.
     * @param preferVsync True to prefer FIFO; false to prefer MAILBOX when available.
     * @return Selected present mode.
     * @throws std::runtime_error when no modes are supplied or FIFO is unavailable while vsync is preferred.
     */
    [[nodiscard]] VkPresentModeKHR choosePresentMode(
        const std::vector<VkPresentModeKHR>& presentModes,
        bool preferVsync);

    /**
     * @brief Chooses the swapchain extent using surface capabilities and current drawable size.
     * @param capabilities Vulkan surface capabilities.
     * @param drawableSize Current platform drawable size in pixels.
     * @return Fixed current extent or clamped drawable size for variable-extent surfaces.
     */
    [[nodiscard]] VkExtent2D chooseSwapchainExtent(
        const VkSurfaceCapabilitiesKHR& capabilities,
        std::pair<int, int> drawableSize) noexcept;

    /**
     * @brief Selects graphics and present queue families from precomputed support flags.
     * @param queueFlags Vulkan queue family flags by index.
     * @param presentSupport Presentation support by matching index.
     * @return Queue family selection using the first complete pair discovered.
     */
    [[nodiscard]] QueueFamilySelection chooseQueueFamilies(
        const std::vector<VkQueueFlags>& queueFlags,
        const std::vector<VkBool32>& presentSupport) noexcept;

    /**
     * @brief Clips a rectangle to a framebuffer extent.
     * @param rect Input rectangle in framebuffer pixel coordinates.
     * @param extent Target framebuffer extent.
     * @return Vulkan rectangle when any visible pixels remain, otherwise empty.
     */
    [[nodiscard]] std::optional<VkRect2D> clipRectToExtent(const Rect2D& rect, VkExtent2D extent) noexcept;

    /**
     * @brief Classifies a bounded Vulkan fence wait result for runtime liveness handling.
     * @param result Vulkan result returned by vkWaitForFences.
     * @return Renderer wait status used by Core and Renderer policy.
     */
    [[nodiscard]] RendererWaitStatus classifyFenceWaitResult(VkResult result) noexcept;

    /**
     * @brief Classifies a Vulkan acquire result into a high-level frame status.
     * @param result Vulkan result returned by vkAcquireNextImageKHR.
     * @return Renderer frame status used by renderFrame.
     */
    [[nodiscard]] RendererFrameStatus classifyAcquireResult(VkResult result) noexcept;

    /**
     * @brief Classifies a Vulkan present result into a high-level frame status.
     * @param result Vulkan result returned by vkQueuePresentKHR.
     * @param wasFramebufferResized True when a resize was observed before present classification.
     * @return Renderer frame status used by renderFrame.
     */
    [[nodiscard]] RendererFrameStatus classifyPresentResult(VkResult result, bool wasFramebufferResized) noexcept;
}
