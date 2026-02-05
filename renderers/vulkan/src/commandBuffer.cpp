#include "keptech/vulkan/commandBuffer.hpp"

#include "keptech/vulkan/buffer.hpp"
#include "keptech/vulkan/texture.hpp"

#include "conversions.hpp"

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

  void CommandBuffer::copyBufferToBuffer(IBuffer& src, IBuffer& dst,
                                         uint64_t size, uint64_t srcOffset,
                                         uint64_t dstOffset) {
    vkh::Buffer& vkSrc = dynamic_cast<vkh::Buffer&>(src);
    vkh::Buffer& vkDst = dynamic_cast<vkh::Buffer&>(dst);

    vk::BufferCopy copyRegion{
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = size,
    };

    cmd.copyBuffer(vkSrc.getBuffer().buffer, vkDst.getBuffer().buffer,
                   copyRegion);
  }

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
        .imageView =
            info.depthAttachment.texture == nullptr
                ? nullptr
                : static_cast<vk::ImageView>(
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

  void CommandBuffer::writePushConstants(const IPipeline& pipeline,
                                         Bitflag<shaders::ShaderStages> stages,
                                         uint32_t offset, uint32_t size,
                                         const void* data) {
    const LoadedPipeline& vkPipeline =
        static_cast<const LoadedPipeline&>(pipeline);

    vk::PushConstantsInfo info{
        .layout = *vkPipeline.pipelineLayout,
        .stageFlags = from(stages),
        .offset = offset,
        .size = size,
        .pValues = data,
    };

    cmd.pushConstants2(info);
  }

  void CommandBuffer::bindIndexBuffer(IBuffer& buffer, uint64_t offset) {
    vkh::Buffer& vkBuffer = static_cast<vkh::Buffer&>(buffer);

    cmd.bindIndexBuffer(vkBuffer.getBuffer().buffer, offset,
                        vk::IndexType::eUint32);
  }

  void CommandBuffer::bindVertexBuffer(uint32_t firstBinding,
                                       std::vector<IBuffer*> buffers,
                                       std::vector<uint64_t> offsets) {
    std::vector<vk::Buffer> vkBuffers;
    vkBuffers.reserve(buffers.size());
    for (auto* buf : buffers) {
      vkh::Buffer& vkBuffer = static_cast<vkh::Buffer&>(*buf);
      vkBuffers.push_back(vkBuffer.getBuffer().buffer);
    }

    cmd.bindVertexBuffers(firstBinding, vkBuffers, offsets);
  }

  void CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount,
                           uint32_t firstVertex, uint32_t firstInstance) {
    cmd.draw(vertexCount, instanceCount, firstVertex, firstInstance);
  }

  void CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                  uint32_t firstIndex, int32_t vertexOffset,
                                  uint32_t firstInstance) {
    cmd.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset,
                    firstInstance);
  }

} // namespace keptech::vkh
