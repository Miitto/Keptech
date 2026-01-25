#include "keptech/vulkan/commandBuffer.hpp"

#include "keptech/vulkan/texture.hpp"

namespace keptech::vkh {
  using vk::ClearDepthStencilValue;

  namespace {
    inline vk::AttachmentLoadOp from(AttachmentLoadOp loadOp) {
      return static_cast<vk::AttachmentLoadOp>(loadOp);
    }

    inline vk::AttachmentStoreOp from(AttachmentStoreOp storeOp) {
      return static_cast<vk::AttachmentStoreOp>(storeOp);
    }
  } // namespace

  void
  CommandBuffer::beginRendering(const CommandBufferBeginRenderingInfo& info) {
    std::vector<vk::RenderingAttachmentInfo> colorAttachments;
    colorAttachments.reserve(info.colorAttachments.size());
    for (auto& atch : info.colorAttachments) {
      vk::RenderingAttachmentInfo vkAtch{
          .imageView = static_cast<vk::ImageView>(
              dynamic_cast<vkh::Texture*>(atch.texture)->getImage().view),
          .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
          .loadOp = from(atch.loadOp),
          .storeOp = from(atch.storeOp),
          .clearValue = vk::ClearValue{vk::ClearColorValue{
              std::array<float, 4>{atch.clearColor.r, atch.clearColor.g,
                                   atch.clearColor.b, atch.clearColor.a}}},
      };
      colorAttachments.push_back(vkAtch);
    }

    vk::RenderingAttachmentInfo depthAttachmentInfo{
        .imageView = static_cast<vk::ImageView>(
            dynamic_cast<vkh::Texture*>(info.depthAttachment.texture)
                ->getImage()
                .view),
        .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp = from(info.depthAttachment.loadOp),
        .storeOp = from(info.depthAttachment.storeOp),
        .clearValue =
            vk::ClearValue{
                .depthStencil = {.depth = info.depthAttachment.clearDepth,
                                 .stencil = info.depthAttachment.clearStencil}},
    };

    vk::RenderingInfo renderingInfo{
        .renderArea =
            {
                .offset = {.x = info.renderAreaOffset.x,
                           .y = info.renderAreaOffset.y},
                .extent = {.width = info.renderAreaExtent.x,
                           .height = info.renderAreaExtent.y},
            },
        .layerCount = 1,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
        .pColorAttachments = colorAttachments.data(),
        .pDepthAttachment = &depthAttachmentInfo,
    };

    cmd.beginRendering(renderingInfo);
  }

  void CommandBuffer::setViewport(glm::vec2 offset, glm::vec2 extent) {
    cmd.setViewport(0, vk::Viewport{
                           .x = offset.x,
                           .y = offset.y,
                           .width = extent.x,
                           .height = extent.y,
                           .minDepth = 0.0f,
                           .maxDepth = 1.0f,
                       });
  }
  void CommandBuffer::setScissor(glm::ivec2 offset, glm::uvec2 extent) {
    cmd.setScissor(0, vk::Rect2D{
                          .offset =
                              {
                                  .x = offset.x,
                                  .y = offset.y,
                              },
                          .extent =
                              {
                                  .width = extent.x,
                                  .height = extent.y,
                              },
                      });
  }

} // namespace keptech::vkh
