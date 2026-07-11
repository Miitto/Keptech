#include "printing.hpp"

#include <iostream>
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace kt::shader_processor::printing {
  inline std::shared_ptr<spdlog::logger> createLogger(const std::string& name, const spdlog::level::level_enum level) noexcept {
    auto logger = spdlog::stdout_color_mt(name);
    logger->set_level(level);
    logger->set_pattern("[%H:%M:%S.%e] [%n] [%^%L%$] %v");
    return logger;
  }
  const std::shared_ptr<spdlog::logger> logger = createLogger("KT Shaders", spdlog::level::info);
  const auto& o = std::cout; // NOLINT

#define L(...) logger->info(__VA_ARGS__) // NOLINT

  void print(slang::VariableReflection* var) {
    const char* name = var->getName();
    slang::TypeReflection* type = var->getType();

    L("Name: \"{}\"", name);
    print(type);
  }

  void print(slang::TypeReflection* type) {
    const char* name = type->getName();
    slang::TypeReflection::Kind kind = type->getKind();

    L("Type Name: \"{}\"", name);
    switch (kind) {
    case slang::TypeReflection::Kind::None:
      L("Kind: None");
      return;
    case slang::TypeReflection::Kind::Struct: {
      L("Kind: Struct");
      auto fieldCount = type->getFieldCount();
      for (size_t i = 0; i < fieldCount; ++i) {
        slang::VariableReflection* field = type->getFieldByIndex(i);
        print(field);
      }
      return;
    }
    case slang::TypeReflection::Kind::Array: {
      auto elemCount = type->getElementCount();
      L("Kind: Array[{}]", elemCount);
      auto elemType = type->getElementType();
      print(elemType);
      return;
    }
    case slang::TypeReflection::Kind::Matrix:
      L("Type Kind: Matrix");
      return;
    case slang::TypeReflection::Kind::Vector:
      L("Type Kind: Vector");
      return;
    case slang::TypeReflection::Kind::Scalar:
      L("Type Kind: Scalar");
      return;
    case slang::TypeReflection::Kind::ConstantBuffer:
      L("Type Kind: ConstantBuffer");
      return;
    case slang::TypeReflection::Kind::Resource:
      L("Type Kind: Resource");
      return;
    case slang::TypeReflection::Kind::SamplerState:
      L("Type Kind: SamplerState");
      return;
    case slang::TypeReflection::Kind::TextureBuffer:
      L("Type Kind: TextureBuffer");
      return;
    case slang::TypeReflection::Kind::ShaderStorageBuffer:
      L("Type Kind: ShaderStorageBuffer");
      return;
    case slang::TypeReflection::Kind::ParameterBlock:
      L("Type Kind: ParameterBlock");
      return;
    case slang::TypeReflection::Kind::GenericTypeParameter:
      L("Type Kind: GenericTypeParameter");
      return;
    case slang::TypeReflection::Kind::Interface:
      L("Type Kind: Interface");
      return;
    case slang::TypeReflection::Kind::OutputStream:
      L("Type Kind: OutputStream");
      return;
    case slang::TypeReflection::Kind::Specialized:
      L("Type Kind: Specialized");
      return;
    case slang::TypeReflection::Kind::Feedback:
      L("Type Kind: Feedback");
      return;
    case slang::TypeReflection::Kind::Pointer:
      L("Type Kind: Pointer");
      return;
    case slang::TypeReflection::Kind::DynamicResource:
      L("Type Kind: DynamicResource");
      return;
    case slang::TypeReflection::Kind::MeshOutput:
      L("Type Kind: MeshOutput");
      return;
    }
  }

} // namespace kt::shader_processor::printing
