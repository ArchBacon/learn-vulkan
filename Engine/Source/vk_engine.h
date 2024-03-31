// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

class VulkanEngine
{
public:
    bool mIsInitialized{false};
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
};
