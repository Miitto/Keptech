#include "keptech/vulkan/helpers/shader.hpp"

#include "vk-logger.hpp"
#include <fstream>
#include <macros.hpp>
#include <vector>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#include <Windows.h>
#endif

namespace keptech::vkh {

  [[nodiscard]]
  auto createShaderModule(const vk::raii::Device& device,
                          std::span<const unsigned char> code)
      -> std::expected<vk::raii::ShaderModule, std::string> {

    vk::ShaderModuleCreateInfo createInfo{
        .codeSize = code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())};

    VK_MAKE(shaderModule, device.createShaderModule(createInfo),
            "Failed to create shader module");

    return std::move(shaderModule);
  }

  auto Shader::create(const vk::raii::Device& device,
                      const unsigned char* const code, size_t size)
      -> std::expected<Shader, std::string> {
    auto shaderModule_res = createShaderModule(device, {code, size});

    if (!shaderModule_res) {
      return std::unexpected(shaderModule_res.error());
    }

    return Shader(shaderModule_res.value());
  }

} // namespace keptech::vkh
