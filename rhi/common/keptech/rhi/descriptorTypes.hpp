#pragma once

#ifdef KT_VULKAN
#include <Volk/volk.h>
#elif defined(KT_DX12)
#include <d3d12.h>
#else
#error "Unsupported RHI backend for descriptorTypes.hpp"
#endif

namespace kt::rhi {

#ifdef KT_VULKAN
  using RawDescriptorType = VkDescriptorType;
#elif defined(KT_DX12)
  using RawDescriptorType = D3D12_DESCRIPTOR_RANGE_TYPE;
#endif

  enum class DescriptorType : uint8_t {
    Sampler,
    CombinedImageSampler,
    SampledImage,
    StorageImage,
    UniformBuffer,
    StorageBuffer,
    RWStorageBuffer,
  };

  RawDescriptorType raw(DescriptorType type);

  bool isImage(DescriptorType type);
  bool isBuffer(DescriptorType type);
} // namespace kt::rhi