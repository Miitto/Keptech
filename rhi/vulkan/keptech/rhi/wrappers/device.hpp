#pragma once

#include "keptech/core/fwd.hpp"
#include "keptech/rhi/wrappers/fwd.hpp"
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

namespace kt::rhi {
  struct Pipeline;
  class RHI;

  class Device {
  public:
    friend class RHI;
    Device() = default;
    Device(VkPhysicalDevice physical, VkDevice logical, VmaAllocator allocator) noexcept
        : physical(physical), logical(logical), allocator(allocator) {}

    operator VkPhysicalDevice() const noexcept { return physical; }
    operator VkDevice() const noexcept { return logical; }
    operator VmaAllocator() const noexcept { return allocator; }

    [[nodiscard]]
    Result<VkPipelineLayout, VkResult, VK_SUCCESS> createPipelineLayout(const VkPipelineLayoutCreateInfo& info) const;
    [[nodiscard]]
    Result<Pipeline, VkResult, VK_SUCCESS> createPipeline(const VkGraphicsPipelineCreateInfo& info) const;
    [[nodiscard]]
    Result<Pipeline, VkResult, VK_SUCCESS> createPipeline(const VkComputePipelineCreateInfo& info) const;

    operator bool() const noexcept { return physical != VK_NULL_HANDLE && logical != VK_NULL_HANDLE && allocator != nullptr; }

    const Device& setAllocationName(VmaAllocation alloc, const char* name) const;

    template <typename T> const Device& setDebugName(VkObjectType objectType, T object, const char* name) const {
#ifndef NDEBUG

      VkDebugUtilsObjectNameInfoEXT nameInfo{
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
          .pNext = nullptr,
          .objectType = objectType,
          .objectHandle = reinterpret_cast<uint64_t>(object),
          .pObjectName = name,
      };
      vkSetDebugUtilsObjectNameEXT(logical, &nameInfo);
#endif
      return *this;
    }

    const Device& setDebugName(const Image& image, const char* name) const;
    const Device& setDebugName(VkBuffer buffer, const char* name) const;
    const Device& setDebugName(VkImage image, const char* name) const;
    const Device& setDebugName(VkImageView imageView, const char* name) const;
    const Device& setDebugName(VkSampler sampler, const char* name) const;
    const Device& setDebugName(VkPipeline pipeline, const char* name) const;
    const Device& setDebugName(VkPipelineLayout pipelineLayout, const char* name) const;
    const Device& setDebugName(VkShaderModule shaderModule, const char* name) const;
    const Device& setDebugName(VkDescriptorSet descriptorSet, const char* name) const;
    const Device& setDebugName(VkDescriptorSetLayout descriptorSetLayout, const char* name) const;
    const Device& setDebugName(VkDescriptorPool descriptorPool, const char* name) const;
    const Device& setDebugName(VkSemaphore semaphore, const char* name) const;
    const Device& setDebugName(VkFence fence, const char* name) const;
    const Device& setDebugName(VkCommandPool commandPool, const char* name) const;
    const Device& setDebugName(VkCommandBuffer commandBuffer, const char* name) const;
    const Device& setDebugName(VkQueue queue, const char* name) const;

    void destroy();

  private:
    VkPhysicalDevice physical;
    VkDevice logical;
    VmaAllocator allocator;
  };
} // namespace kt::rhi
