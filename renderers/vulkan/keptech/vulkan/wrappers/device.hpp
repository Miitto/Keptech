#pragma once

#include "keptech/core/fwd.hpp"
#include "keptech/vulkan/wrappers/fwd.hpp"
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>
namespace kt::vkh {

  class BufferCreateInfo {
  public:
    constexpr BufferCreateInfo(size_t size, VkBufferUsageFlags usage, VmaAllocationCreateFlags allocFlags, VmaMemoryUsage memUsage,
                               const char* name = nullptr) noexcept
        : bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                     .pNext = nullptr,
                     .flags = 0,
                     .size = size,
                     .usage = usage,
                     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                     .queueFamilyIndexCount = 0,
                     .pQueueFamilyIndices = nullptr},
          allocInfo{
              .flags = allocFlags,
              .usage = memUsage,
              .requiredFlags = 0,
              .preferredFlags = 0,
              .memoryTypeBits = 0,
              .pool = nullptr,
              .pUserData = nullptr,
              .priority = 0.f,
          },
          name(name) {}

    void setSize(size_t size) noexcept { bufferInfo.size = size; }
    void setUsage(VkBufferUsageFlags usage) noexcept { bufferInfo.usage = usage; }
    void setAllocFlags(VmaAllocationCreateFlags flags) noexcept { allocInfo.flags = flags; }
    void setMemoryUsage(VmaMemoryUsage usage) noexcept { allocInfo.usage = usage; }

    [[nodiscard]]
    const VkBufferCreateInfo& getBufferInfo() const noexcept {
      return bufferInfo;
    }
    [[nodiscard]]
    const VmaAllocationCreateInfo& getAllocInfo() const noexcept {
      return allocInfo;
    }
    [[nodiscard]]
    const char* getName() const noexcept {
      return name;
    }

  private:
    VkBufferCreateInfo bufferInfo;
    VmaAllocationCreateInfo allocInfo;
    const char* name = nullptr;
  };

  class ImageCreateInfo {
  public:
    constexpr ImageCreateInfo(VkImageType imageType, VkFormat format, VkExtent3D extent, uint32_t mipLevels = 1, uint32_t arrayLayers = 1,
                              const char* name = nullptr) noexcept
        : imageInfo{
              .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .imageType = imageType,
              .format = format,
              .extent = extent,
              .mipLevels = mipLevels,
              .arrayLayers = arrayLayers,
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .tiling = VK_IMAGE_TILING_OPTIMAL,
              .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
              .queueFamilyIndexCount = 0,
              .pQueueFamilyIndices = nullptr,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          },
          viewInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                   .pNext = nullptr,
                   .flags = 0,
                   .image = nullptr,
                   .viewType = VK_IMAGE_VIEW_TYPE_2D,
                   .format = format,
                   .components = {.r = VK_COMPONENT_SWIZZLE_R,
                                  .g = VK_COMPONENT_SWIZZLE_G,
                                  .b = VK_COMPONENT_SWIZZLE_B,
                                  .a = VK_COMPONENT_SWIZZLE_A},
                   .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                        .baseMipLevel = 0,
                                        .levelCount = mipLevels,
                                        .baseArrayLayer = 0,
                                        .layerCount = arrayLayers}},
          allocInfo{.flags = 0,
                    .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                    .requiredFlags = 0,
                    .preferredFlags = 0,
                    .memoryTypeBits = 0,
                    .pool = nullptr,
                    .pUserData = nullptr,
                    .priority = 0.f},
          name(name) {}

    void setImageType(VkImageType type) noexcept { imageInfo.imageType = type; }
    void setFormat(VkFormat format) noexcept {
      imageInfo.format = format;
      viewInfo.format = format;
    }
    void setExtent(VkExtent3D extent) noexcept { imageInfo.extent = extent; }
    void setMipLevels(uint32_t mipLevels) noexcept {
      imageInfo.mipLevels = mipLevels;
      viewInfo.subresourceRange.levelCount = mipLevels;
    }
    void setArrayLayers(uint32_t arrayLayers) noexcept {
      imageInfo.arrayLayers = arrayLayers;
      viewInfo.subresourceRange.layerCount = arrayLayers;
    }
    void setSamples(VkSampleCountFlagBits samples) noexcept { imageInfo.samples = samples; }
    void setTiling(VkImageTiling tiling) noexcept { imageInfo.tiling = tiling; }
    void setUsage(VkImageUsageFlags usage) noexcept { imageInfo.usage = usage; }
    void setSharingMode(VkSharingMode sharingMode) noexcept { imageInfo.sharingMode = sharingMode; }
    void setInitialLayout(VkImageLayout layout) noexcept { imageInfo.initialLayout = layout; }
    void setViewType(VkImageViewType viewType) noexcept { viewInfo.viewType = viewType; }
    void setAspectMask(VkImageAspectFlags aspectMask) noexcept { viewInfo.subresourceRange.aspectMask = aspectMask; }
    void setAllocFlags(VmaAllocationCreateFlags flags) noexcept { allocInfo.flags = flags; }
    void setMemoryUsage(VmaMemoryUsage usage) noexcept { allocInfo.usage = usage; }

    [[nodiscard]]
    const VkImageCreateInfo& getImageInfo() const noexcept {
      return imageInfo;
    }
    [[nodiscard]]
    const VkImageViewCreateInfo& getViewInfo() const noexcept {
      return viewInfo;
    }
    [[nodiscard]]
    const VmaAllocationCreateInfo& getAllocInfo() const noexcept {
      return allocInfo;
    }

    [[nodiscard]]
    const char* getName() const noexcept {
      return name;
    }

  private:
    VkImageCreateInfo imageInfo;
    VkImageViewCreateInfo viewInfo;
    VmaAllocationCreateInfo allocInfo;
    const char* name = nullptr;
  };

  class Device {
  public:
    Device() = default;
    Device(VkPhysicalDevice physical, VkDevice logical, VmaAllocator allocator) noexcept
        : physical(physical), logical(logical), allocator(allocator) {}

    operator VkPhysicalDevice() const noexcept { return physical; }
    operator VkDevice() const noexcept { return logical; }
    operator VmaAllocator() const noexcept { return allocator; }

    [[nodiscard]]
    Result<Buffer, VkResult, VK_SUCCESS> createBuffer(const BufferCreateInfo& info) const;
    [[nodiscard]]
    Result<Image, VkResult, VK_SUCCESS> createImage(const ImageCreateInfo& info) const;

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
} // namespace kt::vkh
