#pragma once

#include <Volk/volk.h>
#include <deque>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kt::vkh {

  class DescriptorLayoutBuilder {
  public:
    constexpr static VkDescriptorBindingFlags INDEX_BINDING_FLAGS =
        VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
        VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    void clear() { bindings.clear(); }

    void addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount = 1,
                    VkDescriptorBindingFlags bindingFlags = 0, void* pNext = nullptr);

    std::expected<VkDescriptorSetLayout, std::string> build(const VkDevice& device, void* pNext = nullptr) const;

  private:
    std::vector<VkDescriptorSetLayoutBinding> bindings{};
    std::vector<VkDescriptorBindingFlags> bFlags = {};
  };

  class GrowableDescriptorPool {
  public:
    struct PoolRatios {
      VkDescriptorType type;
      float ratio;
    };

    std::expected<void, std::string> init(const VkDevice& device, std::span<PoolRatios> ratios,
                                          VkDescriptorPoolCreateFlags poolCreateFlags = {}, uint32_t poolSize = 256);

    std::expected<VkDescriptorSet, std::string> allocate(const VkDescriptorSetLayout& layout, void* pNext = nullptr);

    void reset() {
      if (pool) {
        vkResetDescriptorPool(device, pool, 0);
      }

      for (auto& oldPool : oldPools) {
        vkResetDescriptorPool(device, oldPool, 0);
      }
    }

  private:
    std::expected<VkDescriptorPool, std::string> createPool(uint32_t setCount, std::span<PoolRatios> ratios);

    VkDevice device;
    uint32_t poolSize;
    VkDescriptorPool pool = nullptr;
    std::vector<VkDescriptorPool> oldPools{};

    std::vector<PoolRatios> size{};
    VkDescriptorPoolCreateFlags poolCreateFlags = {};
  };

  struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> imagnInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    enum class ImageType : uint8_t {
      Sampler = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER,
      CombinedImageSampler = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      SampledImage = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      StorageImage = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    };

    void writeImage(uint32_t binding, const VkDescriptorImageInfo& imageInfo, ImageType type, void* pNext = nullptr) {
      imagnInfos.push_back(imageInfo);
      writes.push_back(VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = pNext,
          .dstBinding = binding,
          .descriptorCount = 1,
          .descriptorType = static_cast<VkDescriptorType>(type),
          .pImageInfo = &imagnInfos.back(),
      });
    }

    enum class BufferType : uint8_t {
      Uniform = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      Storage = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      UniformDynamic = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
      StorageDynamic = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    };

    void writeBuffer(uint32_t binding, const VkDescriptorBufferInfo& bufferInfo, BufferType type, void* pNext = nullptr) {
      bufferInfos.push_back(bufferInfo);
      writes.push_back(VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = pNext,
          .dstBinding = binding,
          .descriptorCount = 1,
          .descriptorType = static_cast<VkDescriptorType>(type),
          .pBufferInfo = &bufferInfos.back(),
      });
    }

    void clear() {
      imagnInfos.clear();
      bufferInfos.clear();
      writes.clear();
    }

    void update(const VkDevice& device, const VkDescriptorSet& descriptorSet) {
      for (auto& write : writes) {
        write.dstSet = descriptorSet;
      }
      vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
    }
  };
} // namespace kt::vkh
