#pragma once

#include <Volk/volk.h>

namespace kt::rhi {
  template <uint8_t MAX_FRAMES_IN_FLIGHT> struct DescriptorPoolSet {
    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> sets;
  };
} // namespace kt::rhi