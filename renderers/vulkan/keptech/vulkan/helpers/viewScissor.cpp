#include "viewScissor.hpp"

#include "wrappers/image.hpp"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace kt::vkh {

  void setFullscreenViewportAndScissor(VkCommandBuffer cmdBuf, const Image& image) {
    setViewportAndScissor(cmdBuf, {image.extent().x, image.extent().y}, {});
  }

  void setViewportAndScissor(VkCommandBuffer cmdBuf, const glm::uvec2& extent, const glm::ivec2& offset) {
    VkRect2D scissor{
        .offset = VkOffset2D{.x = static_cast<int32_t>(offset.x), .y = static_cast<int32_t>(offset.y)},
        .extent =
            VkExtent2D{
                .width = extent.x,
                .height = extent.y,
            },
    };
    VkViewport viewport{
        .x = static_cast<float>(offset.x),
        .y = static_cast<float>(offset.y),
        .width = static_cast<float>(extent.x),
        .height = static_cast<float>(extent.y),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
  }
} // namespace kt::vkh