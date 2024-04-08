#pragma once

#include <vulkan/vulkan.h>

namespace vkutil
{
    void transition_image(VkCommandBuffer command, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
};
