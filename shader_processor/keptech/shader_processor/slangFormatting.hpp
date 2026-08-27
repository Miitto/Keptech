#pragma once

#include <slang.h>
#include <spdlog/fmt/bundled/format.h>
#include <spdlog/fmt/bundled/ranges.h>
#include <string_view>
#include <vector>

template <> struct fmt::formatter<SlangResult> : formatter<std::string_view> {
  template <typename FormatContext> auto format(const SlangResult& res, FormatContext& ctx) const {
    switch (res) {
    case SLANG_OK:
      return fmt::format_to(ctx.out(), "OK");
    case SLANG_E_NOT_IMPLEMENTED:
      return fmt::format_to(ctx.out(), "E_NOT_IMPLEMENTED");
    case SLANG_E_OUT_OF_MEMORY:
      return fmt::format_to(ctx.out(), "E_OUT_OF_MEMORY");
    case SLANG_E_INVALID_ARG:
      return fmt::format_to(ctx.out(), "E_INVALID_ARG");
    case SLANG_E_NOT_FOUND:
      return fmt::format_to(ctx.out(), "E_NOT_FOUND");
    default:
      return fmt::format_to(ctx.out(), "Unknown SlangResult");
    }
  }
};

template <> struct fmt::formatter<slang::TypeReflection::Kind> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const slang::TypeReflection::Kind& kind, FormatContext& ctx) const {
    switch (kind) {
    case slang::TypeReflection::Kind::None:
      return fmt::format_to(ctx.out(), "None");
    case slang::TypeReflection::Kind::Struct:
      return fmt::format_to(ctx.out(), "Struct");
    case slang::TypeReflection::Kind::Array:
      return fmt::format_to(ctx.out(), "Array");
    case slang::TypeReflection::Kind::Matrix:
      return fmt::format_to(ctx.out(), "Matrix");
    case slang::TypeReflection::Kind::Vector:
      return fmt::format_to(ctx.out(), "Vector");
    case slang::TypeReflection::Kind::Scalar:
      return fmt::format_to(ctx.out(), "Scalar");
    case slang::TypeReflection::Kind::ConstantBuffer:
      return fmt::format_to(ctx.out(), "ConstantBuffer");
    case slang::TypeReflection::Kind::Resource:
      return fmt::format_to(ctx.out(), "Resource");
    case slang::TypeReflection::Kind::SamplerState:
      return fmt::format_to(ctx.out(), "SamplerState");
    case slang::TypeReflection::Kind::TextureBuffer:
      return fmt::format_to(ctx.out(), "TextureBuffer");
    case slang::TypeReflection::Kind::ShaderStorageBuffer:
      return fmt::format_to(ctx.out(), "ShaderStorageBuffer");
    case slang::TypeReflection::Kind::ParameterBlock:
      return fmt::format_to(ctx.out(), "ParameterBlock");
    case slang::TypeReflection::Kind::GenericTypeParameter:
      return fmt::format_to(ctx.out(), "GenericTypeParameter");
    case slang::TypeReflection::Kind::Interface:
      return fmt::format_to(ctx.out(), "Interface");
    case slang::TypeReflection::Kind::OutputStream:
      return fmt::format_to(ctx.out(), "OutputStream");
    case slang::TypeReflection::Kind::Specialized:
      return fmt::format_to(ctx.out(), "Specialized");
    case slang::TypeReflection::Kind::Feedback:
      return fmt::format_to(ctx.out(), "Feedback");
    case slang::TypeReflection::Kind::Pointer:
      return fmt::format_to(ctx.out(), "Pointer");
    case slang::TypeReflection::Kind::DynamicResource:
      return fmt::format_to(ctx.out(), "DynamicResource");
    case slang::TypeReflection::Kind::MeshOutput:
      return fmt::format_to(ctx.out(), "MeshOutput");
    case slang::TypeReflection::Kind::Enum:
      return fmt::format_to(ctx.out(), "Enum");
    }
  }
};

template <> struct fmt::formatter<slang::ParameterCategory> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const slang::ParameterCategory& category, FormatContext& ctx) const {
    switch (category) {
    case slang::ParameterCategory::None:
      return fmt::format_to(ctx.out(), "None");
    case slang::ParameterCategory::ConstantBuffer:
      return fmt::format_to(ctx.out(), "ConstantBuffer");
    case slang::Mixed:
      return fmt::format_to(ctx.out(), "Mixed");
    case slang::ShaderResource:
      return fmt::format_to(ctx.out(), "ShaderResource");
    case slang::UnorderedAccess:
      return fmt::format_to(ctx.out(), "UnorderedAccess");
    case slang::VaryingInput:
      return fmt::format_to(ctx.out(), "VaryingInput");
    case slang::VaryingOutput:
      return fmt::format_to(ctx.out(), "VaryingOutput");
    case slang::SamplerState:
      return fmt::format_to(ctx.out(), "SamplerState");
    case slang::Uniform:
      return fmt::format_to(ctx.out(), "Uniform");
    case slang::DescriptorTableSlot:
      return fmt::format_to(ctx.out(), "DescriptorTableSlot");
    case slang::SpecializationConstant:
      return fmt::format_to(ctx.out(), "SpecializationConstant");
    case slang::PushConstantBuffer:
      return fmt::format_to(ctx.out(), "PushConstantBuffer");
    case slang::RegisterSpace:
      return fmt::format_to(ctx.out(), "RegisterSpace");
    case slang::GenericResource:
      return fmt::format_to(ctx.out(), "GenericResource");
    case slang::RayPayload:
      return fmt::format_to(ctx.out(), "RayPayload");
    case slang::HitAttributes:
      return fmt::format_to(ctx.out(), "HitAttributes");
    case slang::CallablePayload:
      return fmt::format_to(ctx.out(), "CallablePayload");
    case slang::ShaderRecord:
      return fmt::format_to(ctx.out(), "ShaderRecord");
    case slang::ExistentialTypeParam:
      return fmt::format_to(ctx.out(), "ExistentialTypeParam");
    case slang::ExistentialObjectParam:
      return fmt::format_to(ctx.out(), "ExistentialObjectParam");
    case slang::SubElementRegisterSpace:
      return fmt::format_to(ctx.out(), "SubElementRegisterSpace");
    case slang::InputAttachmentIndex:
      return fmt::format_to(ctx.out(), "InputAttachmentIndex");
    case slang::MetalArgumentBufferElement:
      return fmt::format_to(ctx.out(), "MetalArgumentBufferElement");
    case slang::MetalAttribute:
      return fmt::format_to(ctx.out(), "MetalAttribute");
    case slang::MetalPayload:
      return fmt::format_to(ctx.out(), "MetalPayload");
    }
  }
};

template <> struct fmt::formatter<slang::TypeReflection::ScalarType> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const slang::TypeReflection::ScalarType& scalarType, fmt::format_context& ctx) const {
    switch (scalarType) {
    case slang::TypeReflection::ScalarType::None:
      return fmt::format_to(ctx.out(), "None");
    case slang::TypeReflection::ScalarType::Bool:
      return fmt::format_to(ctx.out(), "Bool");
    case slang::TypeReflection::ScalarType::Int8:
      return fmt::format_to(ctx.out(), "Int8");
    case slang::TypeReflection::ScalarType::UInt8:
      return fmt::format_to(ctx.out(), "UInt8");
    case slang::TypeReflection::ScalarType::Int16:
      return fmt::format_to(ctx.out(), "Int16");
    case slang::TypeReflection::ScalarType::UInt16:
      return fmt::format_to(ctx.out(), "UInt16");
    case slang::TypeReflection::ScalarType::Int32:
      return fmt::format_to(ctx.out(), "Int32");
    case slang::TypeReflection::ScalarType::UInt32:
      return fmt::format_to(ctx.out(), "UInt32");
    case slang::TypeReflection::ScalarType::Int64:
      return fmt::format_to(ctx.out(), "Int64");
    case slang::TypeReflection::ScalarType::UInt64:
      return fmt::format_to(ctx.out(), "UInt64");
    case slang::TypeReflection::Void:
      return fmt::format_to(ctx.out(), "Void");
    case slang::TypeReflection::Float16:
      return fmt::format_to(ctx.out(), "Float16");
    case slang::TypeReflection::Float32:
      return fmt::format_to(ctx.out(), "Float32");
    case slang::TypeReflection::Float64:
      return fmt::format_to(ctx.out(), "Float64");
    case slang::TypeReflection::IntPtr:
      return fmt::format_to(ctx.out(), "IntPtr");
    case slang::TypeReflection::UIntPtr:
      return fmt::format_to(ctx.out(), "UIntPtr");
    case slang::TypeReflection::BFloat16:
      return fmt::format_to(ctx.out(), "BFloat16");
    case slang::TypeReflection::FloatE4M3:
      return fmt::format_to(ctx.out(), "FloatE4M3");
    case slang::TypeReflection::FloatE5M2:
      return fmt::format_to(ctx.out(), "FloatE5M2");
      break;
    }
  }
};

template <> struct fmt::formatter<SlangResourceShape> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const SlangResourceShape& shape, FormatContext& ctx) const {
    if (shape == SLANG_RESOURCE_NONE) {
      return fmt::format_to(ctx.out(), "None");
    }

    std::vector<std::string_view> shapeStrings;

    auto baseShape = shape & SLANG_RESOURCE_BASE_SHAPE_MASK;

    switch (baseShape) {
    case SLANG_TEXTURE_1D:
      shapeStrings.push_back("Texture1D");
      break;
    case SLANG_TEXTURE_2D:
      shapeStrings.push_back("Texture2D");
      break;
    case SLANG_TEXTURE_3D:
      shapeStrings.push_back("Texture3D");
      break;
    case SLANG_TEXTURE_CUBE:
      shapeStrings.push_back("TextureCube");
      break;
    case SLANG_TEXTURE_BUFFER:
      shapeStrings.push_back("TextureBuffer");
      break;
    case SLANG_STRUCTURED_BUFFER:
      shapeStrings.push_back("StructuredBuffer");
      break;
    case SLANG_BYTE_ADDRESS_BUFFER:
      shapeStrings.push_back("ByteAddressBuffer");
      break;
    case SLANG_RESOURCE_UNKNOWN:
      shapeStrings.push_back("Unknown");
      break;
    case SLANG_ACCELERATION_STRUCTURE:
      shapeStrings.push_back("AccelerationStructure");
      break;
    case SLANG_TEXTURE_SUBPASS:
      shapeStrings.push_back("TextureSubpass");
      break;
    default:
      break;
    }

    if ((shape & SLANG_TEXTURE_FEEDBACK_FLAG) != 0) {
      shapeStrings.push_back("TextureFeedback");
    }
    if ((shape & SLANG_TEXTURE_SHADOW_FLAG) != 0) {
      shapeStrings.push_back("TextureShadow");
    }
    if ((shape & SLANG_TEXTURE_ARRAY_FLAG) != 0) {
      shapeStrings.push_back("TextureArray");
    }
    if ((shape & SLANG_TEXTURE_MULTISAMPLE_FLAG) != 0) {
      shapeStrings.push_back("TextureMultisample");
    }
    if ((shape & SLANG_TEXTURE_COMBINED_FLAG) != 0) {
      shapeStrings.push_back("TextureCombined");
    }

    return fmt::format_to(ctx.out(), "{}", fmt::join(shapeStrings, " | "));
  }
};

template <> struct fmt::formatter<SlangResourceAccess> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const SlangResourceAccess& access, FormatContext& ctx) const {
    switch (access) {
    case SLANG_RESOURCE_ACCESS_NONE:
      return fmt::format_to(ctx.out(), "None");
    case SLANG_RESOURCE_ACCESS_READ:
      return fmt::format_to(ctx.out(), "Read");
    case SLANG_RESOURCE_ACCESS_WRITE:
      return fmt::format_to(ctx.out(), "Write");
    case SLANG_RESOURCE_ACCESS_READ_WRITE:
      return fmt::format_to(ctx.out(), "ReadWrite");
    }
  }
};

template <> struct fmt::formatter<SlangMatrixLayoutMode> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const SlangMatrixLayoutMode& mode, FormatContext& ctx) const {
    switch (mode) {
    case SLANG_MATRIX_LAYOUT_ROW_MAJOR:
      return fmt::format_to(ctx.out(), "RowMajor");
    case SLANG_MATRIX_LAYOUT_COLUMN_MAJOR:
      return fmt::format_to(ctx.out(), "ColumnMajor");
    }
  }
};

template <> struct fmt::formatter<SlangStage> : formatter<std::string_view> {
  template <typename FormatContext> auto format(const SlangStage& stage, FormatContext& ctx) const {
    switch (stage) {
    case SLANG_STAGE_NONE:
      return fmt::format_to(ctx.out(), "None");
    case SLANG_STAGE_VERTEX:
      return fmt::format_to(ctx.out(), "Vertex");
    case SLANG_STAGE_HULL:
      return fmt::format_to(ctx.out(), "Hull");
    case SLANG_STAGE_DOMAIN:
      return fmt::format_to(ctx.out(), "Domain");
    case SLANG_STAGE_GEOMETRY:
      return fmt::format_to(ctx.out(), "Geometry");
    case SLANG_STAGE_FRAGMENT:
      return fmt::format_to(ctx.out(), "Fragment");
    case SLANG_STAGE_COMPUTE:
      return fmt::format_to(ctx.out(), "Compute");
    }
  }
};