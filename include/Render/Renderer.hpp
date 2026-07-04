// RaythmDemo - Vulkan Renderer Skeleton
// Defines the minimal engine-facing Vulkan renderer that clears and presents an SDL-backed window.
// Author: RatherHard
// Date: 2026-07-04

#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include <volk.h>

#include "Platform/Window.hpp"

namespace Raythm::Render
{
    /**
     * @brief Clear color used by the minimal renderer when clearing the swapchain image.
     */
    struct ClearColor
    {
        /** @brief Red channel in linear floating-point color space. */
        float red = 0.02F;

        /** @brief Green channel in linear floating-point color space. */
        float green = 0.03F;

        /** @brief Blue channel in linear floating-point color space. */
        float blue = 0.05F;

        /** @brief Alpha channel in linear floating-point color space. */
        float alpha = 1.0F;
    };

    /**
     * @brief Configuration for the minimal Vulkan renderer.
     */
    struct RendererOptions
    {
        /** @brief Color used for every frame until the game renderer is implemented. */
        ClearColor clearColor{};

        /** @brief Enables FIFO present mode preference so presentation remains widely supported. */
        bool preferVsync = true;
    };

    /**
     * @brief Move-only RAII owner for Vulkan instance, device, swapchain, and frame resources.
     */
    class Renderer
    {
    public:
        /** @brief Maximum number of CPU frames that may be queued ahead of the GPU. */
        static constexpr std::size_t MAX_FRAMES_IN_FLIGHT = 2;

        /**
         * @brief Creates a renderer for an SDL-backed Vulkan-capable window.
         * @param window Platform window used to create the Vulkan surface and determine drawable size.
         * @param options Renderer configuration used for the clear-and-present path.
         * @note volkInitialize is called before any Vulkan command and volkLoadInstance follows instance creation.
         */
        explicit Renderer(Platform::Window& window, const RendererOptions& options = {});

        /** @brief Destroys all Vulkan resources in dependency order. */
        ~Renderer();

        /** @brief Copying is disabled because the renderer uniquely owns Vulkan handles. */
        Renderer(const Renderer&) = delete;

        /** @brief Copy assignment is disabled because the renderer uniquely owns Vulkan handles. */
        Renderer& operator=(const Renderer&) = delete;

        /**
         * @brief Transfers Vulkan resource ownership from another renderer.
         * @param other Source renderer that becomes empty after the move.
         * @note Move construction is noexcept so renderer ownership can be transferred during bootstrap.
         */
        Renderer(Renderer&& other) noexcept;

        /**
         * @brief Replaces this renderer by taking Vulkan resource ownership from another renderer.
         * @param other Source renderer that becomes empty after the move.
         * @return Reference to this renderer after ownership transfer.
         * @note Any currently owned resources are destroyed before the transfer.
         */
        Renderer& operator=(Renderer&& other) noexcept;

        /**
         * @brief Handles platform window events that affect render lifecycle.
         * @param event Translated window event from the Platform module.
         * @note Resize-like events mark the swapchain for recreation on the next renderFrame call.
         */
        void handleWindowEvent(const Platform::WindowEvent& event) noexcept;

        /**
         * @brief Clears the current swapchain image and presents it.
         * @return True when a frame was submitted; false when rendering is paused by a zero drawable size.
         * @note Swapchain out-of-date and suboptimal results are handled by recreating swapchain resources.
         */
        bool renderFrame();

        /** @brief Blocks until the logical device has completed all queued work. */
        void waitIdle() const noexcept;

        /** @brief Reports whether initial Vulkan setup completed successfully. */
        bool isInitialized() const noexcept;

        /** @brief Reports whether rendering is paused due to a zero-sized drawable surface. */
        bool isRenderingPaused() const noexcept;

    private:
        /** @brief Queue family indices selected for graphics and presentation work. */
        struct QueueFamilyIndices
        {
            /** @brief Queue family index that supports graphics commands. */
            std::uint32_t graphicsFamily = UINT32_MAX;

            /** @brief Queue family index that supports presentation to the renderer surface. */
            std::uint32_t presentFamily = UINT32_MAX;

            /** @brief Reports whether both required queue families were selected. */
            bool isComplete() const noexcept;
        };

        /** @brief Surface capabilities and choices required to create a swapchain. */
        struct SwapchainSupportDetails
        {
            /** @brief Capabilities reported for the selected physical device and surface. */
            VkSurfaceCapabilitiesKHR capabilities{};

            /** @brief Surface formats supported by the selected physical device and surface. */
            std::vector<VkSurfaceFormatKHR> formats;

            /** @brief Present modes supported by the selected physical device and surface. */
            std::vector<VkPresentModeKHR> presentModes;
        };

        /** @brief Creates the Vulkan instance from SDL-required instance extensions. */
        void createInstance();

        /** @brief Creates the SDL-backed Vulkan presentation surface. */
        void createSurface();

        /** @brief Selects a physical device that can present to the SDL surface. */
        void pickPhysicalDevice();

        /** @brief Creates the logical device and retrieves queue handles. */
        void createLogicalDevice();

        /** @brief Creates swapchain images and image views for the current drawable size. */
        void createSwapchain();

        /** @brief Creates the command pool used for transient clear commands. */
        void createCommandPool();

        /** @brief Allocates command buffers used to clear swapchain images. */
        void createCommandBuffers();

        /** @brief Creates semaphores and fences for frame submission. */
        void createSyncObjects();

        /** @brief Destroys all Vulkan resources in dependency order. */
        void destroyResources() noexcept;

        /** @brief Destroys swapchain-dependent resources while preserving device-level resources. */
        void cleanupSwapchain() noexcept;

        /** @brief Recreates swapchain-dependent resources after resize or surface invalidation. */
        bool recreateSwapchain();

        /** @brief Records commands that transition, clear, and prepare one swapchain image for presentation. */
        void recordClearCommandBuffer(VkCommandBuffer commandBuffer, VkImage image) const;

        /** @brief Finds graphics and presentation queue family indices for a physical device. */
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice) const;

        /** @brief Queries swapchain capabilities for a physical device against the renderer surface. */
        SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice physicalDevice) const;

        /** @brief Checks whether the required swapchain device extension is available. */
        static bool supportsRequiredDeviceExtensions(VkPhysicalDevice physicalDevice);

        /** @brief Selects the preferred surface format from the supported list. */
        static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);

        /** @brief Selects the preferred present mode from the supported list. */
        static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes, bool preferVsync);

        /** @brief Chooses the swapchain extent using surface capabilities and current drawable size. */
        static VkExtent2D chooseSwapchainExtent(
            const VkSurfaceCapabilitiesKHR& capabilities,
            std::pair<int, int> drawableSize) noexcept;

        /** @brief Platform window used as the surface owner and drawable-size source. */
        Platform::Window* m_window = nullptr;

        /** @brief Renderer configuration copied at construction. */
        RendererOptions m_options{};

        /** @brief Vulkan instance owning global Vulkan dispatch state. */
        VkInstance m_instance = VK_NULL_HANDLE;

        /** @brief SDL-created Vulkan surface tied to the platform window. */
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;

        /** @brief Physical device chosen for graphics and present support. */
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

        /** @brief Logical device owning queues, swapchain, command buffers, and sync resources. */
        VkDevice m_device = VK_NULL_HANDLE;

        /** @brief Graphics queue family selected during device creation. */
        std::uint32_t m_graphicsFamily = UINT32_MAX;

        /** @brief Present queue family selected during device creation. */
        std::uint32_t m_presentFamily = UINT32_MAX;

        /** @brief Queue used to submit clear command buffers. */
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;

        /** @brief Queue used to present swapchain images to the surface. */
        VkQueue m_presentQueue = VK_NULL_HANDLE;

        /** @brief Swapchain whose images are cleared and presented each frame. */
        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

        /** @brief Format of swapchain images. */
        VkFormat m_swapchainImageFormat = VK_FORMAT_UNDEFINED;

        /** @brief Extent of swapchain images in physical pixels. */
        VkExtent2D m_swapchainExtent{};

        /** @brief Images owned by the swapchain. */
        std::vector<VkImage> m_swapchainImages;

        /** @brief Image views created for future render-pass compatibility and destroyed with the swapchain. */
        std::vector<VkImageView> m_swapchainImageViews;

        /** @brief Command pool used to allocate primary clear command buffers. */
        VkCommandPool m_commandPool = VK_NULL_HANDLE;

        /** @brief Command buffers used to clear acquired swapchain images. */
        std::vector<VkCommandBuffer> m_commandBuffers;

        /** @brief Semaphores signaled when swapchain images become available. */
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores{};

        /** @brief Semaphores signaled when rendering work has finished. */
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_renderFinishedSemaphores{};

        /** @brief Fences used to keep CPU frame submission bounded. */
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_inFlightFences{};

        /** @brief Current frame-in-flight slot. */
        std::size_t m_currentFrame = 0;

        /** @brief True when swapchain recreation is required before the next present. */
        bool m_framebufferResized = false;

        /** @brief True when rendering is paused because drawable size is zero. */
        bool m_renderingPaused = false;

        /** @brief True after initial Vulkan setup completes. */
        bool m_initialized = false;
    };
}
