#include "pipelineBuilder.hpp"

namespace kt::rhi {
  PipelineBuilder& PipelineBuilder::setShader(const shaders::Shader& s) {
    shader = &s;
    return *this;
  }

  PipelineBuilder& PipelineBuilder::setPolygonMode(PolygonMode mode) {
    this->polygonMode = mode;
    return *this;
  }

  PipelineBuilder& PipelineBuilder::setFrontFace(FrontFace face) {
    this->frontFace = face;
    return *this;
  }

  PipelineBuilder& PipelineBuilder::setDepthCompareOp(DepthCompareOp op) {
    this->depthCompareOp = op;
    return *this;
  }

  PipelineBuilder& PipelineBuilder::addColorAttachment(ImageFormat format) {
    colorAttachmentFormats.push_back(format);
    return *this;
  }

  PipelineBuilder& PipelineBuilder::setDepthAttachment(ImageFormat format) {
    depthAttachmentFormat = format;
    return *this;
  }
} // namespace kt::rhi