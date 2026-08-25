#include "shader_processor.hpp"

#include "paramProcessing.hpp"
#include "shader-logger.hpp"
#include <array>
#include <expected>
#include <iostream>
#include <slang-com-ptr.h>
#include <slang.h>
#include <spdlog/fmt/bundled/format.h>
#include <string>
#include <utility>
#include <vector>

#ifdef KT_DX12
#include <d3dcompiler.h>
#endif

#include "conversions.hpp"

#ifdef max
#undef max
#endif

namespace kt::shader_processor {
  Slang::ComPtr<slang::IGlobalSession> globalSession; // NOLINT
  SlangGlobalSessionDesc globalSessionDesc;           // NOLINT

  void init() { slang::createGlobalSession(&globalSessionDesc, globalSession.writeRef()); }

  CompilerSession::CompilerSession(SessionConfig config) {
    slang::SessionDesc sessionDesc = {};

    std::vector<slang::CompilerOptionEntry> compilerOptionEntries{};

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
      throw std::runtime_error(fmt::format("Failed to create session: {}", res));
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
          throw std::runtime_error(fmt::format("Failed to get entry point from module: {}", res));
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
        throw std::runtime_error(fmt::format("Failed to create composite component type: {}", res));
      }
    }

    Slang::ComPtr<slang::IBlob> diagBlob;
    Slang::ComPtr<slang::IComponentType> linkedProgram;
    auto res = program->link(linkedProgram.writeRef(), diagBlob.writeRef());
    if (res != SLANG_OK) {
      throw std::runtime_error(fmt::format("Failed to link program: {}", res));
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
      throw std::runtime_error(fmt::format("Failed to get target code: {}", res));
    }
#elif defined(KT_DX12)
    kernels.resize(entryPointCount);
    for (uint32_t i = 0; i < kernels.size(); ++i) {
      auto res = program->getEntryPointCode(i, 0, kernels[i].code.writeRef(), kernels[i].diagnostics.writeRef());
      if (res != SLANG_OK) {
        std::string errorMsg = kernels[i].diagnostics ? std::string(static_cast<const char*>(kernels[i].diagnostics->getBufferPointer()),
                                                                    kernels[i].diagnostics->getBufferSize())
                                                      : "Unknown error";
        throw std::runtime_error(fmt::format("Failed to get target code: {}\n{}", res, errorMsg));
      }
    }
#endif

    return kernels;
  }

  namespace {
    std::expected<void, std::string> parseVertexAttribs(shaders::Vertex& v, slang::FunctionReflection& func) {
      auto attribCount = func.getUserAttributeCount();
      for (uint32_t idx = 0; idx < attribCount; ++idx) {
        auto attribute = func.getUserAttributeByIndex(idx);

        auto attributeName = attribute->getName();
        if (strcmp(attributeName, "topology") == 0) {
          auto argCount = attribute->getArgumentCount();
          if (argCount != 1) {
            return std::unexpected<std::string>("Invalid number of arguments for topology attribute");
          }
          auto argType = attribute->getArgumentType(0);
          std::clog << fmt::format("Argument type: {}", argType->getKind());
          int value = 0;
          attribute->getArgumentValueInt(0, &value);
          switch (value) {
          case 0:
            v.topology = kt::shaders::PrimitiveTopology::TriangleList;
            break;
          case 1:
            v.topology = kt::shaders::PrimitiveTopology::TriangleStrip;
            break;
          default:
            return std::unexpected<std::string>("Invalid value for topology attribute");
          }
        } else if (strcmp(attributeName, "cull") == 0) {
          auto argCount = attribute->getArgumentCount();
          if (argCount != 1) {
            return std::unexpected<std::string>("Invalid number of arguments for cull attribute");
          }
          auto argType = attribute->getArgumentType(0);
          std::clog << fmt::format("Argument type: {}", argType->getKind());
          int value = 0;
          attribute->getArgumentValueInt(0, &value);
          switch (value) {
          case 0:
            v.cullMode = kt::shaders::CullMode::None;
            break;
          case 1:
            v.cullMode = kt::shaders::CullMode::Front;
            break;
          case 2:
            v.cullMode = kt::shaders::CullMode::Back;
            break;
          case 3:
            v.cullMode = kt::shaders::CullMode::FrontAndBack;
            break;
          default:
            return std::unexpected<std::string>("Invalid value for cull attribute");
          }
        }
      }
      return {};
    }

    std::expected<void, std::string> parseFragmentAttribs(shaders::Fragment& f, slang::FunctionReflection& func) {
      auto attribCount = func.getUserAttributeCount();
      for (uint32_t idx = 0; idx < attribCount; ++idx) {
        auto attribute = func.getUserAttributeByIndex(idx);

        auto attributeName = attribute->getName();
        if (strcmp(attributeName, "blend") == 0) {
          auto argCount = attribute->getArgumentCount();
          if (argCount != 2) {
            return std::unexpected<std::string>("Invalid number of arguments for blend attribute");
          }
          f.enableBlending = true;
          int value = 0;
          attribute->getArgumentValueInt(0, &value);
          f.srcColorBlendFactor = static_cast<shaders::BlendFactor>(value);
          attribute->getArgumentValueInt(1, &value);
          f.dstColorBlendFactor = static_cast<shaders::BlendFactor>(value);
        } else if (strcmp(attributeName, "depthWrite") == 0) {
          auto argCount = attribute->getArgumentCount();
          if (argCount != 1) {
            return std::unexpected<std::string>("Invalid number of arguments for depthWrite attribute");
          }
          int value = 0;
          attribute->getArgumentValueInt(0, &value);
          f.depthWrite = value != 0;
        }
      }
      return {};
    }
  } // namespace

  std::expected<Return<kt::shaders::Shader>, std::string> Program::toShader(const char* name, const char* file) const {
    SHDR_DEBUG("Creating shader from program: {}", name);
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
        .file = file,
        .code =
#ifdef KT_VULKAN
            std::move(code[0]),
#elif defined(KT_DX12)
            std::move(code),
#endif
        .stages = {},
        .info =
            {
                .name = name,
            },
    };
    auto layout = program->getLayout();

    {
      auto globalVarLayout = layout->getGlobalParamsVarLayout();
      auto setsRes = parseParameter(*globalVarLayout);
      if (!setsRes) {
        return std::unexpected<std::string>("Failed to parse global parameters: " + setsRes.error());
      }
      shader.info.resources = std::move(setsRes.value());
    }

    shader.stages.reserve(entryPointCount);

    std::vector<shaders::VertexBuffer> vertexLayout;

    for (uint32_t i = 0; i < entryPointCount; ++i) {
      auto entryPoint = layout->getEntryPointByIndex(i);
      kt::shaders::ShaderStages stage = slangStagetoKeptechStage(entryPoint->getStage());
      switch (stage) {
      case kt::shaders::ShaderStages::Vertex: {
        size_t pushConstantSize = 0;
        auto paramCount = entryPoint->getParameterCount();
        for (auto j = 0u; j < paramCount; ++j) {
          auto param = entryPoint->getParameterByIndex(j);
          auto category = param->getCategory();

          if (category == slang::ParameterCategory::Uniform) {
            pushConstantSize += param->getTypeLayout()->getSize();
            continue;
          }

          if (category != slang::ParameterCategory::VaryingInput && category != slang::ParameterCategory::Mixed)
            continue; // Some sort of builtin, such as vertex ID

          switch (param->getType()->getKind()) {
          case slang::TypeReflection::Kind::Scalar:
          case slang::TypeReflection::Kind::Vector: {
            auto semantic = param->getSemanticName();
            auto semanticIndex = param->getSemanticIndex();
            auto type = slangTypeToKeptechTypes(param->getType())[0];
            vertexLayout.push_back(shaders::VertexBuffer{
                .layout = {shaders::VertexLayoutEntry{.type = type, .semantic = semantic, .semanticIndex = semanticIndex}},
                .inputRate = shaders::InputRate::Vertex});
          } break;
          case slang::TypeReflection::Kind::Struct: {
            bool isInstanceData = false;
            auto attribCount = param->getType()->getUserAttributeCount();
            for (uint32_t idx = 0; idx < attribCount; ++idx) {
              auto attribute = param->getType()->getUserAttributeByIndex(idx);
              if (strcmp(attribute->getName(), "instance") == 0) {
                isInstanceData = true;
              }
            }
            auto fieldCount = param->getTypeLayout()->getFieldCount();
            shaders::VertexBuffer buffer;
            for (auto k = 0u; k < fieldCount; ++k) {
              auto field = param->getTypeLayout()->getFieldByIndex(k);
              auto types = slangTypeToKeptechTypes(field->getType());
              if (types.size() != 1) {
                return std::unexpected<std::string>("Reflection does not yet support nested structs for vertex input");
              }
              auto semantic = field->getSemanticName();
              auto semanticIndex = field->getSemanticIndex();
              buffer.layout.push_back(shaders::VertexLayoutEntry{.type = types[0], .semantic = semantic, .semanticIndex = semanticIndex});
            }
            buffer.inputRate = isInstanceData ? shaders::InputRate::Instance : shaders::InputRate::Vertex;
            vertexLayout.push_back(std::move(buffer));
          } break;
          default:
          }
        }

        auto res = parseVertexAttribs(shader.info.vertex, *entryPoint->getFunction());
        if (!res) {
          return std::unexpected<std::string>("Failed to parse vertex attributes: " + res.error());
        }

        shader.info.pushConstantSize = std::max(shader.info.pushConstantSize, pushConstantSize);

        break;
      }
      case kt::shaders::ShaderStages::Mesh: {
        auto res = parseVertexAttribs(shader.info.vertex, *entryPoint->getFunction());
        if (!res) {
          return std::unexpected<std::string>("Failed to parse mesh attributes: " + res.error());
        }

        size_t pushConstantSize = 0;

        auto paramCount = entryPoint->getParameterCount();
        for (auto j = 0u; j < paramCount; ++j) {
          auto param = entryPoint->getParameterByIndex(j);
          auto category = param->getCategory();
          if (category == slang::ParameterCategory::Uniform) {
            pushConstantSize += param->getTypeLayout()->getSize();
          }
        }

        shader.info.pushConstantSize = std::max(shader.info.pushConstantSize, pushConstantSize);
        break;
      }
      case kt::shaders::ShaderStages::Fragment: {
        auto returnT = entryPoint->getFunction()->getReturnType();
        Slang::ComPtr<slang::IBlob> typeNameBlob;
        returnT->getFullName(typeNameBlob.writeRef());

        std::string_view returnTypeName(static_cast<const char*>(typeNameBlob->getBufferPointer()), typeNameBlob->getBufferSize());

        auto res = parseFragmentAttribs(shader.info.fragment, *entryPoint->getFunction());
        if (!res) {
          return std::unexpected<std::string>("Failed to parse fragment attributes: " + res.error());
        }

        size_t pushConstantSize = 0;
        auto paramCount = entryPoint->getParameterCount();
        for (auto j = 0u; j < paramCount; ++j) {
          auto param = entryPoint->getParameterByIndex(j);
          auto category = param->getCategory();
          if (category == slang::ParameterCategory::Uniform) {
            pushConstantSize += param->getTypeLayout()->getSize();
          }
        }

        shader.info.pushConstantSize = std::max(shader.info.pushConstantSize, pushConstantSize);

      } break;
      default:
        break;
      }
      shader.stages.push_back(kt::shaders::ShaderStage{.name = entryPoint->getName(), .stage = stage});
    }

    shader.info.vertex.layout.reserve(vertexLayout.size());
    for (auto& layoutEntry : vertexLayout) {
      shader.info.vertex.layout.emplace_back(std::move(layoutEntry));
    }

    return Return<kt::shaders::Shader>{.value = std::move(shader), .diagnostics = std::move(diag)};
  }
} // namespace kt::shader_processor
