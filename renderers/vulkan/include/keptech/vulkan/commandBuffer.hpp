#pragma once

#include "keptech/vulkan/material.hpp"
#include <keptech/core/rendering/commandBuffer.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {
  class CommandBuffer final : public keptech::ICommandBuffer {
  public:
    CommandBuffer(vk::raii::CommandBuffer&& commandBuffer, CmdBufType type)
        : ICommandBuffer(type), cmd(std::move(commandBuffer)) {}

    void begin() final {
      cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    }

    void copyBufferToBuffer(IBuffer& src, IBuffer& dst, uint64_t size,
                            uint64_t srcOffset = 0,
                            uint64_t dstOffset = 0) final;

    void beginRendering(const CommandBufferBeginRenderingInfo& info) final;

    void setViewport(glm::vec2 offset, glm::vec2 extent) final;
    void setScissor(glm::ivec2 offset, glm::uvec2 extent) final;

    void bindPipeline(const IPipeline& pipeline) final {
      const LoadedPipeline& vkPipeline =
          static_cast<const LoadedPipeline&>(pipeline);

      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *vkPipeline.pipeline);
    }

    void writePushConstants(const IPipeline& pipeline,
                            Bitflag<shaders::ShaderStages> stages,
                            uint32_t offset, uint32_t size,
                            const void* data) final;

    void bindIndexBuffer(IBuffer& buffer, uint64_t offset) final;
    void bindVertexBuffer(uint32_t firstBinding, std::vector<IBuffer*> buffers,
                          std::vector<uint64_t> offsets) final;

    void draw(uint32_t vertexCount, uint32_t instanceCount = 1,
              uint32_t firstVertex = 0, uint32_t firstInstance = 0) final;
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                     uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                     uint32_t firstInstance = 0) final;

    void endRendering() final { cmd.endRendering(); }
    void end() final { cmd.end(); }

    vk::raii::CommandBuffer& get() { return cmd; }

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&) = default;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer& operator=(CommandBuffer&&) = default;
    ~CommandBuffer() final = default;

  private:
    vk::raii::CommandBuffer cmd;
  };
} // namespace keptech::vkh
