#include "keptech/rhi/descriptorTypes.hpp"

namespace kt::rhi {
  RawDescriptorType raw(DescriptorType type) {
    switch (type) {
    case DescriptorType::Sampler:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    case DescriptorType::CombinedImageSampler:
    case DescriptorType::SampledImage:
    case DescriptorType::StorageBuffer:
      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case DescriptorType::StorageImage:
    case DescriptorType::RWStorageBuffer:
      return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case DescriptorType::UniformBuffer:
      return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    }
  }
} // namespace kt::rhi