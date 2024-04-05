// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vkbootstrap/VkBootstrap.h>
#include "vk_initializers.h"
#include "vk_types.h"

class VulkanEngine
{
public:
    bool isInitialized{false};
    int mFrameNumber{0};
    bool mStopRendering{false};
    VkExtent2D windowExtent{1700,900};

    struct SDL_Window* window{nullptr};

    static VulkanEngine& Get();

    //initializes everything in the engine
    void Init();

    //shuts down the engine
    void Cleanup();

    //draw loop
    void Draw();

    //run main loop
    void Run();

    VkInstance instance; // Vulkan library handle
    VkDebugUtilsMessengerEXT debugMessenger; // Vulkan debug output
    VkPhysicalDevice chosenGPU; // GPU chosen as the default device
    VkDevice device; // Vulkan device for commands
    VkSurfaceKHR surface; // Vulkan window surface

    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;

    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtend;

private:
    void InitVulkan();
    void InitSwapchain();
    void InitCommands();
    void InitSyncStructures();

    void CreateSwapchain(uint32_t width, uint32_t height);
    void DestroySwapchain();
};