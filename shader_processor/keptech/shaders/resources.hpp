#pragma once

#include <cstdint>
#include <spdlog/fmt/bundled/format.h>
#include <unordered_map>
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

  struct FieldInfo {
    /// Offset of the field within the buffer or push constant block.
    size_t offset = 0;
    /// Size of the field in bytes.
    size_t size = 0;
    /// Stride of the fields elements in bytes, if the field is an array.
    size_t stride = 0;
  };

  struct BufferInfo {
    size_t sizeOrStride = 0;
    std::unordered_map<std::string, FieldInfo> fieldOffsets{};
  };

  struct ResourceBinding {
    std::string name{};
    ShaderResourceType type{};
    uint32_t binding = ~0u;
    uint32_t count = 0;
    bool isPush = false;
    BufferInfo bufferInfo{};
  };

  struct ResourceSet {
    std::vector<ResourceBinding> resources{};
  };

  struct PushConstantInfo {
    size_t size = 0;
    size_t space = 0;
    std::unordered_map<std::string, FieldInfo> fieldOffsets{};
  };

  struct Resources {
    std::vector<ResourceSet> sets{};
    PushConstantInfo pushConstants{};
  };
} // namespace kt::shaders

template <> struct fmt::formatter<kt::shaders::ShaderResourceType> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(kt::shaders::ShaderResourceType type, fmt::format_context& ctx) const;
};