#include "device.hpp"
#include "keptech/core/result.hpp"
#include "wrappers/image.hpp"
#include "wrappers/pipeline.hpp"

namespace kt::rdr {

  kt::Result<VkPipelineLayout, VkResult, VK_SUCCESS> Device::createPipelineLayout(const VkPipelineLayoutCreateInfo& info) const {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    auto res = vkCreatePipelineLayout(logical, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
      return {res};
    }
    return {layout};
  }

  kt::Result<Pipeline, VkResult, VK_SUCCESS> Device::createPipeline(const VkGraphicsPipelineCreateInfo& info) const {
    VkPipeline pipeline = VK_NULL_HANDLE;
    auto res = vkCreateGraphicsPipelines(logical, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    if (res != VK_SUCCESS) {
      return {res};
    }
    return {Pipeline{.layout = info.layout, .pipeline = pipeline}};
  }
  kt::Result<Pipeline, VkResult, VK_SUCCESS> Device::createPipeline(const VkComputePipelineCreateInfo& info) const {
    VkPipeline pipeline = VK_NULL_HANDLE;
    auto res = vkCreateComputePipelines(logical, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    if (res != VK_SUCCESS) {
      return {res};
    }
    return {Pipeline{.layout = info.layout, .pipeline = pipeline}};
  }

  const Device& Device::setAllocationName(VmaAllocation alloc, const char* name) const {
    vmaSetAllocationName(allocator, alloc, name);
    return *this;
  }

  const Device& Device::setDebugName(const Image& image, const char* name) const {
#ifndef NDEBUG
    setDebugName(VK_OBJECT_TYPE_IMAGE, static_cast<VkImage>(image), name);
    std::string viewName = std::string(name) + "_view";
    setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, static_cast<VkImageView>(image), viewName.c_str());
#endif
    return *this;
  }

  const Device& Device::setDebugName(VkBuffer buffer, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_BUFFER, buffer, name);
    return *this;
  }

  const Device& Device::setDebugName(VkImage image, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_IMAGE, image, name);
    return *this;
  }

  const Device& Device::setDebugName(VkImageView imageView, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, imageView, name);
    return *this;
  }

  const Device& Device::setDebugName(VkSampler sampler, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_SAMPLER, sampler, name);
    return *this;
  }
  const Device& Device::setDebugName(VkPipeline pipeline, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_PIPELINE, pipeline, name);
    return *this;
  }
  const Device& Device::setDebugName(VkPipelineLayout pipelineLayout, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, pipelineLayout, name);
    return *this;
  }
  const Device& Device::setDebugName(VkShaderModule shaderModule, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_SHADER_MODULE, shaderModule, name);
    return *this;
  }
  const Device& Device::setDebugName(VkDescriptorSet descriptorSet, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, descriptorSet, name);
    return *this;
  }
  const Device& Device::setDebugName(VkDescriptorSetLayout descriptorSetLayout, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, descriptorSetLayout, name);
    return *this;
  }
  const Device& Device::setDebugName(VkDescriptorPool descriptorPool, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_POOL, descriptorPool, name);
    return *this;
  }
  const Device& Device::setDebugName(VkSemaphore semaphore, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_SEMAPHORE, semaphore, name);
    return *this;
  }
  const Device& Device::setDebugName(VkFence fence, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_FENCE, fence, name);
    return *this;
  }
  const Device& Device::setDebugName(VkCommandPool commandPool, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_COMMAND_POOL, commandPool, name);
    return *this;
  }
  const Device& Device::setDebugName(VkCommandBuffer commandBuffer, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, commandBuffer, name);
    return *this;
  }
  const Device& Device::setDebugName(VkQueue queue, const char* name) const {
    setDebugName(VK_OBJECT_TYPE_QUEUE, queue, name);
    return *this;
  }

  void Device::destroy() {
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(logical, nullptr);
  }
} // namespace kt::rdr