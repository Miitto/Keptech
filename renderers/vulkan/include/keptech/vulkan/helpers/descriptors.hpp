#pragma once

#include <deque>
#include <expected>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {

  class DescriptorLayoutBuilder {
  public:
    void clear() { bindings.clear(); }

    void addBinding(uint32_t binding, vk::DescriptorType descriptorType,
                    vk::ShaderStageFlags stageFlags,
                    uint32_t descriptorCount = 1, void* pNext = nullptr);

    std::expected<vk::raii::DescriptorSetLayout, std::string>
    build(const vk::raii::Device& device, void* pNext = nullptr) const;

  private:
    std::vector<vk::DescriptorSetLayoutBinding> bindings{};
  };

  class GrowableDescriptorPool {
  public:
    struct PoolRatios {
      vk::DescriptorType type;
      float ratio;
    };

    std::expected<void, std::string> init(const vk::raii::Device& device,
                                          std::span<PoolRatios> ratios,
                                          bool individualFree = false,
                                          uint32_t poolSize = 256);

    std::expected<vk::raii::DescriptorSet, std::string>
    allocate(const vk::DescriptorSetLayout& layout, void* pNext = nullptr);

    void reset() {
      if (pool) {
        pool->reset(vk::DescriptorPoolResetFlags{});
      }

      for (auto& oldPool : oldPools) {
        oldPool.reset(vk::DescriptorPoolResetFlags{});
      }
    }

  private:
    std::expected<vk::raii::DescriptorPool, std::string>
    createPool(uint32_t setCount, std::span<PoolRatios> ratios);

    const vk::raii::Device* device;
    bool individualFree = false;
    uint32_t poolSize;
    std::optional<vk::raii::DescriptorPool> pool;
    std::vector<vk::raii::DescriptorPool> oldPools{};

    std::vector<PoolRatios> size{};
  };

  struct DescriptorWriter {
    std::deque<vk::DescriptorImageInfo> imagnInfos;
    std::deque<vk::DescriptorBufferInfo> bufferInfos;
    std::vector<vk::WriteDescriptorSet> writes;

    enum class ImageType : uint8_t {
      Sampler = static_cast<int>(vk::DescriptorType::eSampler),
      CombinedImageSampler =
          static_cast<int>(vk::DescriptorType::eCombinedImageSampler),
      SampledImage = static_cast<int>(vk::DescriptorType::eSampledImage),
      StorageImage = static_cast<int>(vk::DescriptorType::eStorageImage),
    };

    void writeImage(uint32_t binding, const vk::DescriptorImageInfo& imageInfo,
                    ImageType type, void* pNext = nullptr) {
      imagnInfos.push_back(imageInfo);
      writes.push_back(vk::WriteDescriptorSet{
          .pNext = pNext,
          .dstBinding = binding,
          .descriptorCount = 1,
          .descriptorType = static_cast<vk::DescriptorType>(type),
          .pImageInfo = &imagnInfos.back(),
      });
    }

    enum class BufferType : uint8_t {
      Uniform = static_cast<int>(vk::DescriptorType::eUniformBuffer),
      Storage = static_cast<int>(vk::DescriptorType::eStorageBuffer),
      UniformDynamic =
          static_cast<int>(vk::DescriptorType::eUniformBufferDynamic),
      StorageDynamic =
          static_cast<int>(vk::DescriptorType::eStorageBufferDynamic),
    };

    void writeBuffer(uint32_t binding,
                     const vk::DescriptorBufferInfo& bufferInfo,
                     BufferType type, void* pNext = nullptr) {
      bufferInfos.push_back(bufferInfo);
      writes.push_back(vk::WriteDescriptorSet{
          .pNext = pNext,
          .dstBinding = binding,
          .descriptorCount = 1,
          .descriptorType = static_cast<vk::DescriptorType>(type),
          .pBufferInfo = &bufferInfos.back(),
      });
    }

    void clear() {
      imagnInfos.clear();
      bufferInfos.clear();
      writes.clear();
    }

    void update(const vk::raii::Device& device,
                const vk::DescriptorSet& descriptorSet) {
      for (auto& write : writes) {
        write.dstSet = descriptorSet;
      }
      device.updateDescriptorSets(writes, {});
    }
  };
} // namespace keptech::vkh
