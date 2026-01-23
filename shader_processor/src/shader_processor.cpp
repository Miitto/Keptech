#include "keptech/shader_processor/shader_processor.hpp"

#include <expected>
#include <iostream>
#include <slang-com-ptr.h>
#include <slang.h>
#include <utility>
#include <vector>

namespace {
  const char* slangStagetoString(SlangStage stage) {
    switch (stage) {
    case SLANG_STAGE_VERTEX:
      return "vertex";
    case SLANG_STAGE_FRAGMENT:
      return "fragment";
    case SLANG_STAGE_COMPUTE:
      return "compute";
    case SLANG_STAGE_NONE:
      return "none";
    case SLANG_STAGE_HULL:
      return "hull";
    case SLANG_STAGE_DOMAIN:
      return "domain";
    case SLANG_STAGE_GEOMETRY:
      return "geometry";
    case SLANG_STAGE_RAY_GENERATION:
      return "ray generation";
    case SLANG_STAGE_INTERSECTION:
      return "intersection";
    case SLANG_STAGE_ANY_HIT:
      return "any hit";
    case SLANG_STAGE_CLOSEST_HIT:
      return "closest hit";
    case SLANG_STAGE_MISS:
      return "miss";
    case SLANG_STAGE_CALLABLE:
      return "callable";
    case SLANG_STAGE_MESH:
      return "mesh";
    case SLANG_STAGE_AMPLIFICATION:
      return "amplification";
    case SLANG_STAGE_DISPATCH:
      return "dispatch";
    case SLANG_STAGE_COUNT:
      return "count";
    }
  }

  const keptech::shaders::ShaderStages
  slangStagetoKeptechStage(SlangStage stage) {
    switch (stage) {
    case SLANG_STAGE_VERTEX:
      return keptech::shaders::ShaderStages::Vertex;
    case SLANG_STAGE_FRAGMENT:
      return keptech::shaders::ShaderStages::Fragment;
    case SLANG_STAGE_COMPUTE:
      return keptech::shaders::ShaderStages::Compute;
    default:
      std::cerr << "Unsupported shader stage: " << slangStagetoString(stage)
                << '\n';
      throw std::runtime_error("Unsupported shader stage");
    }
  }
} // namespace

namespace keptech::shader_processor {
  Slang::ComPtr<slang::IGlobalSession> globalSession; // NOLINT
  SlangGlobalSessionDesc globalSessionDesc;           // NOLINT

  void init() {
    slang::createGlobalSession(&globalSessionDesc, globalSession.writeRef());
  }

  CompilerSession::CompilerSession(SessionConfig config) {
    slang::SessionDesc sessionDesc = {};

    std::vector<slang::CompilerOptionEntry> compilerOptionEntries;

    {
      auto name = slang::CompilerOptionName::Optimization;
      SlangOptimizationLevel level =
          SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_DEFAULT;
      switch (config.optimizationLevel) {
      case OptimizationLevel::None:
      case OptimizationLevel::Debug:
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

    std::vector<const char*> searchPaths{KEPTECH_SHADER_DIR};

    sessionDesc.compilerOptionEntries = compilerOptionEntries.data();
    sessionDesc.compilerOptionEntryCount =
        static_cast<uint32_t>(compilerOptionEntries.size());

    sessionDesc.searchPathCount = static_cast<uint32_t>(searchPaths.size());
    sessionDesc.searchPaths = searchPaths.data();

    slang::TargetDesc target;
    target.format = SLANG_SPIRV;
    target.profile = globalSession->findProfile("spirv_1_4");

    slang::CompilerOptionEntry emitDirectlyToBinaryEntry;
    emitDirectlyToBinaryEntry.name =
        slang::CompilerOptionName::EmitSpirvDirectly;
    emitDirectlyToBinaryEntry.value.intValue0 = true;

    target.compilerOptionEntries = &emitDirectlyToBinaryEntry;
    target.compilerOptionEntryCount = 1;

    sessionDesc.targets = &target;
    sessionDesc.targetCount = 1;

    globalSession->createSession(sessionDesc, session.writeRef());

    Slang::ComPtr<slang::IBlob> keptechModuleDiag;
    keptechModule =
        session->loadModule("keptech", keptechModuleDiag.writeRef());
    if (!keptechModule) {
      std::cerr << "Failed to load keptech shader module: "
                << (char*)keptechModuleDiag->getBufferPointer() // NOLINT
                << '\n';
    }
  }

  std::pair<slang::IModule*, Slang::ComPtr<slang::IBlob>>
  CompilerSession::loadModule(const char* moduleName, const std::string& source,
                              const char* path) {
    Slang::ComPtr<slang::IBlob> diagBlob;
    auto mod = session->loadModuleFromSourceString(
        moduleName, path, source.c_str(), diagBlob.writeRef());

    if (mod) {
      loadedModules.emplace_back(mod);
      return {mod, diagBlob};
    } else {
      return {mod, diagBlob};
    }
  }

  std::pair<Program, Slang::ComPtr<slang::IBlob>> CompilerSession::link() {
    std::vector<slang::IComponentType*> modules;
    modules.reserve(1 + loadedModules.size());
    modules.push_back(keptechModule.get());
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
    session->createCompositeComponentType(modules.data(),
                                          static_cast<uint32_t>(modules.size()),
                                          program.writeRef());

    Slang::ComPtr<slang::IBlob> diagBlob;
    Slang::ComPtr<slang::IComponentType> linkedProgram;
    program->link(linkedProgram.writeRef(), diagBlob.writeRef());

    return {linkedProgram, diagBlob};
  }

  Program::Program(Slang::ComPtr<slang::IComponentType> componentType)
      : program(std::move(componentType)) {}

  Program::Kernel Program::getCode() const {
    Slang::ComPtr<slang::IBlob> spirvBlob;
    Slang::ComPtr<slang::IBlob> diagBlob;

    program->getTargetCode(0, spirvBlob.writeRef(), diagBlob.writeRef());

    return {.spirv = std::move(spirvBlob), .diagnostics = std::move(diagBlob)};
  }

  std::expected<std::pair<keptech::shaders::Shader, Program::ShaderResources>,
                std::string>
  Program::toShader(const char* name) const {
    auto [kernel, diag] = getCode();
    if (diag) {
      std::cerr << (char*)diag->getBufferPointer() << '\n'; // NOLINT
    }
    if (!kernel) {
      return std::unexpected<std::string>(
          "Failed to get SPIR-V code for shader.");
    }

    keptech::shaders::Shader shader = {
        .name = name,
        .code = std::span<const uint8_t>(
            static_cast<const uint8_t*>(kernel->getBufferPointer()),
            kernel->getBufferSize()),
    };
    auto layout = program->getLayout();

    auto entryPointCount = layout->getEntryPointCount();
    std::vector<keptech::shaders::ShaderStage> stages;
    stages.reserve(entryPointCount);

    shader.mode = keptech::shaders::RenderingMode::Deferred;

    for (uint32_t i = 0; i < entryPointCount; ++i) {
      auto entryPoint = layout->getEntryPointByIndex(i);
      keptech::shaders::ShaderStages stage =
          slangStagetoKeptechStage(entryPoint->getStage());
      if (stage == shaders::ShaderStages::Fragment) {
        auto returnT = entryPoint->getFunction()->getReturnType();
        Slang::ComPtr<slang::IBlob> typeNameBlob;
        returnT->getFullName(typeNameBlob.writeRef());

        std::string_view returnTypeName(
            static_cast<const char*>(typeNameBlob->getBufferPointer()),
            typeNameBlob->getBufferSize());

        if (returnTypeName == "keptech.DeferredOutput") {
          std::cout << "Auto detecting deferred rendering mode for shader '"
                    << name << "'\n";
          shader.mode = keptech::shaders::RenderingMode::Deferred;
        } else if (returnTypeName == "vector<float,4>") {
          std::cout << "Auto detecting forward rendering mode for shader '"
                    << name << "'\n";
          shader.mode = keptech::shaders::RenderingMode::Forward;
        } else {
          std::cout << "Couldn't auto detect rendering mode for shader '"
                    << name << "'\n";
          shader.mode = keptech::shaders::RenderingMode::Custom;
        }
      }
      stages.push_back(keptech::shaders::ShaderStage{
          .name = entryPoint->getName(), .stage = stage});
    }

    shader.stages = std::span<const keptech::shaders::ShaderStage>(
        stages.data(), stages.size());

    return std::move(
        std::make_pair(shader, ShaderResources{.stages = std::move(stages),
                                               .code = std::move(kernel)}));
  }
} // namespace keptech::shader_processor
