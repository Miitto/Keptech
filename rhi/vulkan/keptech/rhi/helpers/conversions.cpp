#include "conversions.hpp"
#include "vk-logger.hpp"

namespace kt::rhi {
  VkShaderStageFlagBits from(shaders::ShaderStages stage) {
    switch (stage) {
    case shaders::ShaderStages::Vertex:
      return VK_SHADER_STAGE_VERTEX_BIT;
    case shaders::ShaderStages::Fragment:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    case shaders::ShaderStages::Geometry:
      return VK_SHADER_STAGE_GEOMETRY_BIT;
    case shaders::ShaderStages::Compute:
      return VK_SHADER_STAGE_COMPUTE_BIT;
    case shaders::ShaderStages::Mesh:
      return VK_SHADER_STAGE_MESH_BIT_EXT;
    case shaders::ShaderStages::Task:
      return VK_SHADER_STAGE_TASK_BIT_EXT;
    default:
      VK_CRITICAL("Unsupported shader stage: {}", static_cast<int>(stage));
      std::abort();
    }
  }

  VkFormat from(shaders::DataType type) {
    using T = shaders::DataType;
#define F(_F) return VkFormat::VK_FORMAT_##_F
    switch (type) {
    case T::F32:
      F(R32_SFLOAT);
    case T::F32_2:
      F(R32G32_SFLOAT);
    case T::F32_3:
      F(R32G32B32_SFLOAT);
    case T::F32_4:
      F(R32G32B32A32_SFLOAT);
    case T::U8:
      F(R8_UINT);
    case T::U8_2:
      F(R8G8_UINT);
    case T::U8_3:
      F(R8G8B8_UINT);
    case T::U8_4:
      F(R8G8B8A8_UINT);
    case T::U16:
      F(R16_UINT);
    case T::U16_2:
      F(R16G16_UINT);
    case T::U16_3:
      F(R16G16B16_UINT);
    case T::U16_4:
      F(R16G16B16A16_UINT);
    case T::U32:
      F(R32_UINT);
    case T::U32_2:
      F(R32G32_UINT);
    case T::U32_3:
      F(R32G32B32_UINT);
    case T::U32_4:
      F(R32G32B32A32_UINT);
    case shaders::DataType::None:
    case shaders::DataType::Void:
      F(UNDEFINED);
    case shaders::DataType::Bool:
      F(R8_UINT);
    case shaders::DataType::F16:
      F(R16_SFLOAT);
    case shaders::DataType::F64:
      F(R64_SFLOAT);
    case shaders::DataType::F16_2:
      F(R16G16_SFLOAT);
    case shaders::DataType::F64_2:
      F(R64G64_SFLOAT);
    case shaders::DataType::F16_3:
      F(R16G16B16_SFLOAT);
    case shaders::DataType::F64_3:
      F(R64G64B64_SFLOAT);
    case shaders::DataType::F16_4:
      F(R16G16B16A16_SFLOAT);
    case shaders::DataType::F64_4:
      F(R64G64B64A64_SFLOAT);
    case shaders::DataType::I8:
      F(R8_SINT);
    case shaders::DataType::I16:
      F(R16_SINT);
    case shaders::DataType::I32:
      F(R32_SINT);
    case shaders::DataType::I64:
      F(R64_SINT);
    case shaders::DataType::I8_2:
      F(R8G8_SINT);
    case shaders::DataType::I16_2:
      F(R16G16_SINT);
    case shaders::DataType::I32_2:
      F(R32G32_SINT);
    case shaders::DataType::I64_2:
      F(R64G64_SINT);
    case shaders::DataType::I8_3:
      F(R8G8B8_SINT);
    case shaders::DataType::I16_3:
      F(R16G16B16_SINT);
    case shaders::DataType::I32_3:
      F(R32G32B32_SINT);
    case shaders::DataType::I64_3:
      F(R64G64B64_SINT);
    case shaders::DataType::I8_4:
      F(R8G8B8A8_SINT);
    case shaders::DataType::I16_4:
      F(R16G16B16A16_SINT);
    case shaders::DataType::I32_4:
      F(R32G32B32A32_SINT);
    case shaders::DataType::I64_4:
      F(R64G64B64A64_SINT);
    case shaders::DataType::U64:
      F(R64_UINT);
    case shaders::DataType::U64_2:
      F(R64G64_UINT);
    case shaders::DataType::U64_3:
      F(R64G64B64_UINT);
    case shaders::DataType::U64_4:
      F(R64G64B64A64_UINT);
    case shaders::DataType::F32_4x4:
      F(R32G32B32A32_SFLOAT);
    case shaders::DataType::Sampler2D:
      F(UNDEFINED);
    }
  }

  uint32_t getSize(shaders::DataType type) {
    using T = shaders::DataType;
    switch (type) {
    case T::F32:
      return 4;
    case T::F32_2:
      return 8;
    case T::F32_3:
      return 12;
    case T::F32_4:
      return 16;
    case T::U8:
      return 1;
    case T::U8_2:
      return 2;
    case T::U8_3:
      return 3;
    case T::U8_4:
      return 4;
    case T::U16:
      return 2;
    case T::U16_2:
      return 4;
    case T::U16_3:
      return 6;
    case T::U16_4:
      return 8;
    case T::U32:
      return 4;
    case T::U32_2:
      return 8;
    case T::U32_3:
      return 12;
    case T::U32_4:
      return 16;
    case shaders::DataType::None:
    case shaders::DataType::Void:
      return 0;
    case shaders::DataType::Bool:
      return 1;
    case shaders::DataType::F16:
      return 2;
    case shaders::DataType::F64:
      return 8;
    case shaders::DataType::F16_2:
      return 4;
    case shaders::DataType::F64_2:
      return 16;
    case shaders::DataType::F16_3:
      return 6;
    case shaders::DataType::F64_3:
      return 24;
    case shaders::DataType::F16_4:
      return 8;
    case shaders::DataType::F64_4:
      return 32;
    case shaders::DataType::I8:
      return 1;
    case shaders::DataType::I16:
      return 2;
    case shaders::DataType::I32:
      return 4;
    case shaders::DataType::I64:
      return 8;
    case shaders::DataType::I8_2:
      return 2;
    case shaders::DataType::I16_2:
      return 4;
    case shaders::DataType::I32_2:
      return 8;
    case shaders::DataType::I64_2:
      return 16;
    case shaders::DataType::I8_3:
      return 3;
    case shaders::DataType::I16_3:
      return 6;
    case shaders::DataType::I32_3:
      return 12;
    case shaders::DataType::I64_3:
      return 24;
    case shaders::DataType::I8_4:
      return 4;
    case shaders::DataType::I16_4:
      return 8;
    case shaders::DataType::I32_4:
      return 16;
    case shaders::DataType::I64_4:
      return 32;
    case shaders::DataType::U64:
      return 8;
    case shaders::DataType::U64_2:
      return 16;
    case shaders::DataType::U64_3:
      return 24;
    case shaders::DataType::U64_4:
      return 32;
    case shaders::DataType::F32_4x4:
      return 64;
    case shaders::DataType::Sampler2D:
      return 0;
    }
  }

} // namespace kt::rhi
