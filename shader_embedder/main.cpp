#include "keptech/shaders/shader.h"
#include <filesystem>
#include <fstream>
#include <iostream>

#include <keptech/shader_processor/shader_processor.hpp>

using namespace keptech::shader_processor;

int main(int argc, char** argv) {
  if (argc < 4 || argc > 5) {
    std::cerr << "Usage: shader_embedder <var_name> <input_file> <output_file> "
                 "[optimization_level (0|d|1|3)]\n";
    return -1;
  }

  const char* name = argv[1];
  const char* inputFile = argv[2];
  const char* outputFile = argv[3];

  keptech::shader_processor::init();

  SessionConfig config;
  if (argc == 4) {
    switch (argv[3][0]) {
    case '0':
      config.optimizationLevel = OptimizationLevel::None;
      break;
    case 'd':
      config.optimizationLevel = OptimizationLevel::Debug;
      break;
    case '1':
      config.optimizationLevel = OptimizationLevel::Basic;
      break;
    case '3':
      config.optimizationLevel = OptimizationLevel::Aggressive;
      break;
    default:
      break;
    }
  }

  CompilerSession session(config);

  std::ifstream inputStream(inputFile);
  auto size = std::filesystem::file_size(inputFile);
  std::string source(size, '\0');
  inputStream.read(source.data(), static_cast<std::streamsize>(size));

  auto [inputModule, inputDiag] = session.loadModule(name, source);
  if (inputDiag)
    std::cerr << (char*)inputDiag->getBufferPointer() << '\n';
  if (!inputModule) {
    std::cerr << "Failed to load input module.\n";
    return -1;
  }

  auto [program, diag] = session.link();
  if (diag)
    std::cerr << (char*)diag->getBufferPointer() << '\n';
  if (!program.valid()) {
    std::cerr << "Failed to link program.\n";
    return -1;
  }

  auto res = program.toShader(name);
  if (!res) {
    std::cerr << "Failed to convert program to shader: " << res.error() << '\n';
    return -1;
  }

  auto& [shader, codeOwner] = res.value();

  std::ofstream out(outputFile);

  out << "#pragma once\n\n#include <keptech/shaders/shader.h>\n";

  out << "constexpr const unsigned char " << name << "_source[] = {";
  for (size_t i = 0; i < shader.code.size(); ++i) {
    if (i % 12 == 0) {
      out << "\n    ";
    }
    out << "0x" << std::hex << static_cast<uint32_t>(shader.code[i]);
    if (i + 1 < shader.code.size()) {
      out << ", ";
    }
  }
  out << "};\nconstexpr keptech::shaders::ShaderStage " << name
      << "_stages[] = {";

  for (auto& stage : shader.stages) {
    out << "{.name = \"" << stage.name
        << "\", .stage = static_cast<keptech::shaders::ShaderStages>("
        << static_cast<uint32_t>(stage.stage) << ")},\n";
  }

  out << "};\nconstexpr keptech::shaders::Shader " << name << "{ .name = \""
      << name << "\",\n .code = std::span(" << name << "_source, " << std::dec
      << shader.code.size()
      << "),\n.mode = static_cast<keptech::shaders::RenderingMode>("
      << static_cast<uint32_t>(shader.mode) << "),\n .stages = std::span("
      << name << "_stages, " << std::dec << shader.stages.size() << "),};\n";

  return 0;
}
