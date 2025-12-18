#pragma once

#include <expected>
#include <string>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {

  class Shader {
    vk::raii::ShaderModule module;

    Shader(vk::raii::ShaderModule& module) noexcept
        : module(std::move(module)) {}

  public:
    struct Stage {
      vk::ShaderStageFlags stage;
      vk::raii::ShaderModule& module;
      const char* name;
    };

    static auto create(const vk::raii::Device& device,
                       const std::string& filename)
        -> std::expected<Shader, std::string>;

    [[nodiscard]] auto get() const noexcept -> const vk::raii::ShaderModule& {
      return module;
    }

    operator const vk::raii::ShaderModule&() const noexcept { return module; }
    auto operator*() const noexcept -> const vk::raii::ShaderModule& {
      return module;
    }

    struct ShaderStageParams {
      vk::ShaderStageFlagBits stage;
      const char* name;
    };

    template <size_t LEN>
    [[nodiscard]] constexpr inline auto stages(
        const std::array<ShaderStageParams, LEN> shaderStages) const noexcept {
      std::array<vk::PipelineShaderStageCreateInfo, LEN> stages;

      for (size_t i = 0; i < LEN; ++i) {
        stages[i] =
            vk::PipelineShaderStageCreateInfo{.stage = shaderStages[i].stage,
                                              .module = get(),
                                              .pName = shaderStages[i].name};
      }

      return stages;
    }

    constexpr inline auto
    vertFrag(const char* const vertFn = "vert",
             const char* const fragFn = "frag") const noexcept
        -> std::array<vk::PipelineShaderStageCreateInfo, 2> {

      std::array<ShaderStageParams, 2> shaderStages = {
          ShaderStageParams{.stage = vk::ShaderStageFlagBits::eVertex,
                            .name = vertFn},
          ShaderStageParams{.stage = vk::ShaderStageFlagBits::eFragment,
                            .name = fragFn}};

      return stages(shaderStages);
    }
  };
} // namespace keptech::vkh
