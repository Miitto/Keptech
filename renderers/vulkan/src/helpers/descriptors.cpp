#include "keptech/vulkan/helpers/descriptors.hpp"
#include "macros.hpp"
#include <vulkan/vulkan.h>

namespace kt::vkh {
  void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags,
                                           uint32_t descriptorCount, VkDescriptorBindingFlags bindingFlags, void* pNext) {
    bindings.push_back(VkDescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = descriptorType,
        .descriptorCount = descriptorCount,
        .stageFlags = stageFlags,
        .pImmutableSamplers = nullptr,
    });
    bFlags.emplace_back(bindingFlags);
  }

  std::expected<VkDescriptorSetLayout, std::string> DescriptorLayoutBuilder::build(const VkDevice& device, void* pNext) const {
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = pNext,
        .bindingCount = static_cast<uint32_t>(bFlags.size()),
        .pBindingFlags = bFlags.data(),
    };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &bindingFlagsInfo,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    VkDescriptorSetLayout descriptorSetLayout;
    VK_MAKE(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &descriptorSetLayout),
            "Failed to create descriptor set layout.");

    return std::move(descriptorSetLayout);
  }

  std::expected<void, std::string> GrowableDescriptorPool::init(const VkDevice& device, std::span<PoolRatios> ratios,
                                                                VkDescriptorPoolCreateFlags flags, uint32_t poolSize) {
    this->device = device;
    this->poolSize = poolSize;
    this->poolCreateFlags = flags;
    for (const auto& ratio : ratios) {
      size.push_back(ratio);
    }

    VKH_MAKE(initialPool, createPool(poolSize, size), "Failed to create initial descriptor pool.");
    pool = std::move(initialPool);
    poolSize = static_cast<uint32_t>(static_cast<float>(poolSize) * 1.5);

    return {};
  }

  std::expected<VkDescriptorSet, std::string> GrowableDescriptorPool::allocate(const VkDescriptorSetLayout& layout, void* pNext) {
    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = pNext,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet set;
    auto allocRes = vkAllocateDescriptorSets(device, &allocInfo, &set);
    if (allocRes == VkResult::VK_ERROR_OUT_OF_POOL_MEMORY || allocRes == VkResult::VK_ERROR_FRAGMENTED_POOL) {
      // Try to create a new pool and allocate again
      oldPools.emplace_back(pool);

      VKH_MAKE(newPool, createPool(poolSize, size), "Failed to create new descriptor pool.");

      pool = newPool;

      poolSize = static_cast<uint32_t>(static_cast<float>(poolSize) * 1.5);

      allocInfo.descriptorPool = pool;
      allocRes = vkAllocateDescriptorSets(device, &allocInfo, &set);
    }

    if (allocRes != VK_SUCCESS) {
      return std::unexpected("Failed to allocate descriptor set.");
    }

    return set;
  }

  std::expected<VkDescriptorPool, std::string> GrowableDescriptorPool::createPool(uint32_t setCount, std::span<PoolRatios> ratios) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const auto& ratio : ratios) {
      poolSizes.push_back(VkDescriptorPoolSize{
          .type = ratio.type,
          .descriptorCount = static_cast<uint32_t>(ratio.ratio * static_cast<float>(setCount)),
      });
    }

    VkDescriptorPoolCreateInfo poolCreateInfo{
        .flags = poolCreateFlags,
        .maxSets = setCount,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    VkDescriptorPool descriptorPool;
    VK_MAKE(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool), "Failed to create descriptor pool.");

    return descriptorPool;
  }
} // namespace kt::vkh
