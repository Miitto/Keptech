#include "keptech/vulkan/constants.hpp"
#include <vulkan/vulkan_raii.hpp>

namespace kt::vkh::limits {
  VkDeviceSize minUniformBufferOffsetAlignment = 0;
  VkDeviceSize minStorageBufferOffsetAlignment = 0;
  VkDeviceSize maxPushConstantsSize = 0;
  VkDeviceSize maxMemoryAllocationSize = 0;
} // namespace kt::vkh::limits

vk::raii::ImageView test(vk::raii::Device& d, const vk::Image& img) {
  vk::ImageViewCreateInfo createInfo = {
      .image = img,
  };

  auto viewRes = d.createImageView(createInfo);
  return std::move(viewRes.value);
};

void otherTest(vk::raii::Device& d, const vk::raii::Image& img) { test(d, *img); }
