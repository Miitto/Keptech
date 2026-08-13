#pragma once

#include "keptech/graph/builder.hpp"
#include "keptech/graph/pass.hpp"
#include "keptech/rhi/rhi.hpp"
#include "shaders/examples/triangle/triangle.h"
#include <keptech/core/kt-logger.hpp>
#include <keptech/graph/passInterface.hpp>
#include <keptech/rhi/pipelineBuilder.hpp>
#include <keptech/rhi/wrappers/pipeline.hpp>

class TrianglePass : public kt::rhi::RenderPassInterface {
public:
  TrianglePass() = default;

  void setupDependencies(kt::rhi::RenderPassBuilder& self, kt::rhi::RenderGraphBuilder& graph, const kt::rhi::Renderer& renderer) override {
    auto& formats = renderer.getFormats();
    self.addColorOutput("color", {.format = formats.render.albedo});
  }

  void setup(kt::rhi::RenderGraph&, kt::rhi::Renderer& renderer) override {
    kt::rhi::PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::triangle).addColorAttachment(renderer.getFormats().render.albedo);

    auto pipeline_res = pipelineBuilder.build();
    if (!pipeline_res.has_value()) {
      KT_ABORT("Failed to create graphics pipeline: {}", pipeline_res.error());
    }
    pipeline = pipeline_res.value();
  }

  void execute(kt::rhi::RenderGraph&, const kt::rhi::CommandBuffer& cmd, glm::uvec2 framebufferSize) override {
    KT_TRACE("Executing geometry pass");
    auto& r = kt::rhi::RHI::get();
  }

  bool getClearColor(size_t attachmentIndex, kt::rhi::ColorClearValue* value) const override { return true; }

  bool getClearDepthStencil(kt::rhi::DepthClearValue* value) const override { return true; }

  void shutdown(kt::rhi::RenderGraph&, kt::rhi::Renderer& renderer) override {}

private:
  kt::rhi::Pipeline pipeline{};
};