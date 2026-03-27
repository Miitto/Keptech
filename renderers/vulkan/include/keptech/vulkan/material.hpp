#pragma once

#include "keptech/rendering/pipeline.hpp"
#include <utility>
#include <vulkan/vulkan.h>

namespace kt::vkh {
  struct LoadedPipeline : public kt::IPipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    uint32_t extraInstanceDataSize = 0;

    void setRenderingMode(shaders::RenderingMode newMode) { mode = newMode; }

    std::vector<shaders::DataType>& getInstanceDataTypes() {
      return instanceDataTypes;
    }

    LoadedPipeline(VkPipeline&& pipeline, VkPipelineLayout&& pipelineLayout,
                   uint32_t extraInstanceDataSize, PipelineStage stage,
                   std::vector<shaders::DataType> instanceDataTypes
#ifdef KT_ADD_RESOURCE_INFO
                   ,
                   std::string name, shaders::RenderingMode mode,
                   PipelineCreateInfo createInfo
#endif
                   )
        : IPipeline(stage, std::move(instanceDataTypes)
#ifdef KT_ADD_RESOURCE_INFO
                               ,
                    std::move(name), mode, std::move(createInfo)
#endif
                        ),
          pipeline(std::move(pipeline)),
          pipelineLayout(std::move(pipelineLayout)),
          extraInstanceDataSize(extraInstanceDataSize) {
    }

    void replace(IPipeline& other) final {
      IPipeline::replace(other);
      auto& otherPipeline = static_cast<LoadedPipeline&>(other);
      pipeline = std::move(otherPipeline.pipeline);
      pipelineLayout = std::move(otherPipeline.pipelineLayout);
      extraInstanceDataSize = otherPipeline.extraInstanceDataSize;
    }
  };
} // namespace kt::vkh
