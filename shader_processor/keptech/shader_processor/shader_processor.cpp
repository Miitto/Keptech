#include "shader_processor.hpp"

#include <array>
#include <expected>
#include <slang-com-ptr.h>
#include <slang.h>
#include <string>
#include <utility>
#include <vector>

#ifdef KT_DX12
#include <d3dcompiler.h>
#endif

#include "conversions.hpp"

namespace kt::shader_processor {
  std::string toString(SlangResult res);

  Slang::ComPtr<slang::IGlobalSession> globalSession; // NOLINT
  SlangGlobalSessionDesc globalSessionDesc;           // NOLINT

  void init() { slang::createGlobalSession(&globalSessionDesc, globalSession.writeRef()); }

  CompilerSession::CompilerSession(SessionConfig config) {
    slang::SessionDesc sessionDesc = {};

    std::vector<slang::CompilerOptionEntry> compilerOptionEntries{
#ifndef KT_USE_DESCRIPTOR_HEAP
        slang::CompilerOptionEntry{
            .name = slang::CompilerOptionName::BindlessSpaceIndex,
            .value{.intValue0 = 0},
        },
#endif
    };

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
      value.intValue0 = static_cast<int32_t>(level);
      compilerOptionEntries.push_back({.name = name, .value = value});
    }

    if (config.debugInfo) {
      compilerOptionEntries.push_back({
          .name = slang::CompilerOptionName::DebugInformation,
          .value = {.intValue0 = SlangDebugInfoLevel::SLANG_DEBUG_INFO_LEVEL_MAXIMAL},
      });
      compilerOptionEntries.push_back({
          .name = slang::CompilerOptionName::DebugInformationFormat,
          .value = {.intValue0 = SlangDebugInfoFormat::SLANG_DEBUG_INFO_FORMAT_DEFAULT},
      });
    }

    std::vector<const char*> searchPaths{KEPTECH_SHADER_DIR "/lib"};

    sessionDesc.compilerOptionEntries = compilerOptionEntries.data();
    sessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(compilerOptionEntries.size());

    sessionDesc.searchPathCount = static_cast<uint32_t>(searchPaths.size());
    sessionDesc.searchPaths = searchPaths.data();

    slang::TargetDesc target;

#ifdef KT_VULKAN
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
#ifdef USE_COLOR_OUTPUT
        slang::CompilerOptionEntry{
            .name = slang::CompilerOptionName::DiagnosticColor,
            .value{
                .intValue0 = SlangDiagnosticColor::SLANG_DIAGNOSTIC_COLOR_ALWAYS,
            },
        },
        slang::CompilerOptionEntry{
            .name = slang::CompilerOptionName::EnableRichDiagnostics,
            .value = {.intValue0 = true},
        },
#endif
#ifdef KT_USE_DESCRIPTOR_HEAP
        slang::CompilerOptionEntry{
            .name = slang::CompilerOptionName::Capability,
            .value{.kind = slang::CompilerOptionValueKind::String, .stringValue0 = "spvDescriptorHeapEXT"},
        },
#endif
    };
#elif defined(KT_DX12)
    target.format = SLANG_DXIL;
    target.profile = globalSession->findProfile("sm_6_8");
    std::array compilerOptions{
        slang::CompilerOptionEntry{
            .name = slang::CompilerOptionName::DiagnosticColor,
            .value{
                .intValue0 = SlangDiagnosticColor::SLANG_DIAGNOSTIC_COLOR_ALWAYS,
            },
        },
        slang::CompilerOptionEntry{
            .name = slang::CompilerOptionName::EnableRichDiagnostics,
            .value = {.intValue0 = true},
        },
    };
#else
#error "Unsupported graphics API"
#endif

    target.compilerOptionEntryCount = compilerOptions.size();
    target.compilerOptionEntries = compilerOptions.data();

    sessionDesc.targets = &target;
    sessionDesc.targetCount = 1;

    auto res = globalSession->createSession(sessionDesc, session.writeRef());
    if (res != SLANG_OK) {
      throw std::runtime_error("Failed to create session: " + toString(res));
    }
  }

  Return<slang::IModule*> CompilerSession::loadModule(const char* moduleName, const std::string& source, const char* path) {
    Slang::ComPtr<slang::IBlob> diagBlob;
    auto mod = session->loadModuleFromSourceString(moduleName, path, source.c_str(), diagBlob.writeRef());

    if (mod) {
      loadedModules.emplace_back(mod);
    } else {
      std::string errorMsg =
          diagBlob ? std::string(static_cast<const char*>(diagBlob->getBufferPointer()), diagBlob->getBufferSize()) : "Unknown error";
      throw std::runtime_error("Failed to load module: " + errorMsg);
    }
    return {.value = mod, .diagnostics = diagBlob};
  }

  Return<Program> CompilerSession::link() {
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
        auto res = mod->getDefinedEntryPoint(i, entryPoint.writeRef());
        if (res != SLANG_OK) {
          throw std::runtime_error("Failed to get entry point from module: " + toString(res));
        }
        entryPoints.push_back(entryPoint);
      }
    }

    if (entryPoints.empty()) {
      throw std::runtime_error("No entry points found in loaded modules");
    }

    modules.reserve(modules.size() + entryPoints.size());
    for (const auto& entryPoint : entryPoints) {
      modules.push_back(entryPoint.get());
    }

    Slang::ComPtr<slang::IComponentType> program;
    {
      auto res = session->createCompositeComponentType(modules.data(), static_cast<uint32_t>(modules.size()), program.writeRef());
      if (res != SLANG_OK) {
        throw std::runtime_error("Failed to create composite component type: " + toString(res));
      }
    }

    Slang::ComPtr<slang::IBlob> diagBlob;
    Slang::ComPtr<slang::IComponentType> linkedProgram;
    auto res = program->link(linkedProgram.writeRef(), diagBlob.writeRef());
    if (res != SLANG_OK) {
      throw std::runtime_error("Failed to link program: " + toString(res));
    }

    return {.value = {linkedProgram, entryPoints.size()}, .diagnostics = diagBlob};
  }

  Program::Program(Slang::ComPtr<slang::IComponentType> componentType, size_t entryPointCount)
      : program(std::move(componentType)), entryPointCount(entryPointCount) {}

  std::vector<Program::Kernel> Program::getCode() const {

    std::vector<Kernel> kernels;

#ifdef KT_VULKAN
    kernels.resize(1);
    auto res = program->getTargetCode(0, kernels[0].code.writeRef(), kernels[0].diagnostics.writeRef());
    if (res != SLANG_OK) {
      throw std::runtime_error("Failed to get target code: " + toString(res));
    }
#elif defined(KT_DX12)
    kernels.resize(entryPointCount);
    for (uint32_t i = 0; i < kernels.size(); ++i) {
      auto res = program->getEntryPointCode(i, 0, kernels[i].code.writeRef(), kernels[i].diagnostics.writeRef());
      if (res != SLANG_OK) {
        std::string errorMsg = kernels[i].diagnostics ? std::string(static_cast<const char*>(kernels[i].diagnostics->getBufferPointer()),
                                                                    kernels[i].diagnostics->getBufferSize())
                                                      : "Unknown error";
        throw std::runtime_error("Failed to get target code: " + toString(res) + "\n" + errorMsg);
      }
    }
#endif

    return kernels;
  }

  std::expected<Return<kt::shaders::Shader>, std::string> Program::toShader(const char* name) const {
    std::vector<std::vector<uint8_t>> code;
    auto diag = Slang::ComPtr<slang::IBlob>{};
    try {
      auto kernels = getCode();
      if (kernels.empty()) {
        std::string errorMsg =
            diag ? std::string(static_cast<const char*>(diag->getBufferPointer()), diag->getBufferSize()) : "Unknown error";
        return std::unexpected<std::string>("Failed to get code for shader: " + errorMsg);
      }

      code.resize(kernels.size());

      for (size_t i = 0; i < kernels.size(); ++i) {
        auto& kernel = kernels[i];
        code[i].resize(kernel.code->getBufferSize());
        memcpy(code[i].data(), kernel.code->getBufferPointer(), kernel.code->getBufferSize());
      }
    } catch (const std::exception& e) {
      return std::unexpected<std::string>("Failed to get code for shader: " + std::string(e.what()));
    }

    kt::shaders::Shader shader = {
        .name = name,
        .code =
#ifdef KT_VULKAN
            std::move(code[0]),
#elif defined(KT_DX12)
            std::move(code),
#endif
        .mode = kt::shaders::RenderingMode::Custom,
        .stages = {},
        .vertexLayout = {},
    };
    auto layout = program->getLayout();

    auto entryPointCount = layout->getEntryPointCount();

    shader.stages.reserve(entryPointCount);

    std::vector<std::vector<kt::shaders::DataType>> vertexLayout;

    shader.mode = kt::shaders::RenderingMode::Custom;

    for (uint32_t i = 0; i < entryPointCount; ++i) {
      auto entryPoint = layout->getEntryPointByIndex(i);
      kt::shaders::ShaderStages stage = slangStagetoKeptechStage(entryPoint->getStage());
      switch (stage) {
      case kt::shaders::ShaderStages::Vertex: {
        auto paramCount = entryPoint->getParameterCount();
        for (auto j = 0u; j < paramCount; ++j) {
          auto param = entryPoint->getParameterByIndex(j);
          auto category = param->getCategory();
          if (category != slang::ParameterCategory::VaryingInput && category != slang::ParameterCategory::Mixed)
            continue; // Some sort of builtin, such as vertex ID

          switch (param->getType()->getKind()) {
          case slang::TypeReflection::Kind::Scalar:
          case slang::TypeReflection::Kind::Vector: {
            auto type = slangTypeToKeptechTypes(param->getType())[0];
            vertexLayout.push_back({type});
          } break;
          case slang::TypeReflection::Kind::Struct: {
            auto types = slangTypeToKeptechTypes(param->getType());
            vertexLayout.emplace_back(std::move(types));
          } break;
          default:
          }
        }
        break;
      }
      case kt::shaders::ShaderStages::Fragment: {
        auto returnT = entryPoint->getFunction()->getReturnType();
        Slang::ComPtr<slang::IBlob> typeNameBlob;
        returnT->getFullName(typeNameBlob.writeRef());

        std::string_view returnTypeName(static_cast<const char*>(typeNameBlob->getBufferPointer()), typeNameBlob->getBufferSize());

        if (returnTypeName == "kt.DeferredOutput") {
          shader.mode = kt::shaders::RenderingMode::Deferred;
        } else if (returnTypeName == "kt.DeferredLightingOutput") {
          shader.mode = kt::shaders::RenderingMode::DeferredLighting;
        } else if (returnTypeName == "vector<float,4>") {
          shader.mode = kt::shaders::RenderingMode::Forward;
        } else {
          shader.mode = kt::shaders::RenderingMode::Custom;
        }
      } break;
      default:
        break;
      }
      shader.stages.push_back(kt::shaders::ShaderStage{.name = entryPoint->getName(), .stage = stage});
    }

    shader.vertexLayout.reserve(vertexLayout.size());
    for (auto& layoutEntry : vertexLayout) {
      shader.vertexLayout.emplace_back(std::move(layoutEntry));
    }

    return Return<kt::shaders::Shader>{.value = std::move(shader), .diagnostics = std::move(diag)};
  }

  std::string toString(SlangResult res) {
    switch (res) {
    case SLANG_OK:
      return "SLANG_OK";
    case SLANG_E_NOT_IMPLEMENTED:
      return "SLANG_E_NOT_IMPLEMENTED";
    case SLANG_E_OUT_OF_MEMORY:
      return "SLANG_E_OUT_OF_MEMORY";
    case SLANG_E_INVALID_ARG:
      return "SLANG_E_INVALID_ARG";
    case SLANG_E_NOT_FOUND:
      return "SLANG_E_NOT_FOUND";
    default:
      return "Unknown SlangResult";
    }
  }
} // namespace kt::shader_processor
