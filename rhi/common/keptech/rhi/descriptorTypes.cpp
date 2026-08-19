#include "descriptorTypes.hpp"

namespace kt::rhi {
  bool isImage(DescriptorType type) {
    using U = std::underlying_type_t<DescriptorType>;
    constexpr U imageTypes = static_cast<U>(DescriptorType::CombinedImageSampler) | static_cast<U>(DescriptorType::SampledImage) |
                             static_cast<U>(DescriptorType::StorageImage);
    return (static_cast<U>(type) & imageTypes) != 0;
  }

  bool isBuffer(DescriptorType type) {
    using U = std::underlying_type_t<DescriptorType>;
    constexpr U bufferTypes = static_cast<U>(DescriptorType::UniformBuffer) | static_cast<U>(DescriptorType::StorageBuffer);
    return (static_cast<U>(type) & bufferTypes) != 0;
  }
} // namespace kt::rhi