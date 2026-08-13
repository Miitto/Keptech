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