#pragma once

#include <cstdint>
#include <spdlog/fmt/bundled/format.h>
#include <string>
#include <vector>

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

namespace kt::shaders {
  enum class ShaderStages : uint8_t {
    Vertex = BIT(0),
    Fragment = BIT(1),
    Geometry = BIT(2),
    Compute = BIT(3),
    Mesh = BIT(4),
    Task = BIT(5),
  };

  enum class DataType : uint8_t {
    None,
    Void,
    Bool,
    F16,
    F32,
    F64,
    F16_2,
    F32_2,
    F64_2,
    F16_3,
    F32_3,
    F64_3,
    F16_4,
    F32_4,
    F64_4,
    I8,
    I16,
    I32,
    I64,
    I8_2,
    I16_2,
    I32_2,
    I64_2,
    I8_3,
    I16_3,
    I32_3,
    I64_3,
    I8_4,
    I16_4,
    I32_4,
    I64_4,
    U8,
    U16,
    U32,
    U64,
    U8_2,
    U16_2,
    U32_2,
    U64_2,
    U8_3,
    U16_3,
    U32_3,
    U64_3,
    U8_4,
    U16_4,
    U32_4,
    U64_4,
    F32_4x4,
    Sampler2D,
  };

  struct ShaderStage {
    const char* name;
    ShaderStages stage;
  };

  enum class PrimitiveTopology : uint8_t { TriangleList, TriangleStrip };

  enum class CullMode : uint8_t { None, Front, Back, FrontAndBack };

  enum class InputRate : uint8_t { Vertex, Instance };

  struct VertexLayoutEntry {
    DataType type;
    std::string semantic;
    size_t semanticIndex = 0;
  };

  struct VertexBuffer {
    std::vector<VertexLayoutEntry> layout;
    InputRate inputRate = InputRate::Vertex;
  };

  struct Vertex {
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    CullMode cullMode = CullMode::Back;
    std::vector<VertexBuffer> layout;
  };

  enum class BlendFactor : uint8_t {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
  };

  struct Fragment {
    bool enableBlending = false;
    BlendFactor srcColorBlendFactor = BlendFactor::One;
    BlendFactor dstColorBlendFactor = BlendFactor::Zero;
    bool depthWrite = true;
  };

  enum class ShaderResourceType : uint8_t { Texture2D, Sampler, UniformBuffer, StorageBuffer, RWStorageBuffer };
  struct ResourceBinding {
    ShaderResourceType type;
    uint32_t set;
    uint32_t binding;
  };

  struct ShaderInfo {
    const char* name;
    Vertex vertex;
    Fragment fragment;
    uint32_t bindlessIndex;
    std::vector<ResourceBinding> resources;
    size_t pushConstantSize;
  };

  struct Shader {
    const char* file = nullptr;
#ifdef KT_VULKAN
    std::vector<uint8_t> code;
#elif defined(KT_DX12)
    std::vector<std::vector<uint8_t>> code;
#endif
    std::vector<ShaderStage> stages;
    ShaderInfo info;
  };
} // namespace kt::shaders

template <> struct fmt::formatter<kt::shaders::ShaderStages> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::ShaderStages& stage, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<kt::shaders::DataType> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::DataType& dt, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<kt::shaders::PrimitiveTopology> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::PrimitiveTopology& pt, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<kt::shaders::CullMode> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::CullMode& cm, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<kt::shaders::InputRate> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::InputRate& ir, fmt::format_context& ctx) const;
};