#pragma once

#include <cstdint>
#include <spdlog/fmt/bundled/format.h>
#include <vector>

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
    uint32_t binding;
    uint32_t count;
    bool isPush = false;
  };

  struct ResourceSet {
    uint8_t space;
    std::vector<ResourceBinding> resources;
  };
} // namespace kt::shaders

template <> struct fmt::formatter<kt::shaders::ShaderResourceType> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::ShaderResourceType& type, fmt::format_context& ctx) const;
};