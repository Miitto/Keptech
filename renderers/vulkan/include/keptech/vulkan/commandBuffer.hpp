#pragma once

#include <keptech/core/rendering/commandBuffer.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {
  class CommandBuffer final : public keptech::ICommandBuffer {
  public:
    CommandBuffer(vk::raii::CommandBuffer&& commandBuffer)
        : cmd(std::move(commandBuffer)) {}

    void begin() final {
      cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    }
    void beginRendering(const CommandBufferBeginRenderingInfo& info) final;

    void setViewport(glm::vec2 offset, glm::vec2 extent) final;
    void setScissor(glm::ivec2 offset, glm::uvec2 extent) final;

    void endRendering() final { cmd.endRendering(); }
    void end() final { cmd.end(); }

    vk::raii::CommandBuffer& get() { return cmd; }

  private:
    vk::raii::CommandBuffer cmd;
  };
} // namespace keptech::vkh
