#pragma once

#include "keptech/vulkan/texture.hpp"
#include <keptech/core/rendering/commandBuffer.hpp>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {
  class CommandBuffer final : public keptech::ICommandBuffer {
  public:
    CommandBuffer(vk::raii::CommandBuffer&& commandBuffer)
        : cmd(std::move(commandBuffer)) {}

    void begin() final {
      cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    }
    void beginRenderering(const CommandBufferBeginRenderingInfo& info) final;
    void endRendering();
    void end();

    vk::raii::CommandBuffer& get() { return cmd; }

  private:
    vk::raii::CommandBuffer cmd;
  };
} // namespace keptech::vkh
