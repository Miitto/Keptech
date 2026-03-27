#pragma once

#include <expected>
#include <span>
#include <string>
#include <vulkan/vulkan.h>

namespace kt::vkh {

  class Shader {
    VkShaderModule module;

    Shader(VkShaderModule& module) noexcept : module(std::move(module)) {}

  public:
    void destroy(const VkDevice& device) {
      vkDestroyShaderModule(device, module, nullptr);
    }

    struct Stage {
      VkShaderStageFlags stage;
      VkShaderModule& module;
      const char* name;
    };

    static auto create(const VkDevice& device,
                       const std::span<const uint8_t> code)
        -> std::expected<Shader, std::string>;

    [[nodiscard]] auto get() const noexcept -> const VkShaderModule& {
      return module;
    }

    operator const VkShaderModule&() const noexcept { return module; }
    auto operator*() const noexcept -> const VkShaderModule& { return module; }

    struct ShaderStageParams {
      VkShaderStageFlagBits stage;
      const char* name;
    };

    template <size_t LEN>
    [[nodiscard]] constexpr inline auto stages(
        const std::array<ShaderStageParams, LEN> shaderStages) const noexcept {
      std::array<VkPipelineShaderStageCreateInfo, LEN> stages;

      for (size_t i = 0; i < LEN; ++i) {
        stages[i] =
            VkPipelineShaderStageCreateInfo{.stage = shaderStages[i].stage,
                                            .module = get(),
                                            .pName = shaderStages[i].name};
      }

      return stages;
    }
  };
} // namespace kt::vkh
