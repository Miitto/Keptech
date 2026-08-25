#pragma once

#include <slang.h>
#include <spdlog/fmt/bundled/format.h>

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