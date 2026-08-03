#pragma once

#include <Volk/volk.h>
#include <vk_mem_alloc.h>

namespace kt::rdr {

  template <size_t N> struct DescriptorPoolSet {
    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;
    std::array<VkDescriptorSet, N> sets;
  };

} // namespace kt::rdr
