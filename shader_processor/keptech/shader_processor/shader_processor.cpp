#include "shader_processor.hpp"

#include "keptech/shaders/stages.hpp"
#include "paramProcessing.hpp"
#include "shader-logger.hpp"
#include <algorithm>
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
    searchPaths.reserve(searchPaths.size() + config.includePaths.size());
    for (const auto& path : config.includePaths) {
      searchPaths.push_back(path.c_str());
    }

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
    SHDR_INFO("Shader {}:", name);
    if (file)
      SHDR_INFO("  File: {}", file);
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

    LayoutManager layoutManager{};
    layoutManager.printProgramLayout(layout, shader.info);

    for (auto& set : shader.info.globalResources.sets) {
      std::sort(set.resources.begin(), set.resources.end(), [](const auto& a, const auto& b) { return a.binding < b.binding; });
    }

    for (auto& entryPointResources : shader.info.entryPointResources) {
      for (auto& set : entryPointResources.sets) {
        std::sort(set.resources.begin(), set.resources.end(), [](const auto& a, const auto& b) { return a.binding < b.binding; });
      }
    }

    for (auto& buffer : shader.info.vertex.layout) {
      std::sort(buffer.layout.begin(), buffer.layout.end(), [](const auto& a, const auto& b) { return a.vIndex < b.vIndex; });
    }

    auto printResources = [&](const shaders::Resources& resources, size_t indent) {
      auto indentStr = std::string(indent, ' ');
      for (size_t space = 0; space < resources.sets.size(); ++space) {
        auto& resourceSet = resources.sets[space];
        SHDR_INFO("{}Space {}: {} resources", indentStr, space, resourceSet.resources.size());
        for (const auto& resource : resourceSet.resources) {
          SHDR_INFO("{}  Binding {}: {} {} (count: {}, push: {}, size/stride: {})", indentStr, resource.binding, resource.type,
                    resource.name, resource.count, resource.isPush, resource.bufferInfo.sizeOrStride);
          for (const auto& [fieldName, offset] : resource.bufferInfo.fieldOffsets) {
            SHDR_INFO("{}    Field {}: offset {}, size {}, stride {}", indentStr, fieldName, offset.offset, offset.size, offset.stride);
          }
        }
      }
      if (resources.pushConstants.size > 0) {
        SHDR_INFO("{}Push Constants: {} bytes, space {}", indentStr, resources.pushConstants.size, resources.pushConstants.space);
        for (const auto& [fieldName, offset] : resources.pushConstants.fieldOffsets) {
          SHDR_INFO("{}  Field {}: offset {}, size {}, stride {}", indentStr, fieldName, offset.offset, offset.size, offset.stride);
        }
      }
    };

    SHDR_INFO("  Global Resources:");
    printResources(shader.info.globalResources, 4);

    shader.stages.reserve(entryPointCount);

    for (uint32_t i = 0; i < entryPointCount; ++i) {
      auto entryPoint = layout->getEntryPointByIndex(i);
      kt::shaders::ShaderStages stage = slangStagetoKeptechStage(entryPoint->getStage());
      shader.stages.push_back(kt::shaders::ShaderStage{.name = entryPoint->getName(), .stage = stage});

      switch (stage) {
      case shaders::ShaderStages::Vertex: {
        auto res = parseVertexAttribs(shader.info.vertex, *entryPoint->getFunction());
        if (!res) {
          return std::unexpected<std::string>("Failed to parse vertex attributes for entry point \"" + std::string(entryPoint->getName()) +
                                              "\": " + res.error());
        }
      } break;
      case shaders::ShaderStages::Mesh: {
        auto res = parseVertexAttribs(shader.info.vertex, *entryPoint->getFunction());
        if (!res) {
          return std::unexpected<std::string>("Failed to parse geometry attributes for entry point \"" +
                                              std::string(entryPoint->getName()) + "\": " + res.error());
        }
      } break;
      case shaders::ShaderStages::Fragment: {
        auto res = parseFragmentAttribs(shader.info.fragment, *entryPoint->getFunction());
        if (!res) {
          return std::unexpected<std::string>("Failed to parse fragment attributes for entry point \"" +
                                              std::string(entryPoint->getName()) + "\": " + res.error());
        }
      } break;
      default:
        break;
      }
    }

    SHDR_INFO("  Entry Points:");
    for (size_t i = 0; i < shader.info.entryPointResources.size(); ++i) {
      auto& resources = shader.info.entryPointResources[i];
      SHDR_INFO("    {}) {} \"{}\":", i, shader.stages[i].stage, shader.stages[i].name);
      printResources(shader.info.entryPointResources[i], 6);

      auto stage = shader.stages[i].stage;
      switch (stage) {
      case shaders::ShaderStages::Vertex:
      case shaders::ShaderStages::Fragment:
      case shaders::ShaderStages::Geometry:
      case shaders::ShaderStages::Mesh:
      case shaders::ShaderStages::Task: {
        if (resources.pushConstants.size > 0 || !resources.sets.empty()) {
          SHDR_ERROR("Shader \"{}\" has a {} entry point (\"{}\") with entry point resources. Per-entry resources are not supported for "
                     "multi-stage programs (vertex/fragment/geometry/mesh/task).",
                     name, stage, shader.stages[i].name);
          return std::unexpected<std::string>(
              "Per-entry resources are not supported for multi-stage programs (vertex/fragment/geometry/mesh/task).");
        }
      } break;
      default:
        break;
      }
    }

    return Return<kt::shaders::Shader>{.value = std::move(shader), .diagnostics = std::move(diag)};
  }
} // namespace kt::shader_processor
