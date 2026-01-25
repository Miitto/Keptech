#pragma once

#include "keptech/core/rendering/pipeline.hpp"
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {
  struct LoadedPipeline : public keptech::IPipeline {
    vk::raii::Pipeline pipeline;
    vk::raii::PipelineLayout pipelineLayout;
    uint32_t extraInstanceDataSize = 0;

    void setRenderingMode(shaders::RenderingMode newMode) { mode = newMode; }

    std::vector<InstanceDataType>& getInstanceDataTypes() {
      return instanceDataTypes;
    }
  };
} // namespace keptech::vkh
