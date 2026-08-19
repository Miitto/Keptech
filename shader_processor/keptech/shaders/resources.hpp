#pragma once

#include <cstdint>

namespace kt::shaders {
  enum class ShaderResourceType : uint8_t {
    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube,
    Texture1DArray,
    Texture2DArray,
    Texture3DArray,
    Sampler,
    UniformBuffer,
    StorageBuffer,
    RWStorageBuffer
  };

  struct ResourceBinding {
    ShaderResourceType type;
    uint32_t set;
    uint32_t binding;
    bool isPushDescriptor;
  };
} // namespace kt::shaders