#pragma once

#include "keptech/core/rendering/pipeline.hpp"
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {
  struct LoadedPipeline : public keptech::core::rendering::Pipeline {
    vk::raii::Pipeline pipeline;
    vk::raii::PipelineLayout pipelineLayout;
    uint32_t extraInstanceDataSize = 0;
  };
} // namespace keptech::vkh
