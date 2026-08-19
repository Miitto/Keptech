#include "keptech/shaders/shader.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "keptech/shader_processor/shader-logger.hpp"
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

void writeCode(std::ofstream& file, const kt::shaders::Shader& shader) {
  file << "    .code = {\n";
  for (size_t i = 0; i < shader.code.size(); ++i) {
#ifdef KT_VULKAN
    auto& code = shader.code;
#endif
#ifdef KT_DX12
    file << "    // Entry point " << i << "\n    {";
    size_t j = i;
    for (size_t i = 0; i < shader.code[j].size(); ++i) {
      auto& code = shader.code[j];
#endif
      if (i % 96 == 0) {
        file << "\n    ";
      }
      file << "0x" << std::hex << static_cast<uint32_t>(code[i]);
      if (i + 1 < code.size()) {
        file << ", ";
      }
#ifdef KT_DX12
    }
    file << "\n    },\n";
#endif
  }
  file << "\n},\n";
}

void writeStages(std::ofstream& file, const std::vector<kt::shaders::ShaderStage>& stages) {
  file << "    .stages = {\n";
  for (auto& stage : stages) {
    file << "        {.name = \"" << stage.name << "\", .stage = static_cast<::kt::shaders::ShaderStages>(" << std::dec
         << static_cast<uint32_t>(stage.stage) << ")},\n";
  }
  file << "    },\n";
}

void writeVertex(std::ofstream& file, const kt::shaders::Vertex& vertex) {
  file << "    .vertex = {\n        .topology = static_cast<::kt::shaders::PrimitiveTopology>(" << static_cast<uint32_t>(vertex.topology)
       << "),\n        .layout = {\n";
  for (auto& buffer : vertex.layout) {
    file << "            {\n.layout = {\n";
    for (auto& entry : buffer.layout) {
      file << "                {.type = static_cast<::kt::shaders::DataType>(" << std::dec << static_cast<uint32_t>(entry.type) << "),\n"
           << "                 .semantic = \"" << entry.semantic << "\",\n"
           << "                 .semanticIndex = " << entry.semanticIndex << "},\n";
    }
    file << "},\n            .inputRate = static_cast<::kt::shaders::InputRate>(" << std::dec << static_cast<uint32_t>(buffer.inputRate)
         << "),\n"
         << "            },\n";
  }
  file << "        },\n    },\n";
}

void writeFragment(std::ofstream& file, const kt::shaders::Fragment& fragment) {
  file << "    .fragment = {\n        .enableBlending = " << (fragment.enableBlending ? "true" : "false") << ",\n"
       << "        .srcColorBlendFactor = static_cast<::kt::shaders::BlendFactor>(" << std::dec
       << static_cast<uint32_t>(fragment.srcColorBlendFactor) << "),\n"
       << "        .dstColorBlendFactor = static_cast<::kt::shaders::BlendFactor>(" << std::dec
       << static_cast<uint32_t>(fragment.dstColorBlendFactor) << "),\n"
       << "        .depthWrite = " << (fragment.depthWrite ? "true" : "false") << ",\n    },\n";
}

void writeResources(std::ofstream& file, const std::vector<kt::shaders::ResourceBinding>& resources, size_t pushConstantSize) {
  file << "    .resources = {\n";
  for (auto& resource : resources) {
    file << "        {.type = static_cast<::kt::shaders::ShaderResourceType>(" << std::dec << static_cast<uint32_t>(resource.type) << "),\n"
         << "         .set = " << resource.set << ",\n"
         << "         .binding = " << resource.binding << "},\n";
  }
  file << "    },\n    .pushConstantSize = " << std::dec << pushConstantSize << ",\n";
}

int main(int argc, char** argv) {
  if (argc < 6 || argc > 8) {
    SHDR_ERROR("Usage: shader_embedder <var_name> <namespace> <input_file> <output_header> <output_source> "
               "[optimization_level (0|1|3)] [debug_info d]");
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

  if (!std::filesystem::exists(inputFile)) {
    SHDR_ERROR("Input file does not exist: {}", inputFile);
    return -1;
  }

  std::ifstream inputStream(inputFile);

  auto size = std::filesystem::file_size(inputFile);
  std::string source(size, '\0');
  inputStream.read(source.data(), static_cast<std::streamsize>(size));

  try {
    auto [inputModule, inputDiag] = session.loadModule(name, source);
    if (inputDiag)
      SHDR_WARN("Input diagnostic: {}", (char*)inputDiag->getBufferPointer());
    if (!inputModule) {
      SHDR_ERROR("Failed to load input module.");
      return -1;
    }
  } catch (const std::exception& e) {
    SHDR_ERROR("Shader load error: {}", e.what());
    return -1;
  }

  auto link = [&]() -> std::expected<Program, int> {
    try {
      auto [program, diag] = session.link();
      if (diag)
        SHDR_WARN("Link diagnostic: {}", (char*)diag->getBufferPointer());
      if (!program.valid()) {
        SHDR_ERROR("Failed to link program.");
        return std::unexpected(-1);
      }

      return program;
    } catch (const std::exception& e) {
      SHDR_ERROR("Shader link error: {}", e.what());
      return std::unexpected(-1);
    };
  };

  auto programRes = link();
  if (!programRes) {
    return programRes.error();
  }
  auto& program = programRes.value();

  auto res = program.toShader(name);
  if (!res) {
    SHDR_ERROR("Failed to convert program to shader: {}", res.error());
    return -1;
  }

  auto& [shader, shaderDiag] = res.value();

  if (shaderDiag) {
    SHDR_WARN("Shader diagnostic: {}", (char*)shaderDiag->getBufferPointer());
  }

  shader.file = inputFile;

  std::ofstream outHeader(outputHeader);
  std::ofstream outSource(outputSource);

  outHeader << "#pragma once\n\n#include <keptech/shaders/shader.hpp>\n";
  outHeader << "namespace " << ns << " {\n    extern const ::kt::shaders::Shader " << name << ";\n}\n";

  outSource << "#include \"" << std::filesystem::path(outputHeader).filename().string() << "\"\n\n";
  outSource << "namespace " << ns << "{\n    const ::kt::shaders::Shader " << name << "{ .file = \"" << inputFile << "\",\n";

  writeCode(outSource, shader);

  writeStages(outSource, shader.stages);

  outSource << "    .info = {\n        .name = \"" << shader.info.name << "\",\n";

  writeVertex(outSource, shader.info.vertex);

  writeFragment(outSource, shader.info.fragment);

  writeResources(outSource, shader.info.resources, shader.info.pushConstantSize);

  outSource << "}\n};\n}";

  return 0;
}
