#include "keptech/vulkan/helpers/descriptors.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"

namespace keptech::vkh {
  void DescriptorLayoutBuilder::addBinding(
      uint32_t binding, vk::DescriptorType descriptorType,
      vk::ShaderStageFlags stageFlags, uint32_t descriptorCount,
      vk::DescriptorBindingFlagBits bindingFlags, void* pNext) {
    bindings.push_back(vk::DescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = descriptorType,
        .descriptorCount = descriptorCount,
        .stageFlags = stageFlags,
        .pImmutableSamplers = nullptr,
    });
    bFlags.emplace_back(bindingFlags);
  }

  std::expected<vk::raii::DescriptorSetLayout, std::string>
  DescriptorLayoutBuilder::build(const vk::raii::Device& device,
                                 void* pNext) const {
    vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
        .pNext = pNext,
        .bindingCount = static_cast<uint32_t>(bFlags.size()),
        .pBindingFlags = bFlags.data(),
    };

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{
        .pNext = &bindingFlagsInfo,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    VK_MAKE(descriptorSetLayout,
            device.createDescriptorSetLayout(layoutCreateInfo),
            "Failed to create descriptor set layout.");

    return std::move(descriptorSetLayout);
  }

  std::expected<void, std::string> GrowableDescriptorPool::init(
      const vk::raii::Device& device, std::span<PoolRatios> ratios,
      vk::DescriptorPoolCreateFlags flags, uint32_t poolSize) {
    this->device = &device;
    this->poolSize = poolSize;
    this->poolCreateFlags = flags;
    for (const auto& ratio : ratios) {
      size.push_back(ratio);
    }

    VKH_MAKE(initialPool, createPool(poolSize, size),
             "Failed to create initial descriptor pool.");
    pool = std::move(initialPool);
    poolSize = static_cast<uint32_t>(static_cast<float>(poolSize) * 1.5);

    return {};
  }

  std::expected<vk::raii::DescriptorSet, std::string>
  GrowableDescriptorPool::allocate(const vk::DescriptorSetLayout& layout,
                                   void* pNext) {
    vk::DescriptorSetAllocateInfo allocInfo{
        .pNext = pNext,
        .descriptorPool = **pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    auto allocRes = device->allocateDescriptorSets(allocInfo);
    if (allocRes.result == vk::Result::eErrorOutOfPoolMemory ||
        allocRes.result == vk::Result::eErrorFragmentedPool) {
      // Try to create a new pool and allocate again
      oldPools.emplace_back(std::move(*pool));
      pool.reset();

      VKH_MAKE(newPool, createPool(poolSize, size),
               "Failed to create new descriptor pool.");

      pool = std::move(newPool);

      poolSize = static_cast<uint32_t>(static_cast<float>(poolSize) * 1.5);

      allocInfo.descriptorPool = **pool;
      allocRes = device->allocateDescriptorSets(allocInfo);
    }

    if (allocRes.result != vk::Result::eSuccess) {
      return std::unexpected("Failed to allocate descriptor set.");
    }

    return std::move(allocRes.value.front());
  }

  std::expected<vk::raii::DescriptorPool, std::string>
  GrowableDescriptorPool::createPool(uint32_t setCount,
                                     std::span<PoolRatios> ratios) {
    std::vector<vk::DescriptorPoolSize> poolSizes;
    for (const auto& ratio : ratios) {
      poolSizes.push_back(vk::DescriptorPoolSize{
          .type = ratio.type,
          .descriptorCount =
              static_cast<uint32_t>(ratio.ratio * static_cast<float>(setCount)),
      });
    }

    vk::DescriptorPoolCreateInfo poolCreateInfo{
        .flags = poolCreateFlags,
        .maxSets = setCount,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    VK_MAKE(descriptorPool, device->createDescriptorPool(poolCreateInfo),
            "Failed to create descriptor pool.");

    return std::move(descriptorPool);
  }
} // namespace keptech::vkh
