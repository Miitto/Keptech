#pragma once

#include "keptech/graph/builder.hpp"
#include "keptech/graph/pass.hpp"
#include "keptech/rhi/rhi.hpp"
#include "shaders/examples/triangle/triangle.h"
#include <keptech/core/kt-logger.hpp>
#include <keptech/graph/graph.hpp>
#include <keptech/graph/passInterface.hpp>
#include <keptech/rhi/pipeline.hpp>
#include <keptech/rhi/pipelineBuilder.hpp>

class TrianglePass : public kt::RenderPassInterface {
public:
  TrianglePass() = default;

  void setupDependencies(kt::RenderPassBuilder& self, kt::RenderGraphBuilder& graph) override {
    self.addColorOutput("color", {
                                     .sizeType = kt::AttachmentSize::SwapchainRelative,
                                     .format = kt::rhi::ImageFormat::R8G8B8A8_UNORM,
                                 });
  }

  void setup(kt::RenderGraph& g) override {
    kt::rhi::PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::triangle).addColorAttachment(kt::rhi::ImageFormat::R8G8B8A8_UNORM);

    auto pipeline_res = pipelineBuilder.build();
    if (!pipeline_res.has_value()) {
      KT_ABORT("Failed to create graphics pipeline: {}", pipeline_res.error());
    }
    pipeline = pipeline_res.value();

    colorImageIndex = g.getImageIndex("color");
  }

  void execute(kt::RenderGraph& g, kt::rhi::CommandBuffer& cmd, kt::rhi::DescriptorSet&, glm::uvec2 framebufferSize) override {
    KT_TRACE("Executing geometry pass");

    auto& img = g.getImage(colorImageIndex);
    cmd.clearColorImage(img, {0.0f, 0.0f, 0.0f, 1.0f});

    cmd.bindGraphicsPipeline(pipeline);

    kt::rhi::ImageRef ref = img;

    std::array attachments = {kt::rhi::CommandBuffer::ColorAttachmentDesc{
        .imageRef = ref, .loadOp = kt::rhi::LoadOp::Clear, .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}}};

    cmd.beginRendering(attachments);

    cmd.setViewport({static_cast<float>(framebufferSize.x), static_cast<float>(framebufferSize.y)});
    cmd.setScissor({framebufferSize.x, framebufferSize.y});

    cmd.draw(3);

    cmd.endRendering();
  }

  bool getClearColor(size_t attachmentIndex, kt::rhi::ColorClearValue* value) const override { return true; }

  bool getClearDepthStencil(kt::rhi::DepthClearValue* value) const override { return true; }

  void shutdown(kt::RenderGraph&) override {}

private:
  kt::rhi::Pipeline pipeline{};
  size_t colorImageIndex = 0;
};