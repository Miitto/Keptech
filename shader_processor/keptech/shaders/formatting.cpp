#include "shader.hpp"

#include <spdlog/fmt/bundled/ranges.h>

using namespace kt::shaders;

fmt::format_context::iterator fmt::formatter<ShaderStages>::format(const ShaderStages& stage, fmt::format_context& ctx) const {
  std::vector<std::string_view> stages;

  if (static_cast<uint8_t>(stage) & static_cast<uint8_t>(ShaderStages::Vertex)) {
    stages.push_back("Vertex");
  }

  if (static_cast<uint8_t>(stage) & static_cast<uint8_t>(ShaderStages::Fragment)) {
    stages.push_back("Fragment");
  }

  if (static_cast<uint8_t>(stage) & static_cast<uint8_t>(ShaderStages::Geometry)) {
    stages.push_back("Geometry");
  }

  if (static_cast<uint8_t>(stage) & static_cast<uint8_t>(ShaderStages::Mesh)) {
    stages.push_back("Mesh");
  }

  if (static_cast<uint8_t>(stage) & static_cast<uint8_t>(ShaderStages::Task)) {
    stages.push_back("Task");
  }

  if (static_cast<uint8_t>(stage) & static_cast<uint8_t>(ShaderStages::Compute)) {
    stages.push_back("Compute");
  }

  return fmt::format_to(ctx.out(), "{}", fmt::join(stages, " | "));
}

fmt::format_context::iterator fmt::formatter<DataType>::format(const DataType& dt, fmt::format_context& ctx) const {
#define C(x)                                                                                                                               \
  case DataType::x:                                                                                                                        \
    return fmt::format_to(ctx.out(), #x);
  switch (dt) {
    C(None)
    C(Void)
    C(Bool)
    C(F16)
    C(F32)
    C(F64)
    C(F16_2)
    C(F32_2)
    C(F64_2)
    C(F16_3)
    C(F32_3)
    C(F64_3)
    C(F16_4)
    C(F32_4)
    C(F64_4)
    C(I8)
    C(I16)
    C(I32)
    C(I64)
    C(I8_2)
    C(I16_2)
    C(I32_2)
    C(I64_2)
    C(I8_3)
    C(I16_3)
    C(I32_3)
    C(I64_3)
    C(I8_4)
    C(I16_4)
    C(I32_4)
    C(I64_4)
    C(U8)
    C(U16)
    C(U32)
    C(U64)
    C(U8_2)
    C(U16_2)
    C(U32_2)
    C(U64_2)
    C(U8_3)
    C(U16_3)
    C(U32_3)
    C(U64_3)
    C(U8_4)
    C(U16_4)
    C(U32_4)
    C(U64_4)
    C(F32_4x4)
    C(Sampler2D)
  }
}

fmt::format_context::iterator fmt::formatter<PrimitiveTopology>::format(const PrimitiveTopology& pt, fmt::format_context& ctx) const {
  switch (pt) {
  case PrimitiveTopology::TriangleList:
    return fmt::format_to(ctx.out(), "TriangleList");
  case PrimitiveTopology::TriangleStrip:
    return fmt::format_to(ctx.out(), "TriangleStrip");
  }
}

fmt::format_context::iterator fmt::formatter<CullMode>::format(const CullMode& cm, fmt::format_context& ctx) const {
  switch (cm) {
  case CullMode::None:
    return fmt::format_to(ctx.out(), "None");
  case CullMode::Front:
    return fmt::format_to(ctx.out(), "Front");
  case CullMode::Back:
    return fmt::format_to(ctx.out(), "Back");
  case CullMode::FrontAndBack:
    return fmt::format_to(ctx.out(), "FrontAndBack");
  }
}

fmt::format_context::iterator fmt::formatter<InputRate>::format(const InputRate& ir, fmt::format_context& ctx) const {
  switch (ir) {
  case InputRate::Vertex:
    return fmt::format_to(ctx.out(), "Vertex");
  case InputRate::Instance:
    return fmt::format_to(ctx.out(), "Instance");
  }
}

fmt::format_context::iterator fmt::formatter<ShaderResourceType>::format(const ShaderResourceType& srt, fmt::format_context& ctx) const {
  switch (srt) {
  case ShaderResourceType::UniformBuffer:
    return fmt::format_to(ctx.out(), "UniformBuffer");
  case ShaderResourceType::StorageBuffer:
    return fmt::format_to(ctx.out(), "StorageBuffer");
  case ShaderResourceType::RWStorageBuffer:
    return fmt::format_to(ctx.out(), "RWStorageBuffer");
  case ShaderResourceType::Texture1D:
    return fmt::format_to(ctx.out(), "Texture1D");
  case ShaderResourceType::Texture2D:
    return fmt::format_to(ctx.out(), "Texture2D");
  case ShaderResourceType::Texture3D:
    return fmt::format_to(ctx.out(), "Texture3D");
  case ShaderResourceType::TextureCube:
    return fmt::format_to(ctx.out(), "TextureCube");
  case ShaderResourceType::Texture1DArray:
    return fmt::format_to(ctx.out(), "Texture1DArray");
  case ShaderResourceType::Texture2DArray:
    return fmt::format_to(ctx.out(), "Texture2DArray");
  case ShaderResourceType::Texture3DArray:
    return fmt::format_to(ctx.out(), "Texture3DArray");
  case ShaderResourceType::Sampler:
    return fmt::format_to(ctx.out(), "Sampler");
    break;
  }
}