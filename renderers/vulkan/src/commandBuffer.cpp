#include "keptech/vulkan/commandBuffer.hpp"

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
  CommandBuffer::beginRenderering(const CommandBufferBeginRenderingInfo& info) {
    std::vector<vk::RenderingAttachmentInfo> colorAttachments;
    colorAttachments.reserve(info.colorAttachments.size());
    for (auto& atch : info.colorAttachments) {
      vk::RenderingAttachmentInfo vkAtch{
          .imageView = static_cast<vk::ImageView>(
              dynamic_cast<vkh::Texture*>(atch.texture)->image.view),
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
                ->image.view),
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

} // namespace keptech::vkh
