#pragma once

#include "keptech/maths/sizes.hpp"
#include "keptech/rhi/imageLayout.hpp"
#include "keptech/rhi/imageRef.hpp"
#include "keptech/rhi/loadStoreOps.hpp"
#include <Volk/volk.h>
#include <span>

namespace kt::rhi {
  struct Pipeline;
  class BufferRef;
  class DescriptorSet;

  class CommandBuffer {
#include "keptech/rhi/interface/cmdBuf.inl"

  public:
    [[nodiscard]] operator VkCommandBuffer() const;
    [[nodiscard]] VkCommandBuffer get() const;
    [[nodiscard]] VkCommandBuffer operator*() const;

    const CommandBuffer& label(const VkDevice device, const std::string& name) const;

    const CommandBuffer& label(const VkDevice device, const char* name) const;

  private:
    VkCommandBuffer cmdBuf;
  };
} // namespace kt::rhi