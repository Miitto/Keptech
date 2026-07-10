#pragma once

#include "keptech/vulkan/wrappers/fwd.hpp"
#include <Volk/volk.h>
#include <glm/fwd.hpp>

namespace kt::vkh {
  void setFullscreenViewportAndScissor(VkCommandBuffer cmdBuf, const Image& image);
  void setViewportAndScissor(VkCommandBuffer cmdBuf, const glm::uvec2& extent, const glm::ivec2& offset);
} // namespace kt::vkh