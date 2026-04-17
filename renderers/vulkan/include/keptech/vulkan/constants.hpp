#pragma once

#include <vulkan/vulkan.h>

namespace kt::vkh {
  namespace constants {
    constexpr size_t BLOOM_MIP_LEVELS = 5;
    constexpr uint32_t SHADOW_MAP_SIZE = 4096;
    constexpr uint32_t SSAO_KERNEL_SIZE = 64;
    constexpr uint32_t SSAO_NOISE_SIZE = 4;
    enum TextureIndices : uint32_t {
      Albedo,
      Normal,
      Emissive,
      MetRough,
      Depth,
      DiffuseLight,
      SpecularLight,
      SsaoResult,
      SsaoNoise,
      SsaoBlur,
      CombinedLight,
      BloomFirstMip,
      FirstUserTexture = BloomFirstMip + BLOOM_MIP_LEVELS,
      BloomSource = CombinedLight,
    };
  } // namespace constants

  namespace limits {
    extern VkDeviceSize minUniformBufferOffsetAlignment;
    extern VkDeviceSize minStorageBufferOffsetAlignment;
    extern VkDeviceSize maxPushConstantsSize;
    extern VkDeviceSize maxMemoryAllocationSize;
  } // namespace limits

  namespace ext {
#ifdef KT_PROFILE
    extern PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT vkGetPhysicalDeviceCalibrateableTimeDomainsEXT;
    extern PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT;
#endif
  } // namespace ext
} // namespace kt::vkh
