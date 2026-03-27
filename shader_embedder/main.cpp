#include "keptech/shaders/shader.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <keptech/shader_processor/shader_processor.hpp>

using namespace kt::shader_processor;

int main(int argc, char** argv) {
  if (argc < 4 || argc > 5) {
    std::cerr << "Usage: shader_embedder <var_name> <input_file> <output_file> "
                 "[optimization_level (0|d|1|3)]\n";
    return -1;
  }

  const char* name = argv[1];
  const char* inputFile = argv[2];
  const char* outputFile = argv[3];

  kt::shader_processor::init();

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

  auto& shader = res.value();

  shader.file = inputFile;

  std::ofstream out(outputFile);

  out << "#pragma once\n\n#include <keptech/shaders/shader.h>\n";

  out << "keptech::shaders::Shader " << name << "{ .name = \"" << name
      << "\",\n .file = \"" << inputFile << "\",\n .code = {\n";
  for (size_t i = 0; i < shader.code.size(); ++i) {
    if (i % 96 == 0) {
      out << "\n    ";
    }
    out << "0x" << std::hex << static_cast<uint32_t>(shader.code[i]);
    if (i + 1 < shader.code.size()) {
      out << ", ";
    }
  }
  out << "\n},\n .mode = static_cast<keptech::shaders::RenderingMode>("
      << std::dec << static_cast<uint32_t>(shader.mode) << "),\n .stages = {\n";

  for (auto& stage : shader.stages) {
    out << "{.name = \"" << stage.name
        << "\", .stage = static_cast<keptech::shaders::ShaderStages>("
        << std::dec << static_cast<uint32_t>(stage.stage) << ")},\n";
  }

  out << "\n},\n .vertexLayout = {\n";

  for (auto& layout : shader.vertexLayout) {
    out << "{\n";
    for (auto& type : layout) {
      out << "    static_cast<keptech::shaders::DataType>(" << std::dec
          << static_cast<uint32_t>(type) << "),\n";
    }
    out << "},\n";
  }

  out << "},\n};\n";

  return 0;
}
