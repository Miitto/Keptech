#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {
  struct Material {
    enum class Stage : uint8_t { Deferred, Forward, Transparent };

    Stage stage;
    vk::raii::Pipeline pipeline;
    vk::raii::PipelineLayout pipelineLayout;
  };
} // namespace keptech::vkh
