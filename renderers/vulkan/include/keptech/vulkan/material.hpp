#pragma once

#include <keptech/core/rendering/material.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {
  struct Material : public core::rendering::Material {
    vk::raii::Pipeline pipeline;
    vk::raii::PipelineLayout pipelineLayout;
  };
} // namespace keptech::vkh
