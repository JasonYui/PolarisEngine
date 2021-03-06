//#include "GLFW/glfw3.h"
#include "core_minimal_public.hpp"
#include "rhi/details/vulkan/vulkan_dynamic_rhi.hpp"
#include "rhi/details/vulkan/vulkan_platform.hpp"
#include "platform_application.hpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        PL_VERBOSE("Render", pCallbackData->pMessage)
            break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        PL_INFO("Render", pCallbackData->pMessage)
            break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        PL_WARN("Render", pCallbackData->pMessage)
            break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        PL_ERROR("Render", pCallbackData->pMessage)
            break;
    default:
        break;
    }

    return VK_FALSE;
}

namespace Engine
{
    void VulkanDynamicRHI::Init()
    {
        InitInstance();
        SetupDebugMessenger();

        if (!CreateSurface())
        {
            return;
        }
        
        if (!FindPhysicalDevice())
        {
            return;
        }

        if (!CreateLogicalDevice())
        {
            return;
        }

        if (!CheckDeviceSuitable(PhysicalDevice))
        {
            return;
        }

        CreateSwapChain();
        CreateImageViews();
        CreateGraphicsPipeline();
    }

    void VulkanDynamicRHI::Shutdown()
    {
        for (auto&& imageView : SwapChainImageViews)
        {
            vkDestroyImageView(Device, imageView, nullptr);
        }

        if (Device && SwapChain)
        {
            vkDestroySwapchainKHR(Device, SwapChain, nullptr);
        }

        if (Device)
        {
            vkDestroyDevice(Device, nullptr);
        }

        if (DebugMessenger && Instance)
        {
            DestroyDebugUtilsMessengerEXT(Instance, DebugMessenger, nullptr);
        }

        if (Surface && Instance)
        {
            vkDestroySurfaceKHR(Instance, Surface, nullptr);
        }

        if (Instance)
        {
            vkDestroyInstance(Instance, nullptr);
        }
    }

    void VulkanDynamicRHI::InitInstance()
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Polaris";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Polaris Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkArray<const ansi*> extensions = GetExtraExtensions();

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.enabledExtensionCount = extensions.Size();
        createInfo.ppEnabledExtensionNames = extensions.Data();
        createInfo.pNext = nullptr;
#if VULKAN_DEBUG_MODE
        FillUpValidationSetting(createInfo);
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#endif

        if (vkCreateInstance(&createInfo, nullptr, &Instance) != VK_SUCCESS) 
        {
            PL_FATAL("Render", _T("Failed to create vulkan instance"));
            return;
        }
    }

    VkArray<const ansi*> VulkanDynamicRHI::GetExtraExtensions()
    {
        uint32 glfwExtensionCount = 0;
        /*const ansi** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);*/

        VkArray<const ansi*> extensions;
        VulkanPlatformHelper::GetExtraExtensions(extensions);
#if VULKAN_DEBUG_MODE
        extensions.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        return extensions;
    }

    bool VulkanDynamicRHI::CheckValidationLayerSupport()
    {
        uint32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        VkArray<VkLayerProperties> availableLayers;
        availableLayers.Resize(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.Data());

        for (const char* layerName : ValidationLayers) 
        {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) 
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) 
            {
                return false;
            }
        }

        return true;
    }

    void VulkanDynamicRHI::FillUpValidationSetting(VkInstanceCreateInfo& createInfo)
    {
#if VULKAN_DEBUG_MODE
        if (CheckValidationLayerSupport())
        {
            createInfo.enabledLayerCount = ValidationLayers.Size();
            createInfo.ppEnabledLayerNames = ValidationLayers.Data();
        }
        else
        {
            PL_WARN("Render", _T("CheckValidationLayerSupport failed in vulkan debug mode"));
        }
#endif
    }

    void VulkanDynamicRHI::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
    {
#if VULKAN_DEBUG_MODE
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;
#endif
    }

    void VulkanDynamicRHI::SetupDebugMessenger()
    {
#if VULKAN_DEBUG_MODE
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        PopulateDebugMessengerCreateInfo(createInfo);
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr && func(Instance, &createInfo, nullptr, &DebugMessenger) == VK_SUCCESS)
        {
            return;
        }
        else
        {
            PL_ERROR("Render", _T("Failed to set up debug messenger!"));
            return;
        }
#endif
    }

    void VulkanDynamicRHI::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* allocator)
    {
#if VULKAN_DEBUG_MODE
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) 
        {
            func(instance, debugMessenger, allocator);
        }
#endif
    }

    bool VulkanDynamicRHI::FindPhysicalDevice()
    {
        uint32 deviceNum = 0;
        vkEnumeratePhysicalDevices(Instance, &deviceNum, nullptr);

        if (deviceNum <= 0)
        {
            PL_ERROR("Render", _T("Failed to find GPUs with Vulkan support"));
            return false;
        }

        VkArray<VkPhysicalDevice> devices;
        devices.Resize(deviceNum);
        vkEnumeratePhysicalDevices(Instance, &deviceNum, devices.Data());

        for (const auto& device : devices) 
        {
            if (CheckDeviceSuitable(device))
            {
                PhysicalDevice = device;
                return true;
            }
        }

        return false;
    }

    QueueFamilyIndices VulkanDynamicRHI::FindQueueFamilies(VkPhysicalDevice device)
    {
        uint32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        VkArray<VkQueueFamilyProperties> queueFamilies;
        queueFamilies.Resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.Data());

        QueueFamilyIndices indices;
        int i = 0;
        for (const auto& queueFamily : queueFamilies) 
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) 
            {
                indices.GraphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, Surface, &presentSupport);
            
            if (presentSupport)
            {
                indices.PresentFamily = i;
            }

            if (indices.IsValid())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    bool VulkanDynamicRHI::CreateLogicalDevice()
    {
        QueueFamilyIndices indices = FindQueueFamilies(PhysicalDevice);

        VkArray<VkDeviceQueueCreateInfo> queueCreationInfos;
        Set<uint32> queueFamilies = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

        float queuePriority = 1.0f;
        for (uint32 queueFamily : queueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreationInfos.Add(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreationInfos.Data();
        createInfo.queueCreateInfoCount = queueCreationInfos.Size();
        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = DeviceExtensions.Size();
        createInfo.ppEnabledExtensionNames = DeviceExtensions.Data();
        createInfo.enabledLayerCount = 0;

#if VULKAN_DEBUG_MODE
        createInfo.enabledLayerCount = ValidationLayers.Size();
        createInfo.ppEnabledLayerNames = ValidationLayers.Data();
#endif

        if (vkCreateDevice(PhysicalDevice, &createInfo, nullptr, &Device) != VK_SUCCESS) 
        {
            PL_ERROR("Render", _T("Failed to create logical device!"));
            return false;
        }

        vkGetDeviceQueue(Device, indices.GraphicsFamily.value(), 0, &GraphicsQueue);
        vkGetDeviceQueue(Device, indices.PresentFamily.value(), 0, &PresentQueue);

        return true;
    }

    bool VulkanDynamicRHI::CreateSurface()
    {
        return VulkanPlatformHelper::GetSurfaceKHR(Instance, Surface);
    }

    bool VulkanDynamicRHI::CheckDeviceSuitable(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices = FindQueueFamilies(device);

        bool swapChainAdequate = false;
        if (CheckDeviceExtensionSupport(device))
        {
            SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.Formats.IsEmpty() && !swapChainSupport.PresentModes.IsEmpty();
        }
        return swapChainAdequate && indices.IsValid();
    }

    bool VulkanDynamicRHI::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
    {
        uint32 extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        VkExtensionProperties* availableExtensions = new VkExtensionProperties[extensionCount];
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions);

        Set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());

        for (uint32 idx = 0; idx < extensionCount; ++idx)
        {
            if (requiredExtensions.Remove(availableExtensions[idx].extensionName))
            {
                break;
            }
        }

        return requiredExtensions.IsEmpty();
    }

    SwapChainSupportDetails VulkanDynamicRHI::QuerySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, Surface, &details.Capabilities);

        uint32 formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, Surface, &formatCount, nullptr);

        if (formatCount != 0)
        {
            details.Formats.Resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, Surface, &formatCount, details.Formats.Data());
        }

        uint32 presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, Surface, &presentModeCount, nullptr);

        if (presentModeCount != 0)
        {
            details.PresentModes.Resize(formatCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, Surface, &presentModeCount, details.PresentModes.Data());
        }

        return details;
    }

    VkSurfaceFormatKHR VulkanDynamicRHI::ChooseSwapSurfaceFormat(const VkArray<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR VulkanDynamicRHI::ChooseSwapPresentMode(const VkArray<VkPresentModeKHR>& availablePresentModes)
    {
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VulkanDynamicRHI::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != MAX_UINT32)
        {
            return capabilities.currentExtent;
        }
        else
        {
            int32 width, height;
            PlatformApplication::GetFrameBufferSize(width, height);

            VkExtent2D actualExtent =
            {
                static_cast<uint32>(width),
                static_cast<uint32>(height)
            };

            actualExtent.width = Math::Clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = Math::Clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    void VulkanDynamicRHI::CreateSwapChain()
    {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(PhysicalDevice);

        VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.Formats);
        VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.PresentModes);
        VkExtent2D extent = ChooseSwapExtent(swapChainSupport.Capabilities);

        uint32 imageCount = swapChainSupport.Capabilities.minImageCount + 1;
        if (swapChainSupport.Capabilities.maxImageCount > 0 && imageCount > swapChainSupport.Capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.Capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = Surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        QueueFamilyIndices indices = FindQueueFamilies(PhysicalDevice);
        uint32 queueFamilyIndices[] = {indices.GraphicsFamily.value(), indices.PresentFamily.value()};

        if (indices.GraphicsFamily != indices.PresentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }
        createInfo.preTransform = swapChainSupport.Capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(Device, &createInfo, nullptr, &SwapChain) != VK_SUCCESS)
        {
            PL_ERROR("Render", _T("Failed to create swap chain!"));
        }

        vkGetSwapchainImagesKHR(Device, SwapChain, &imageCount, nullptr);
        SwapChainImages.Resize(imageCount);
        vkGetSwapchainImagesKHR(Device, SwapChain, &imageCount, SwapChainImages.Data());

        SwapChainImageFormat = surfaceFormat.format;
        SwapChainExtent = extent;
    }

    void VulkanDynamicRHI::CreateImageViews()
    {
        SwapChainImageViews.Resize(SwapChainImages.Size());
        uint32 idx = 0;
        for (const auto& image : SwapChainImages)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = image;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = SwapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(Device, &createInfo, nullptr, &SwapChainImageViews[idx]) != VK_SUCCESS)
            {
                PL_ERROR("", _T("Failed to create image views!"));
            }
            ++idx;
        }
    }

    void VulkanDynamicRHI::CreateGraphicsPipeline()
    {

    }
}
