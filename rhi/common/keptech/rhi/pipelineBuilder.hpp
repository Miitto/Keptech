#pragma once

#include "keptech/rhi/imageFormat.hpp"
#include "keptech/shaders/shader.hpp"
#include "pipelineTypes.hpp"
#include <expected>


namespace kt::rhi {

  struct Pipeline;

  class PipelineBuilder {
  public:
    PipelineBuilder& setShader(const shaders::Shader& shader);
    PipelineBuilder& setPolygonMode(PolygonMode mode);
    PipelineBuilder& setFrontFace(FrontFace face);
    PipelineBuilder& setDepthCompareOp(DepthCompareOp op);
    PipelineBuilder& addColorAttachment(ImageFormat format);
    PipelineBuilder& setDepthAttachment(ImageFormat format);

    std::expected<Pipeline, std::string> build();

  private:
    const shaders::Shader* shader = nullptr;
    PolygonMode polygonMode = PolygonMode::Fill;
    FrontFace frontFace = FrontFace::CounterClockwise;
    DepthCompareOp depthCompareOp = DepthCompareOp::Always;
    std::vector<ImageFormat> colorAttachmentFormats{};
    ImageFormat depthAttachmentFormat = ImageFormat::Undefined;
  };
} // namespace kt::rhi