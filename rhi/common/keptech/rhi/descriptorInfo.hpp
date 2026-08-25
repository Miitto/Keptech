#pragma once

#include "keptech/rhi/descriptorTypes.hpp"

namespace kt::rhi {
  struct DescriptorPoolInfo {
    /// Maximum number of descriptor sets that can be allocated from the created pool
    /// @note Only applicable for Vulkan
    uint32_t maxSets;

    /// Maximum number of sampled images that can be allocated from the created pool.
    uint32_t maxSampledImages;
    /// Maximum number of storage images that can be allocated from the created pool.
    uint32_t maxStorageImages;
    /// Maximum number of uniform buffers that can be allocated from the created pool.
    uint32_t maxUniformBuffers;
    /// Maximum number of storage buffers that can be allocated from the created pool.
    uint32_t maxStorageBuffers;
  };

  struct DescriptorInfo {
    DescriptorType type;
    uint32_t binding;
    uint32_t count;
  };
} // namespace kt::rhi