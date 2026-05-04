#pragma once

#include "keptech/shaders/shader.h"
#include <cstdint>
#include <expected>
#include <slang-com-ptr.h>
#include <string>
#include <vector>

namespace kt::shader_processor {
  void init();

  enum class OptimizationLevel : uint8_t {
    /// No optimization
    None,
    /// Optimized with O1
    Basic,
    /// Optimized with O3
    Aggressive
  };

  struct SessionConfig {
#ifndef NDEBUG
    /// Optimization level for the shader processor session. Default is None in debug builds.
    OptimizationLevel optimizationLevel = OptimizationLevel::None;
    bool debugInfo = true;
#else
    /// Optimization level for the shader processor session. Default is
    /// Aggressive in release builds.
    OptimizationLevel optimizationLevel = OptimizationLevel::Aggressive;
    bool debugInfo = false;
#endif
  };

  class CompilerSession;

  class Program {
    friend class CompilerSession;
    Program(Slang::ComPtr<slang::IComponentType> componentType);

  public:
    struct Kernel {
      Slang::ComPtr<slang::IBlob> spirv;
      Slang::ComPtr<slang::IBlob> diagnostics;
    };

    [[nodiscard]] bool valid() const { return program != nullptr; }

    [[nodiscard]] Kernel getCode() const;

    [[nodiscard]] slang::ProgramLayout* getLayout() const { return program->getLayout(); }

    [[nodiscard]] std::expected<kt::shaders::Shader, std::string> toShader(const char* name) const;

  private:
    Slang::ComPtr<slang::IComponentType> program;
  };

  class CompilerSession {
  public:
    CompilerSession(SessionConfig config);

    std::pair<slang::IModule*, Slang::ComPtr<slang::IBlob>> loadModule(const char* moduleName, const std::string& source,
                                                                       const char* path = nullptr);

    std::pair<Program, Slang::ComPtr<slang::IBlob>> link();

  private:
    Slang::ComPtr<slang::ISession> session;
    Slang::ComPtr<slang::IModule> keptechModule;
    std::vector<Slang::ComPtr<slang::IModule>> loadedModules;
  };

} // namespace kt::shader_processor
