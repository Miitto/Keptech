#include "keptech/vulkan/helpers/shader.hpp"

#include "vk-logger.hpp"
#include <fstream>
#include <macros.hpp>
#include <vector>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#include <Windows.h>
#endif

namespace keptech::vkh {

  namespace {

    auto getExecutablePath() -> std::string {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
      std::array<char, MAX_PATH> buffer = {};
      GetModuleFileNameA(nullptr, buffer.data(), MAX_PATH);
      std::string fullPath(buffer.data());
      size_t pos = fullPath.find_last_of("\\/");
      if (pos != std::string::npos) {
        return std::string(fullPath.substr(0, pos + 1));
      } else {
        return "";
      }

#else
#error "getExecutablePath not implemented for this platform"
#endif
    }

    auto readFile(const std::string& filename)
        -> std::expected<std::vector<char>, std::string> {

      auto exePath = getExecutablePath();
      std::string path(exePath + "shaders/" + filename);
      VK_DEBUG("Reading shader file: {}", path);
      std::ifstream file(path, std::ios::ate | std::ios::binary);

      if (!file.is_open()) {
        return std::unexpected("Failed to open file: " + path);
      }

      std::vector<char> buffer(file.tellg());

      file.seekg(0);

      file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

      file.close();

      return buffer;
    }

    [[nodiscard]]
    auto createShaderModule(const vk::raii::Device& device,
                            const std::string& filename)
        -> std::expected<vk::raii::ShaderModule, std::string> {
      auto code_res = readFile(filename);

      if (!code_res) {
        return std::unexpected(code_res.error());
      }

      auto& code = code_res.value();

      vk::ShaderModuleCreateInfo createInfo{
          .codeSize = code.size() * sizeof(char),
          .pCode = reinterpret_cast<const uint32_t*>(code.data())};

      VK_MAKE(shaderModule, device.createShaderModule(createInfo),
              "Failed to create shader module");

      return std::move(shaderModule);
    }
  } // namespace

  auto Shader::create(const vk::raii::Device& device,
                      const std::string& filename)
      -> std::expected<Shader, std::string> {
    auto shaderModule_res = createShaderModule(device, filename);

    if (!shaderModule_res) {
      return std::unexpected(shaderModule_res.error());
    }

    return Shader(shaderModule_res.value());
  }

} // namespace keptech::vkh
