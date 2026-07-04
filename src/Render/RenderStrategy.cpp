// RaythmDemo - Render Strategy Helpers Implementation
// Implements GPU-independent Render selection policies and 2D clipping helpers.
// Author: RatherHard
// Date: 2026-07-04

#include "Render/RenderStrategy.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace Raythm::Render
{
    namespace
    {
        /** @brief Sentinel used by Vulkan to indicate that the surface controls swapchain extent directly. */
        constexpr std::uint32_t VARIABLE_SURFACE_EXTENT = std::numeric_limits<std::uint32_t>::max();

        /**
         * @brief Checks whether a vector contains a value.
         * @param values Values to inspect.
         * @param value Target value.
         * @return True when the value is present.
         */
        bool contains(const std::vector<VkPresentModeKHR>& values, VkPresentModeKHR value) noexcept
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }
    }

    bool QueueFamilySelection::isComplete() const noexcept
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }

    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
    {
        if (formats.empty())
        {
            throw std::runtime_error("No Vulkan surface formats are available.");
        }

        auto preferredFormat = std::find_if(
            formats.begin(),
            formats.end(),
            [](const VkSurfaceFormatKHR& format)
            {
                return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                       format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            });

        if (preferredFormat != formats.end())
        {
            return *preferredFormat;
        }

        return formats.front();
    }

    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes, bool preferVsync)
    {
        if (presentModes.empty())
        {
            throw std::runtime_error("No Vulkan present modes are available.");
        }

        if (preferVsync)
        {
            if (contains(presentModes, VK_PRESENT_MODE_FIFO_KHR))
            {
                return VK_PRESENT_MODE_FIFO_KHR;
            }

            throw std::runtime_error("Vulkan FIFO present mode is unavailable.");
        }

        if (contains(presentModes, VK_PRESENT_MODE_MAILBOX_KHR))
        {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }

        if (contains(presentModes, VK_PRESENT_MODE_FIFO_KHR))
        {
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        return presentModes.front();
    }

    VkExtent2D chooseSwapchainExtent(
        const VkSurfaceCapabilitiesKHR& capabilities,
        std::pair<int, int> drawableSize) noexcept
    {
        if (capabilities.currentExtent.width != VARIABLE_SURFACE_EXTENT)
        {
            return capabilities.currentExtent;
        }

        auto [drawableWidth, drawableHeight] = drawableSize;
        const std::uint32_t width = static_cast<std::uint32_t>(std::max(drawableWidth, 0));
        const std::uint32_t height = static_cast<std::uint32_t>(std::max(drawableHeight, 0));

        return {
            std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

    QueueFamilySelection chooseQueueFamilies(
        const std::vector<VkQueueFlags>& queueFlags,
        const std::vector<VkBool32>& presentSupport) noexcept
    {
        QueueFamilySelection selection{};
        const std::size_t queueCount = std::min(queueFlags.size(), presentSupport.size());

        for (std::uint32_t index = 0; index < queueCount; ++index)
        {
            if (!selection.graphicsFamily.has_value() && (queueFlags[index] & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                selection.graphicsFamily = index;
            }

            if (!selection.presentFamily.has_value() && presentSupport[index] == VK_TRUE)
            {
                selection.presentFamily = index;
            }

            if (selection.isComplete())
            {
                break;
            }
        }

        return selection;
    }

    std::optional<VkRect2D> clipRectToExtent(const Rect2D& rect, VkExtent2D extent) noexcept
    {
        if (rect.width <= 0 || rect.height <= 0 || extent.width == 0 || extent.height == 0)
        {
            return std::nullopt;
        }

        const std::int64_t maxWidth = std::min<std::uint32_t>(
            extent.width,
            static_cast<std::uint32_t>(std::numeric_limits<int>::max()));
        const std::int64_t maxHeight = std::min<std::uint32_t>(
            extent.height,
            static_cast<std::uint32_t>(std::numeric_limits<int>::max()));
        const std::int64_t rectLeft = rect.x;
        const std::int64_t rectTop = rect.y;
        const std::int64_t rectRight = rectLeft + rect.width;
        const std::int64_t rectBottom = rectTop + rect.height;

        const std::int64_t left = std::clamp<std::int64_t>(rectLeft, 0, maxWidth);
        const std::int64_t top = std::clamp<std::int64_t>(rectTop, 0, maxHeight);
        const std::int64_t right = std::clamp<std::int64_t>(rectRight, 0, maxWidth);
        const std::int64_t bottom = std::clamp<std::int64_t>(rectBottom, 0, maxHeight);

        if (right <= left || bottom <= top)
        {
            return std::nullopt;
        }

        VkRect2D clipped{};
        clipped.offset = {static_cast<int>(left), static_cast<int>(top)};
        clipped.extent = {
            static_cast<std::uint32_t>(right - left),
            static_cast<std::uint32_t>(bottom - top)
        };
        return clipped;
    }
}
