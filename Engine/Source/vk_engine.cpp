//> includes
#include "vk_engine.h"

#include <chrono>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <thread>
#include <vk_initializers.h>
#include <vk_types.h>

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

void VulkanEngine::Init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    const SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN;

    window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        static_cast<int>(windowExtent.width),
        static_cast<int>(windowExtent.height),
        windowFlags
    );

    // everything went fine
    mIsInitialized = true;
}

void VulkanEngine::Cleanup()
{
    if (mIsInitialized) { SDL_DestroyWindow(window); }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::Draw()
{
    // nothing yet
}

void VulkanEngine::Run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit)
    {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT) bQuit = true;

            if (e.type == SDL_WINDOWEVENT)
            {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) { mStopRendering = true; }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) { mStopRendering = false; }
            }
        }

        // do not draw if we are minimized
        if (mStopRendering)
        {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Draw();
    }
}
