#pragma once

#include <slang.h>
#include <spdlog/logger.h>

namespace keptech::shader_processor::printing {
  extern const std::shared_ptr<spdlog::logger> logger;

  void print(slang::VariableReflection* var);
  void print(slang::TypeReflection* type);
  void print(slang::TypeReflection::Kind kind);
} // namespace keptech::shader_processor::printing

#define N(_NAME)                                                               \
  case E::_NAME:                                                               \
    name = #_NAME;                                                             \
    break;

template <>
struct fmt::formatter<slang::ParameterCategory>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const slang::ParameterCategory& category,
              FormatContext& ctx) const {
    using E = slang::ParameterCategory;
    std::string_view name;
    switch (category) {
      N(None)
      N(Mixed)
      N(ConstantBuffer)
      N(ShaderResource)
      N(UnorderedAccess)
      N(VaryingInput)
      N(VaryingOutput)
      N(SamplerState)
      N(Uniform)
      N(DescriptorTableSlot)
      N(SpecializationConstant)
      N(PushConstantBuffer)
      N(RegisterSpace)
      N(GenericResource)
      N(RayPayload)
      N(HitAttributes)
      N(CallablePayload)
      N(ShaderRecord)
      N(ExistentialTypeParam)
      N(ExistentialObjectParam)
      N(SubElementRegisterSpace)
      N(InputAttachmentIndex)
      N(MetalArgumentBufferElement)
      N(MetalAttribute)
      N(MetalPayload)
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};
