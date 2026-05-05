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

  const kt::shaders::ShaderStages slangStagetoKeptechStage(SlangStage stage) {
    switch (stage) {
    case SLANG_STAGE_VERTEX:
      return kt::shaders::ShaderStages::Vertex;
    case SLANG_STAGE_FRAGMENT:
      return kt::shaders::ShaderStages::Fragment;
    case SLANG_STAGE_GEOMETRY:
      return kt::shaders::ShaderStages::Geometry;
    case SLANG_STAGE_COMPUTE:
      return kt::shaders::ShaderStages::Compute;
    default:
      std::cerr << "Unsupported shader stage: " << slangStagetoString(stage) << '\n';
      throw std::runtime_error("Unsupported shader stage");
    }
  }

  const std::vector<kt::shaders::DataType> slangTypeToKeptechTypes(slang::TypeReflection* type) {
    using namespace kt::shaders;

    switch (type->getKind()) {
    case slang::TypeReflection::Kind::Scalar: {
      auto scalarType = type->getScalarType();
      switch (scalarType) {
      case slang::TypeReflection::None:
        return {DataType::None};
      case slang::TypeReflection::Void:
        return {DataType::Void};
      case slang::TypeReflection::Bool:
        return {DataType::Bool};
      case slang::TypeReflection::Int32:
        return {DataType::I32};
      case slang::TypeReflection::UInt32:
        return {DataType::U32};
      case slang::TypeReflection::Int64:
        return {DataType::I64};
      case slang::TypeReflection::UInt64:
        return {DataType::U64};
      case slang::TypeReflection::Float16:
        return {DataType::F16};
      case slang::TypeReflection::Float32:
        return {DataType::F32};
      case slang::TypeReflection::Float64:
        return {DataType::F64};
      case slang::TypeReflection::Int8:
        return {DataType::I8};
      case slang::TypeReflection::UInt8:
        return {DataType::U8};
      case slang::TypeReflection::Int16:
        return {DataType::I16};
      case slang::TypeReflection::UInt16:
        return {DataType::U16};
      }
    } break;
    case slang::TypeReflection::Kind::Vector: {
      auto elemType = type->getElementType()->getScalarType();
      auto elemCount = type->getElementCount();
      switch (elemCount) {
      case 2:
        switch (elemType) {
        case slang::TypeReflection::None:
        case slang::TypeReflection::Void:
        case slang::TypeReflection::Bool:
          logger->error("Unsupported type in vector for resource type extraction");
          abort();
        case slang::TypeReflection::Int32:
          return {DataType::I32_2};
        case slang::TypeReflection::UInt32:
          return {DataType::U32_2};
        case slang::TypeReflection::Int64:
          return {DataType::I64_2};
        case slang::TypeReflection::UInt64:
          return {DataType::U64_2};
        case slang::TypeReflection::Float16:
          return {DataType::F16_2};
        case slang::TypeReflection::Float32:
          return {DataType::F32_2};
        case slang::TypeReflection::Float64:
          return {DataType::F64_2};
        case slang::TypeReflection::Int8:
          return {DataType::I8_2};
        case slang::TypeReflection::UInt8:
          return {DataType::U8_2};
        case slang::TypeReflection::Int16:
          return {DataType::I16_2};
        case slang::TypeReflection::UInt16:
          return {DataType::U16_2};
        }
      case 3:
        switch (elemType) {
        case slang::TypeReflection::None:
        case slang::TypeReflection::Void:
        case slang::TypeReflection::Bool:
          logger->error("Unsupported type in vector for resource type extraction");
          abort();
        case slang::TypeReflection::Int32:
          return {DataType::I32_3};
        case slang::TypeReflection::UInt32:
          return {DataType::U32_3};
        case slang::TypeReflection::Int64:
          return {DataType::I64_3};
        case slang::TypeReflection::UInt64:
          return {DataType::U64_3};
        case slang::TypeReflection::Float16:
          return {DataType::F16_3};
        case slang::TypeReflection::Float32:
          return {DataType::F32_3};
        case slang::TypeReflection::Float64:
          return {DataType::F64_3};
        case slang::TypeReflection::Int8:
          return {DataType::I8_3};
        case slang::TypeReflection::UInt8:
          return {DataType::U8_3};
        case slang::TypeReflection::Int16:
          return {DataType::I16_3};
        case slang::TypeReflection::UInt16:
          return {DataType::U16_3};
        }

      case 4:
        switch (elemType) {
        case slang::TypeReflection::None:
        case slang::TypeReflection::Void:
        case slang::TypeReflection::Bool:
          logger->error("Unsupported type in vector for resource type extraction");
          abort();
        case slang::TypeReflection::Int32:
          return {DataType::I32_4};
        case slang::TypeReflection::UInt32:
          return {DataType::U32_4};
        case slang::TypeReflection::Int64:
          return {DataType::I64_4};
        case slang::TypeReflection::UInt64:
          return {DataType::U64_4};
        case slang::TypeReflection::Float16:
          return {DataType::F16_4};
        case slang::TypeReflection::Float32:
          return {DataType::F32_4};
        case slang::TypeReflection::Float64:
          return {DataType::F64_4};
        case slang::TypeReflection::Int8:
          return {DataType::I8_4};
        case slang::TypeReflection::UInt8:
          return {DataType::U8_4};
        case slang::TypeReflection::Int16:
          return {DataType::I16_4};
        case slang::TypeReflection::UInt16:
          return {DataType::U16_4};
        }

      default:
        std::unreachable();
      }
    } break;
    case slang::TypeReflection::Kind::Array: {
      auto typeElem = type->getElementType();
      auto types = slangTypeToKeptechTypes(typeElem);
      auto elemCount = type->getElementCount();
      std::vector<DataType> arrayTypes;
      arrayTypes.reserve(types.size() * elemCount);
      for (size_t i = 0; i < elemCount; ++i) {
        arrayTypes.insert(arrayTypes.end(), types.begin(), types.end());
      }
      return arrayTypes;
    }
    case slang::TypeReflection::Kind::Struct: {
      auto fieldCount = type->getFieldCount();
      std::vector<DataType> fieldTypes{};
      for (size_t i = 0; i < fieldCount; ++i) {
        slang::VariableReflection* field = type->getFieldByIndex(i);
        auto fieldType = field->getType();
        auto t = slangTypeToKeptechTypes(fieldType);
        fieldTypes.insert(fieldTypes.end(), t.begin(), t.end());
      }
      return fieldTypes;
    }
    case slang::TypeReflection::Kind::Matrix: {
      auto elemType = type->getElementType()->getScalarType();
      auto rowCount = type->getRowCount();
      auto colCount = type->getColumnCount();

      if (rowCount != 4 || colCount != 4) {
        logger->error("Unsupported matrix size in type for resource type extraction");
        abort();
      }
      if (elemType != slang::TypeReflection::Float32) {
        logger->error("Unsupported matrix element type for resource type extraction");
        abort();
      }

      return {DataType::F32_4x4};
    }
    }

    logger->error("Unsupported type kind for resource type extraction");
    abort();
  }
} // namespace

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
          auto types = slangTypeToKeptechTypes(param->getType());
          std::clog << "  Parameter '" << param->getName() << "' mapped to " << types.size() << " vertex attribute(s)\n";
          for (size_t i = 0; i < types.size(); ++i) {
            auto slangT = param->getType()->getFieldByIndex(i);
            auto str = fmt::format("    {} - {}\n", slangT->getName(), types[i]);
            std::clog << str;
          }
          vertexLayout.emplace_back(std::move(types));
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
      case shaders::ShaderStages::Compute:
        break;
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
