// RaythmDemo - Vulkan Renderer Skeleton Implementation
// Implements Vulkan clear-and-present rendering for an SDL-backed platform window.
// Author: RatherHard
// Date: 2026-07-04

#include "Render/Renderer.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Raythm::Render
{
    namespace
    {
        /** @brief Vulkan device extension required for presentation through swapchains. */
        constexpr const char* REQUIRED_SWAPCHAIN_EXTENSION = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

        /** @brief Application name passed into Vulkan instance creation. */
        constexpr const char* APPLICATION_NAME = "RaythmDemo";

        /** @brief Engine name passed into Vulkan instance creation. */
        constexpr const char* ENGINE_NAME = "RaythmDemo";

        /** @brief Sentinel used by Vulkan to indicate that the surface controls swapchain extent directly. */
        constexpr std::uint32_t VARIABLE_SURFACE_EXTENT = std::numeric_limits<std::uint32_t>::max();

        /**
         * @brief Converts a Vulkan result code into an exception with context.
         * @param message High-level failure context.
         * @param result Vulkan result returned by the failed call.
         * @return Runtime error containing the result code.
         */
        std::runtime_error makeVulkanError(const std::string& message, VkResult result)
        {
            return std::runtime_error(message + " VkResult=" + std::to_string(static_cast<int>(result)));
        }

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

    bool Renderer::QueueFamilyIndices::isComplete() const noexcept
    {
        return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
    }

    Renderer::Renderer(Platform::Window& window, const RendererOptions& options)
        : m_window(&window),
          m_options(options)
    {
        try
        {
            createInstance();
            createSurface();
            pickPhysicalDevice();
            createLogicalDevice();
            createSwapchain();
            createCommandPool();
            createCommandBuffers();
            createSyncObjects();
            m_initialized = true;
        }
        catch (...)
        {
            destroyResources();
            throw;
        }
    }

    Renderer::~Renderer()
    {
        destroyResources();
    }

    void Renderer::destroyResources() noexcept
    {
        waitIdle();
        cleanupSwapchain();

        for (VkFence& fence : m_inFlightFences)
        {
            if (fence != VK_NULL_HANDLE)
            {
                vkDestroyFence(m_device, fence, nullptr);
                fence = VK_NULL_HANDLE;
            }
        }

        for (VkSemaphore& semaphore : m_renderFinishedSemaphores)
        {
            if (semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_device, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
        }

        for (VkSemaphore& semaphore : m_imageAvailableSemaphores)
        {
            if (semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_device, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
        }

        if (m_commandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }

        if (m_device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_device, nullptr);
            m_device = VK_NULL_HANDLE;
        }

        if (m_surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }

        if (m_instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
        }

        m_physicalDevice = VK_NULL_HANDLE;
        m_graphicsQueue = VK_NULL_HANDLE;
        m_presentQueue = VK_NULL_HANDLE;
        m_initialized = false;
    }

    Renderer::Renderer(Renderer&& other) noexcept
        : m_window(std::exchange(other.m_window, nullptr)),
          m_options(other.m_options),
          m_instance(std::exchange(other.m_instance, VK_NULL_HANDLE)),
          m_surface(std::exchange(other.m_surface, VK_NULL_HANDLE)),
          m_physicalDevice(std::exchange(other.m_physicalDevice, VK_NULL_HANDLE)),
          m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
          m_graphicsFamily(std::exchange(other.m_graphicsFamily, UINT32_MAX)),
          m_presentFamily(std::exchange(other.m_presentFamily, UINT32_MAX)),
          m_graphicsQueue(std::exchange(other.m_graphicsQueue, VK_NULL_HANDLE)),
          m_presentQueue(std::exchange(other.m_presentQueue, VK_NULL_HANDLE)),
          m_swapchain(std::exchange(other.m_swapchain, VK_NULL_HANDLE)),
          m_swapchainImageFormat(std::exchange(other.m_swapchainImageFormat, VK_FORMAT_UNDEFINED)),
          m_swapchainExtent(std::exchange(other.m_swapchainExtent, VkExtent2D{})),
          m_swapchainImages(std::move(other.m_swapchainImages)),
          m_swapchainImageViews(std::move(other.m_swapchainImageViews)),
          m_commandPool(std::exchange(other.m_commandPool, VK_NULL_HANDLE)),
          m_commandBuffers(std::move(other.m_commandBuffers)),
          m_imageAvailableSemaphores(std::exchange(other.m_imageAvailableSemaphores, {})),
          m_renderFinishedSemaphores(std::exchange(other.m_renderFinishedSemaphores, {})),
          m_inFlightFences(std::exchange(other.m_inFlightFences, {})),
          m_currentFrame(std::exchange(other.m_currentFrame, 0)),
          m_framebufferResized(std::exchange(other.m_framebufferResized, false)),
          m_renderingPaused(std::exchange(other.m_renderingPaused, false)),
          m_initialized(std::exchange(other.m_initialized, false))
    {
    }

    Renderer& Renderer::operator=(Renderer&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        Renderer movedRenderer(std::move(other));
        std::swap(m_window, movedRenderer.m_window);
        std::swap(m_options, movedRenderer.m_options);
        std::swap(m_instance, movedRenderer.m_instance);
        std::swap(m_surface, movedRenderer.m_surface);
        std::swap(m_physicalDevice, movedRenderer.m_physicalDevice);
        std::swap(m_device, movedRenderer.m_device);
        std::swap(m_graphicsFamily, movedRenderer.m_graphicsFamily);
        std::swap(m_presentFamily, movedRenderer.m_presentFamily);
        std::swap(m_graphicsQueue, movedRenderer.m_graphicsQueue);
        std::swap(m_presentQueue, movedRenderer.m_presentQueue);
        std::swap(m_swapchain, movedRenderer.m_swapchain);
        std::swap(m_swapchainImageFormat, movedRenderer.m_swapchainImageFormat);
        std::swap(m_swapchainExtent, movedRenderer.m_swapchainExtent);
        m_swapchainImages.swap(movedRenderer.m_swapchainImages);
        m_swapchainImageViews.swap(movedRenderer.m_swapchainImageViews);
        std::swap(m_commandPool, movedRenderer.m_commandPool);
        m_commandBuffers.swap(movedRenderer.m_commandBuffers);
        std::swap(m_imageAvailableSemaphores, movedRenderer.m_imageAvailableSemaphores);
        std::swap(m_renderFinishedSemaphores, movedRenderer.m_renderFinishedSemaphores);
        std::swap(m_inFlightFences, movedRenderer.m_inFlightFences);
        std::swap(m_currentFrame, movedRenderer.m_currentFrame);
        std::swap(m_framebufferResized, movedRenderer.m_framebufferResized);
        std::swap(m_renderingPaused, movedRenderer.m_renderingPaused);
        std::swap(m_initialized, movedRenderer.m_initialized);
        return *this;
    }

    void Renderer::handleWindowEvent(const Platform::WindowEvent& event) noexcept
    {
        switch (event.type)
        {
        case Platform::WindowEventType::Resized:
        case Platform::WindowEventType::PixelSizeChanged:
        case Platform::WindowEventType::Restored:
            m_framebufferResized = true;
            break;
        case Platform::WindowEventType::Minimized:
            m_renderingPaused = true;
            break;
        default:
            break;
        }
    }

    bool Renderer::renderFrame()
    {
        if (m_window == nullptr || m_device == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Cannot render with an uninitialized renderer.");
        }

        const auto [drawableWidth, drawableHeight] = m_window->getDrawableSize();
        if (drawableWidth <= 0 || drawableHeight <= 0)
        {
            m_renderingPaused = true;
            return false;
        }

        if (m_renderingPaused || m_framebufferResized || m_swapchain == VK_NULL_HANDLE)
        {
            if (!recreateSwapchain())
            {
                return false;
            }
        }

        VkFence inFlightFence = m_inFlightFences[m_currentFrame];
        VkResult result = vkWaitForFences(m_device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to wait for frame fence.", result);
        }

        std::uint32_t imageIndex = 0;
        result = vkAcquireNextImageKHR(
            m_device,
            m_swapchain,
            UINT64_MAX,
            m_imageAvailableSemaphores[m_currentFrame],
            VK_NULL_HANDLE,
            &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            return recreateSwapchain();
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw makeVulkanError("Failed to acquire swapchain image.", result);
        }

        result = vkResetFences(m_device, 1, &inFlightFence);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to reset frame fence.", result);
        }

        VkCommandBuffer commandBuffer = m_commandBuffers[m_currentFrame];
        result = vkResetCommandBuffer(commandBuffer, 0);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to reset command buffer.", result);
        }

        recordClearCommandBuffer(commandBuffer, m_swapchainImages[imageIndex]);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];

        result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, inFlightFence);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to submit clear command buffer.", result);
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_swapchain;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized)
        {
            m_framebufferResized = false;
            recreateSwapchain();
        }
        else if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to present swapchain image.", result);
        }

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        return true;
    }

    void Renderer::waitIdle() const noexcept
    {
        if (m_device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(m_device);
        }
    }

    bool Renderer::isInitialized() const noexcept
    {
        return m_initialized;
    }

    bool Renderer::isRenderingPaused() const noexcept
    {
        return m_renderingPaused;
    }

    void Renderer::createInstance()
    {
        VkResult result = volkInitialize();
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to initialize volk.", result);
        }

        std::vector<const char*> extensions = m_window->getRequiredVulkanInstanceExtensions();
        if (extensions.empty())
        {
            throw std::runtime_error("SDL did not report required Vulkan instance extensions.");
        }

        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = APPLICATION_NAME;
        applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        applicationInfo.pEngineName = ENGINE_NAME;
        applicationInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        applicationInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &applicationInfo;
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        result = vkCreateInstance(&createInfo, nullptr, &m_instance);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to create Vulkan instance.", result);
        }

        volkLoadInstance(m_instance);
    }

    void Renderer::createSurface()
    {
        m_surface = m_window->createVulkanSurface(m_instance);
    }

    void Renderer::pickPhysicalDevice()
    {
        std::uint32_t deviceCount = 0;
        VkResult result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to enumerate Vulkan physical devices.", result);
        }

        if (deviceCount == 0)
        {
            throw std::runtime_error("No Vulkan physical devices are available.");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to read Vulkan physical devices.", result);
        }

        for (VkPhysicalDevice device : devices)
        {
            QueueFamilyIndices indices = findQueueFamilies(device);
            if (!indices.isComplete() || !supportsRequiredDeviceExtensions(device))
            {
                continue;
            }

            SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device);
            if (swapchainSupport.formats.empty() || swapchainSupport.presentModes.empty())
            {
                continue;
            }

            m_physicalDevice = device;
            m_graphicsFamily = indices.graphicsFamily;
            m_presentFamily = indices.presentFamily;
            return;
        }

        throw std::runtime_error("No suitable Vulkan physical device supports graphics, presentation, and swapchains.");
    }

    void Renderer::createLogicalDevice()
    {
        const std::set<std::uint32_t> uniqueQueueFamilies = {m_graphicsFamily, m_presentFamily};
        const float queuePriority = 1.0F;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.reserve(uniqueQueueFamilies.size());

        for (std::uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        const std::array<const char*, 1> deviceExtensions = {REQUIRED_SWAPCHAIN_EXTENSION};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to create Vulkan logical device.", result);
        }

        volkLoadDevice(m_device);
        vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, m_presentFamily, 0, &m_presentQueue);
    }

    void Renderer::createSwapchain()
    {
        SwapchainSupportDetails swapchainSupport = querySwapchainSupport(m_physicalDevice);
        VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(swapchainSupport.formats);
        VkPresentModeKHR presentMode = choosePresentMode(swapchainSupport.presentModes, m_options.preferVsync);
        VkExtent2D extent = chooseSwapchainExtent(swapchainSupport.capabilities, m_window->getDrawableSize());

        if (extent.width == 0 || extent.height == 0)
        {
            m_renderingPaused = true;
            return;
        }

        if ((swapchainSupport.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
        {
            throw std::runtime_error("Selected Vulkan surface does not support transfer-destination swapchain images.");
        }

        std::uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
        if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount)
        {
            imageCount = swapchainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const std::array<std::uint32_t, 2> queueFamilyIndices = {m_graphicsFamily, m_presentFamily};
        if (m_graphicsFamily != m_presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkResult result = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to create Vulkan swapchain.", result);
        }

        vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
        m_swapchainImages.resize(imageCount);
        result = vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to read Vulkan swapchain images.", result);
        }

        m_swapchainImageFormat = surfaceFormat.format;
        m_swapchainExtent = extent;
        m_swapchainImageViews.reserve(m_swapchainImages.size());

        for (VkImage image : m_swapchainImages)
        {
            VkImageViewCreateInfo imageViewInfo{};
            imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewInfo.image = image;
            imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageViewInfo.format = m_swapchainImageFormat;
            imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewInfo.subresourceRange.baseMipLevel = 0;
            imageViewInfo.subresourceRange.levelCount = 1;
            imageViewInfo.subresourceRange.baseArrayLayer = 0;
            imageViewInfo.subresourceRange.layerCount = 1;

            VkImageView imageView = VK_NULL_HANDLE;
            result = vkCreateImageView(m_device, &imageViewInfo, nullptr, &imageView);
            if (result != VK_SUCCESS)
            {
                throw makeVulkanError("Failed to create Vulkan swapchain image view.", result);
            }

            m_swapchainImageViews.push_back(imageView);
        }

        m_renderingPaused = false;
        m_framebufferResized = false;
    }

    void Renderer::createCommandPool()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_graphicsFamily;

        VkResult result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to create Vulkan command pool.", result);
        }
    }

    void Renderer::createCommandBuffers()
    {
        m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = m_commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = static_cast<std::uint32_t>(m_commandBuffers.size());

        VkResult result = vkAllocateCommandBuffers(m_device, &allocateInfo, m_commandBuffers.data());
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to allocate Vulkan command buffers.", result);
        }
    }

    void Renderer::createSyncObjects()
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (std::size_t index = 0; index < MAX_FRAMES_IN_FLIGHT; ++index)
        {
            VkResult result = vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[index]);
            if (result != VK_SUCCESS)
            {
                throw makeVulkanError("Failed to create image-available semaphore.", result);
            }

            result = vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[index]);
            if (result != VK_SUCCESS)
            {
                throw makeVulkanError("Failed to create render-finished semaphore.", result);
            }

            result = vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[index]);
            if (result != VK_SUCCESS)
            {
                throw makeVulkanError("Failed to create frame fence.", result);
            }
        }
    }

    void Renderer::cleanupSwapchain() noexcept
    {
        for (VkImageView imageView : m_swapchainImageViews)
        {
            if (imageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_device, imageView, nullptr);
            }
        }

        m_swapchainImageViews.clear();
        m_swapchainImages.clear();

        if (m_swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
        }

        m_swapchainImageFormat = VK_FORMAT_UNDEFINED;
        m_swapchainExtent = {};
    }

    bool Renderer::recreateSwapchain()
    {
        const auto [drawableWidth, drawableHeight] = m_window->getDrawableSize();
        if (drawableWidth <= 0 || drawableHeight <= 0)
        {
            m_renderingPaused = true;
            return false;
        }

        waitIdle();
        cleanupSwapchain();
        createSwapchain();
        return !m_renderingPaused;
    }

    void Renderer::recordClearCommandBuffer(VkCommandBuffer commandBuffer, VkImage image) const
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to begin Vulkan command buffer.", result);
        }

        VkImageMemoryBarrier transferBarrier{};
        transferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        transferBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        transferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        transferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transferBarrier.image = image;
        transferBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        transferBarrier.subresourceRange.baseMipLevel = 0;
        transferBarrier.subresourceRange.levelCount = 1;
        transferBarrier.subresourceRange.baseArrayLayer = 0;
        transferBarrier.subresourceRange.layerCount = 1;
        transferBarrier.srcAccessMask = 0;
        transferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &transferBarrier);

        VkClearColorValue clearColor = {
            {m_options.clearColor.red, m_options.clearColor.green, m_options.clearColor.blue, m_options.clearColor.alpha}
        };

        VkImageSubresourceRange clearRange{};
        clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clearRange.baseMipLevel = 0;
        clearRange.levelCount = 1;
        clearRange.baseArrayLayer = 0;
        clearRange.layerCount = 1;

        vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);

        VkImageMemoryBarrier presentBarrier{};
        presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentBarrier.image = image;
        presentBarrier.subresourceRange = clearRange;
        presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        presentBarrier.dstAccessMask = 0;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &presentBarrier);

        result = vkEndCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to end Vulkan command buffer.", result);
        }
    }

    Renderer::QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice physicalDevice) const
    {
        QueueFamilyIndices indices{};

        std::uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        for (std::uint32_t index = 0; index < queueFamilyCount; ++index)
        {
            if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                indices.graphicsFamily = index;
            }

            VkBool32 supportsPresent = VK_FALSE;
            VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, index, m_surface, &supportsPresent);
            if (result != VK_SUCCESS)
            {
                throw makeVulkanError("Failed to query Vulkan present support.", result);
            }

            if (supportsPresent == VK_TRUE)
            {
                indices.presentFamily = index;
            }

            if (indices.isComplete())
            {
                break;
            }
        }

        return indices;
    }

    Renderer::SwapchainSupportDetails Renderer::querySwapchainSupport(VkPhysicalDevice physicalDevice) const
    {
        SwapchainSupportDetails details{};

        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_surface, &details.capabilities);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to query Vulkan surface capabilities.", result);
        }

        std::uint32_t formatCount = 0;
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to query Vulkan surface format count.", result);
        }

        details.formats.resize(formatCount);
        if (formatCount > 0)
        {
            result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, details.formats.data());
            if (result != VK_SUCCESS)
            {
                throw makeVulkanError("Failed to query Vulkan surface formats.", result);
            }
        }

        std::uint32_t presentModeCount = 0;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to query Vulkan surface present mode count.", result);
        }

        details.presentModes.resize(presentModeCount);
        if (presentModeCount > 0)
        {
            result = vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice,
                m_surface,
                &presentModeCount,
                details.presentModes.data());
            if (result != VK_SUCCESS)
            {
                throw makeVulkanError("Failed to query Vulkan surface present modes.", result);
            }
        }

        return details;
    }

    bool Renderer::supportsRequiredDeviceExtensions(VkPhysicalDevice physicalDevice)
    {
        std::uint32_t extensionCount = 0;
        VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to query Vulkan device extension count.", result);
        }

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        result = vkEnumerateDeviceExtensionProperties(
            physicalDevice,
            nullptr,
            &extensionCount,
            availableExtensions.data());
        if (result != VK_SUCCESS)
        {
            throw makeVulkanError("Failed to query Vulkan device extensions.", result);
        }

        return std::any_of(
            availableExtensions.begin(),
            availableExtensions.end(),
            [](const VkExtensionProperties& extension)
            {
                return std::string(extension.extensionName) == REQUIRED_SWAPCHAIN_EXTENSION;
            });
    }

    VkSurfaceFormatKHR Renderer::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
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

    VkPresentModeKHR Renderer::choosePresentMode(
        const std::vector<VkPresentModeKHR>& presentModes,
        bool preferVsync)
    {
        if (presentModes.empty())
        {
            throw std::runtime_error("No Vulkan present modes are available.");
        }

        if (!preferVsync && contains(presentModes, VK_PRESENT_MODE_MAILBOX_KHR))
        {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Renderer::chooseSwapchainExtent(
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
}
