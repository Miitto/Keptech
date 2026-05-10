#include "keptech/shader_processor/shader_processor.hpp"

#include "printing.hpp"
#include <array>
#include <expected>
#include <iostream>
#include <slang-com-ptr.h>
#include <slang.h>
#include <utility>
#include <vector>

using namespace kt::shader_processor::printing;

#include "conversions.hpp"

namespace kt::shader_processor {
  Slang::ComPtr<slang::IGlobalSession> globalSession; // NOLINT
  SlangGlobalSessionDesc globalSessionDesc;           // NOLINT

  void init() { slang::createGlobalSession(&globalSessionDesc, globalSession.writeRef()); }

  CompilerSession::CompilerSession(SessionConfig config) {
    slang::SessionDesc sessionDesc = {};

    std::vector<slang::CompilerOptionEntry> compilerOptionEntries;

    {
      auto name = slang::CompilerOptionName::Optimization;
      SlangOptimizationLevel level = SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_DEFAULT;
      switch (config.optimizationLevel) {
      case OptimizationLevel::None:
        level = SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_NONE;
        break;
      case OptimizationLevel::Basic:
        break;
      case OptimizationLevel::Aggressive:
        level = SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_HIGH;
        break;
      }

      slang::CompilerOptionValue value;
      value.intValue0 = level;
      compilerOptionEntries.push_back({.name = name, .value = value});
    }

    if (config.debugInfo) {
      compilerOptionEntries.push_back({.name = slang::CompilerOptionName::DebugInformation,
                                       .value = {.intValue0 = SlangDebugInfoLevel::SLANG_DEBUG_INFO_LEVEL_MAXIMAL}});
      compilerOptionEntries.push_back({.name = slang::CompilerOptionName::DebugInformationFormat,
                                       .value = {.intValue0 = SlangDebugInfoFormat::SLANG_DEBUG_INFO_FORMAT_DEFAULT}});
    }

    std::vector<const char*> searchPaths{KEPTECH_SHADER_DIR "/lib"};

    sessionDesc.compilerOptionEntries = compilerOptionEntries.data();
    sessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(compilerOptionEntries.size());

    sessionDesc.searchPathCount = static_cast<uint32_t>(searchPaths.size());
    sessionDesc.searchPaths = searchPaths.data();

    slang::TargetDesc target;
    target.format = SLANG_SPIRV;
    target.profile = globalSession->findProfile("spirv_1_6");

    std::array compilerOptions{
        slang::CompilerOptionEntry{
            .name = slang::CompilerOptionName::EmitSpirvDirectly,
            .value{.intValue0 = true},
        },
        slang::CompilerOptionEntry{
            .name = slang::CompilerOptionName::VulkanUseEntryPointName,
            .value{.intValue0 = true},
        },
    };

    target.compilerOptionEntryCount = compilerOptions.size();
    target.compilerOptionEntries = compilerOptions.data();

    sessionDesc.targets = &target;
    sessionDesc.targetCount = 1;

    globalSession->createSession(sessionDesc, session.writeRef());
  }

  std::pair<slang::IModule*, Slang::ComPtr<slang::IBlob>> CompilerSession::loadModule(const char* moduleName, const std::string& source,
                                                                                      const char* path) {
    Slang::ComPtr<slang::IBlob> diagBlob;
    auto mod = session->loadModuleFromSourceString(moduleName, path, source.c_str(), diagBlob.writeRef());

    if (mod) {
      loadedModules.emplace_back(mod);
    }
    return {mod, diagBlob};
  }

  std::pair<Program, Slang::ComPtr<slang::IBlob>> CompilerSession::link() {
    std::vector<slang::IComponentType*> modules;
    modules.reserve(loadedModules.size());
    for (const auto& mod : loadedModules) {
      modules.push_back(mod.get());
    }

    std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints;
    for (const auto& mod : loadedModules) {
      auto entryPointCount = mod->getDefinedEntryPointCount();

      for (int32_t i = 0; i < entryPointCount; ++i) {
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        mod->getDefinedEntryPoint(i, entryPoint.writeRef());
        entryPoints.push_back(entryPoint);
      }
    }
    modules.reserve(modules.size() + entryPoints.size());
    for (const auto& entryPoint : entryPoints) {
      modules.push_back(entryPoint.get());
    }

    Slang::ComPtr<slang::IComponentType> program;
    session->createCompositeComponentType(modules.data(), static_cast<uint32_t>(modules.size()), program.writeRef());

    Slang::ComPtr<slang::IBlob> diagBlob;
    Slang::ComPtr<slang::IComponentType> linkedProgram;
    program->link(linkedProgram.writeRef(), diagBlob.writeRef());

    return {linkedProgram, diagBlob};
  }

  Program::Program(Slang::ComPtr<slang::IComponentType> componentType) : program(std::move(componentType)) {}

  Program::Kernel Program::getCode() const {
    Slang::ComPtr<slang::IBlob> spirvBlob;
    Slang::ComPtr<slang::IBlob> diagBlob;

    program->getTargetCode(0, spirvBlob.writeRef(), diagBlob.writeRef());

    return {.spirv = std::move(spirvBlob), .diagnostics = std::move(diagBlob)};
  }

  std::expected<kt::shaders::Shader, std::string> Program::toShader(const char* name) const {
    auto [kernel, diag] = getCode();
    if (diag) {
      std::cerr << (char*)diag->getBufferPointer() << '\n'; // NOLINT
    }
    if (!kernel) {
      return std::unexpected<std::string>("Failed to get SPIR-V code for shader.");
    }

    std::vector<uint8_t> code(kernel->getBufferSize());
    memcpy(code.data(), kernel->getBufferPointer(), kernel->getBufferSize());

    kt::shaders::Shader shader = {
        .name = name,
        .code = std::move(code),
    };
    auto layout = program->getLayout();

    auto entryPointCount = layout->getEntryPointCount();

    shader.stages.reserve(entryPointCount);

    std::vector<std::vector<kt::shaders::DataType>> vertexLayout;

    shader.mode = kt::shaders::RenderingMode::Custom;

    std::clog << "Shader '" << name << "' has " << entryPointCount << " entry point(s)\n";
    for (uint32_t i = 0; i < entryPointCount; ++i) {
      auto entryPoint = layout->getEntryPointByIndex(i);
      kt::shaders::ShaderStages stage = slangStagetoKeptechStage(entryPoint->getStage());
      switch (stage) {
      case kt::shaders::ShaderStages::Vertex: {
        auto paramCount = entryPoint->getParameterCount();
        size_t userParamCount = 0;
        for (auto i = 0; i < paramCount; ++i) {
          auto param = entryPoint->getParameterByIndex(i);
          auto category = param->getCategory();
          if (category != slang::ParameterCategory::VaryingInput && category != slang::ParameterCategory::Mixed)
            continue; // Some sort of builtin, such as vertex ID

          ++userParamCount;
          switch (param->getType()->getKind()) {
          case slang::TypeReflection::Kind::Scalar:
          case slang::TypeReflection::Kind::Vector: {
            auto type = slangTypeToKeptechTypes(param->getType())[0];
            std::clog << "  Parameter '" << param->getName() << "' mapped to vertex attribute of type " << fmt::format("{}", type)
                      << " at binding " << vertexLayout.size() << "\n";
            vertexLayout.push_back({type});
          } break;
          case slang::TypeReflection::Kind::Struct: {
            auto types = slangTypeToKeptechTypes(param->getType());
            std::clog << "  Parameter '" << param->getName() << "' mapped to " << types.size() << " vertex attribute(s) at binding "
                      << vertexLayout.size() << "\n";
            for (size_t i = 0; i < types.size(); ++i) {
              auto slangT = param->getType()->getFieldByIndex(i);
              auto str = fmt::format("    {} - {}\n", slangT->getName(), types[i]);
              std::clog << str;
            }
            vertexLayout.emplace_back(std::move(types));
          } break;
          default:
            std::cerr << "Unsupported parameter type for vertex shader input: " << fmt::format("{}", param->getType()->getKind()) << '\n';
          }
        }
        std::clog << "Processing vertex shader entry point '" << entryPoint->getName() << "' with " << userParamCount
                  << " user parameters\n";

        std::clog.flush();
        break;
      }
      case kt::shaders::ShaderStages::Fragment: {
        auto returnT = entryPoint->getFunction()->getReturnType();
        Slang::ComPtr<slang::IBlob> typeNameBlob;
        returnT->getFullName(typeNameBlob.writeRef());

        std::string_view returnTypeName(static_cast<const char*>(typeNameBlob->getBufferPointer()), typeNameBlob->getBufferSize());

        if (returnTypeName == "kt.DeferredOutput") {
          std::cout << "Auto detecting deferred rendering mode for shader '" << name << "'\n";
          shader.mode = kt::shaders::RenderingMode::Deferred;
        } else if (returnTypeName == "kt.DeferredLightingOutput") {
          std::cout << "Auto detecting deferred lighting rendering mode for shader '" << name << "'\n";
          shader.mode = kt::shaders::RenderingMode::DeferredLighting;
        } else if (returnTypeName == "vector<float,4>") {
          std::cout << "Auto detecting forward rendering mode for shader '" << name << "'\n";
          shader.mode = kt::shaders::RenderingMode::Forward;
        } else {
          std::cout << "Couldn't auto detect rendering mode for shader '" << name << "'\n";
          shader.mode = kt::shaders::RenderingMode::Custom;
        }
      } break;
      }
      shader.stages.push_back(kt::shaders::ShaderStage{.name = entryPoint->getName(), .stage = stage});
    }

    shader.vertexLayout.reserve(vertexLayout.size());
    for (auto& layoutEntry : vertexLayout) {
      shader.vertexLayout.emplace_back(std::move(layoutEntry));
    }

    return std::move(shader);
  }
} // namespace kt::shader_processor
