#pragma once

#include "keptech/shaders/shader.hpp"
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

  template <typename T> struct Return {
    T value;
    Slang::ComPtr<slang::IBlob> diagnostics;
  };

  class Program {
    friend class CompilerSession;
    Program(Slang::ComPtr<slang::IComponentType> componentType, size_t entryPointCount);

  public:
    struct Kernel {
      Slang::ComPtr<slang::IBlob> code;
      Slang::ComPtr<slang::IBlob> diagnostics;
    };

    [[nodiscard]] bool valid() const { return program != nullptr; }

    [[nodiscard]]
    std::vector<Kernel> getCode() const;

    [[nodiscard]] slang::ProgramLayout* getLayout() const { return program->getLayout(); }

    [[nodiscard]] std::expected<Return<kt::shaders::Shader>, std::string> toShader(const char* name, const char* file = nullptr) const;

  private:
    Slang::ComPtr<slang::IComponentType> program;
    size_t entryPointCount = 0;
  };

  class CompilerSession {
  public:
    CompilerSession(SessionConfig config);

    Return<slang::IModule*> loadModule(const char* moduleName, const std::string& source, const char* path = nullptr);

    Return<Program> link();

  private:
    Slang::ComPtr<slang::ISession> session;
    Slang::ComPtr<slang::IModule> keptechModule;
    std::vector<Slang::ComPtr<slang::IModule>> loadedModules;
  };

} // namespace kt::shader_processor
