#include "rhi.hpp"

#include "keptech/rhi/pipelineBuilder.hpp"
#include "shaders/keptech/rhi/convoluteIrradiance.h"
#include "shaders/keptech/rhi/cubeFromEquirectangular.h"

namespace kt::rhi {
  std::expected<void, std::string> RHI::initPipelines() {
    {
      PipelineBuilder builder{};

      builder.setShader(::shaders::kt::cubeFromEquirectangular).addColorAttachment(ImageFormat::R32G32B32A32_FLOAT);

      auto pipelineRes = builder.build();
      if (!pipelineRes) {
        return std::unexpected(fmt::format("Failed to create cube from equirectangular pipeline: {}", pipelineRes.error()));
      }

      m.cubeFromEquirectangularPipeline = std::move(pipelineRes.value());
    }

    {
      PipelineBuilder builder{};

      builder.setShader(::shaders::kt::convoluteIrradiance).addColorAttachment(ImageFormat::R32G32B32A32_FLOAT);

      auto pipelineRes = builder.build();
      if (!pipelineRes) {
        return std::unexpected(fmt::format("Failed to create convolute irradiance pipeline: {}", pipelineRes.error()));
      }

      m.convoluteIrradiancePipeline = std::move(pipelineRes.value());
    }

    return {};
  }
} // namespace kt::rhi