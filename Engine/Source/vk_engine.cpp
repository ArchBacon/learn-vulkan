//> includes
#include "vk_engine.h"

#include <chrono>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <thread>
#include <vkbootstrap/VkBootstrap.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>


#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_types.h"

VulkanEngine* loadedEngine = nullptr;
constexpr bool bUseValidationLayers = true;

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

    InitVulkan();
    InitSwapchain();
    InitCommands();
    InitSyncStructures();

    // everything went fine
    bIsInitialized = true;
}

void VulkanEngine::Cleanup()
{
    if (bIsInitialized)
    {
        // Make sure the GPU has stopped doing its things
        vkDeviceWaitIdle(device);
        mainDeletionQueue.Flush();
        
        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
            auto& frame = frames[i];
            vkDestroyCommandPool(device, frame.commandPool, nullptr);

            vkDestroyFence(device, frame.renderFence, nullptr);
            vkDestroySemaphore(device, frame.renderSemaphore, nullptr);
            vkDestroySemaphore(device, frame.swapchainSemaphore, nullptr);
        }
        
        DestroySwapchain();
        
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyDevice(device, nullptr);
        
        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::Draw()
{
    // Wait until the GPU has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(device, 1, &GetCurrentFrame().renderFence, true, 1000000000));
    GetCurrentFrame().deletionQueue.Flush();
    VK_CHECK(vkResetFences(device, 1, &GetCurrentFrame().renderFence));

    // Request image from the swapchain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex));
    
    VkCommandBuffer command = GetCurrentFrame().commandBuffer;

    // Now that we are sure that the commands finished executing,
    // we can safely reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(command, 0));

    // Begin the command buffer recording. We will use this command buffer exactly once,
    // so we want to let vulkan know that.
    VkCommandBufferBeginInfo commandBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // Start the command buffer recording
    VK_CHECK(vkBeginCommandBuffer(command, &commandBeginInfo));

    // Make the swapchain image into a writable mode before rendering
    vkutil::transition_image(command, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // Make a clear-color from the frame number. This will flash with a 120 frame period.
    float flash = abs(sin(static_cast<float>(frameNumber) / 120.f));
    VkClearColorValue clearValue = {{0.0f, 0.0f, flash, 1.0f}};
    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    // Clear image
    vkCmdClearColorImage(command, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    // Make the swapchain image into presentable mode
    vkutil::transition_image(command, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(command));

    // Prepare the submission to the queue
    // We want to wait on the swapchainSemaphore, as that semaphore is signaled when the swapchain is ready
    // We will signal the renderSemaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo commandInfo = vkinit::command_buffer_submit_info(command);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&commandInfo, &signalInfo, &waitInfo);

    // Submit command buffer to the queue and execute it.
    // renderFence will now block until the graphics commands finish execution
    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, GetCurrentFrame().renderFence));

    // Prepare present
    // This will put the image we just rendered to into the visible window.
    // We want to wait on the renderSemaphore for that, as its necessary
    // that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    
    presentInfo.pImageIndices = &swapchainImageIndex;
    VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));

    // Increase the number of frames drawn
    frameNumber++;
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
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) { bStopRendering = true; }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) { bStopRendering = false; }
            }

            if (e.type == SDL_KEYDOWN)
            {
                fmt::print("Key pressed: {}.\n", SDL_GetKeyName(e.key.keysym.sym));
            }
        }

        // do not draw if we are minimized
        if (bStopRendering)
        {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Draw();
    }
}

void VulkanEngine::InitVulkan()
{
    vkb::InstanceBuilder builder;

    // Make the vulkan instance, with basic debug features
    auto result = builder
        .set_app_name("Example Vulkan Application")
        .request_validation_layers(bUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance vkbInstance = result.value();

    // Grab the instance
    instance = vkbInstance.instance;
    debugMessenger = vkbInstance.debug_messenger;

    SDL_Vulkan_CreateSurface(window, instance, &surface);

    // Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    // Use vkbootstrap to select a GPU
    // We want a GPU that can write tot he SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{vkbInstance};
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(surface)
        .select()
        .value();

    // Create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{physicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    device = vkbDevice.device;
    chosenGPU = physicalDevice.physical_device;

    // Use vkbootstrap to get a Graphics queue
    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // Initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = chosenGPU;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator);

    mainDeletionQueue.PushFunction([&]() -> void
    {
       vmaDestroyAllocator(allocator); 
    });
}

void VulkanEngine::InitSwapchain()
{
    CreateSwapchain(windowExtent.width, windowExtent.height);
}

void VulkanEngine::InitCommands()
{
    // Create a command pool for commands submitted to the graphics queue
    // We also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].commandPool));

        // Allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(frames[i].commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].commandBuffer));
    }
}

void VulkanEngine::InitSyncStructures()
{
    // Create synchronization structures
    // One fence to control when the GPU has finished rendering the frame,
    // and 2 semaphores to synchronize rendering with swapchain
    // We want the fence to start signalled so we can wait on it on the first time
    VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frames[i].renderFence));
        
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames[i].renderSemaphore));
    }
} 

void VulkanEngine::CreateSwapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{chosenGPU, device, surface};

    swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{swapchainImageFormat, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        // Use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    swapchainExtend = vkbSwapchain.extent;
    // Store swapchain and its related images
    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::DestroySwapchain()
{
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    // Destroy swapchain resources
    for (const auto& swapchainImageView : swapchainImageViews)
    {
        vkDestroyImageView(device, swapchainImageView, nullptr);
    }
}