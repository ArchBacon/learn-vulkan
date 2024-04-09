// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vkbootstrap/VkBootstrap.h>
#include "vk_initializers.h"
#include "vk_types.h"

struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void PushFunction(std::function<void()>&& function)
    {
        deletors.push_back(function);
    }

    void Flush()
    {
        // Reverse iterate the deletion to execute all the function
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); //call functors
        }

        deletors.clear();
    }
};

struct FrameData
{
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore swapchainSemaphore;
    VkSemaphore renderSemaphore;
    VkFence renderFence;
    DeletionQueue deletionQueue;
};

constexpr uint8_t FRAME_OVERLAP = 2;

class VulkanEngine
{
public:
    bool bIsInitialized{false};
    int frameNumber{0};
    bool bStopRendering{false};
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

    FrameData frames[FRAME_OVERLAP];
    FrameData& GetCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;
    DeletionQueue mainDeletionQueue;

private:
    void InitVulkan();
    void InitSwapchain();
    void InitCommands();
    void InitSyncStructures();

    void CreateSwapchain(uint32_t width, uint32_t height);
    void DestroySwapchain();
};