#include "keptech/shaders/shader.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <keptech/shader_processor/shader_processor.hpp>

using namespace kt::shader_processor;

enum CliPos : uint8_t {
  POS_VAR_NAME = 1,
  POS_NAMESPACE,
  POS_INPUT_FILE,
  POS_OUTPUT_HEADER,
  POS_OUTPUT_SOURCE,
  POS_OPT_LEVEL,
  POS_DEBUG_INFO,
};

int main(int argc, char** argv) {
  if (argc < 6 || argc > 8) {
    std::cerr << "Usage: shader_embedder <var_name> <namespace> <input_file> <output_header> <output_source> "
                 "[optimization_level (0|1|3)] [debug_info d]\n";
    return -1;
  }

  const char* name = argv[POS_VAR_NAME];
  const char* ns = argv[POS_NAMESPACE];
  const char* inputFile = argv[POS_INPUT_FILE];
  const char* outputHeader = argv[POS_OUTPUT_HEADER];
  const char* outputSource = argv[POS_OUTPUT_SOURCE];

  kt::shader_processor::init();

  SessionConfig config;
  if (argc > POS_OPT_LEVEL) {
    switch (argv[POS_OPT_LEVEL][0]) {
    case '0':
      config.optimizationLevel = OptimizationLevel::None;
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

  if (argc > POS_DEBUG_INFO) {
    config.debugInfo = argv[POS_DEBUG_INFO][0] == 'd';
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

  std::ofstream outHeader(outputHeader);
  std::ofstream outSource(outputSource);

  outHeader << "#pragma once\n\n#include <keptech/shaders/shader.h>\n";
  outHeader << "namespace " << ns << " {\n    extern const kt::shaders::Shader " << name << ";\n}\n";

  outSource << "#include \"" << std::filesystem::path(outputHeader).filename().string() << "\"\n\n";
  outSource << "namespace " << ns << "{\n    const kt::shaders::Shader " << name << "{ .name = \"" << name << "\",\n .file = \""
            << inputFile << "\",\n .code = {\n";
  for (size_t i = 0; i < shader.code.size(); ++i) {
    if (i % 96 == 0) {
      outSource << "\n    ";
    }
    outSource << "0x" << std::hex << static_cast<uint32_t>(shader.code[i]);
    if (i + 1 < shader.code.size()) {
      outSource << ", ";
    }
  }
  outSource << "\n},\n .mode = static_cast<kt::shaders::RenderingMode>(" << std::dec << static_cast<uint32_t>(shader.mode)
            << "),\n .stages = {\n";

  for (auto& stage : shader.stages) {
    outSource << "{.name = \"" << stage.name << "\", .stage = static_cast<kt::shaders::ShaderStages>(" << std::dec
              << static_cast<uint32_t>(stage.stage) << ")},\n";
  }

  outSource << "\n},\n .vertexLayout = {\n";

  for (auto& layout : shader.vertexLayout) {
    outSource << "{\n";
    for (auto& type : layout) {
      outSource << "    static_cast<kt::shaders::DataType>(" << std::dec << static_cast<uint32_t>(type) << "),\n";
    }
    outSource << "},\n";
  }

  outSource << "},\n};\n}";

  return 0;
}
