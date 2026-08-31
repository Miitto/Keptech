#include "lightCombine.hpp"

#include "graph/graph.hpp"
#include "keptech/graph/builder.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/pipelineBuilder.hpp"
#include "shaders/keptech/lightCombine.h"

namespace kt {

  void LightCombinePass::setupDependencies(RenderPassBuilder& self, RenderGraphBuilder&) {
    self.addColorOutput("kt::lighting", {.format = rhi::ImageFormat::R11G11B10_FLOAT});

    self.addTextureInput("kt::emissive");
    self.addTextureInput("kt::diffuse");
    self.addTextureInput("kt::specular");
    self.addHistoryInput("kt::albedo");
  }

  void LightCombinePass::setup(RenderGraph& graph, const rhi::DescriptorLayout&) {
    lightTexIndex = graph.getImageIndex("kt::lighting");

    kt::rhi::PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::kt::lightCombine).addColorAttachment(kt::rhi::ImageFormat::R11G11B10_FLOAT);

    auto pipeline_res = pipelineBuilder.build();
    if (!pipeline_res.has_value()) {
      KT_ABORT("Failed to create graphics pipeline: {}", pipeline_res.error());
    }
    pipeline = pipeline_res.value();
  }

  void LightCombinePass::execute(RenderGraph& graph, rhi::CommandBuffer& cmd, const rhi::DescriptorSet& set, glm::uvec2 framebufferSize) {
    auto& lightTex = graph.getImage(lightTexIndex);

    cmd.bindGraphicsPipeline(pipeline);
    cmd.setViewport({static_cast<float>(framebufferSize.x), static_cast<float>(framebufferSize.y)});
    cmd.setScissor({framebufferSize.x, framebufferSize.y});

    std::array<rhi::CommandBuffer::ColorAttachmentDesc, 1> colorAttachments = {
        rhi::CommandBuffer::ColorAttachmentDesc{.imageRef = lightTex, .loadOp = rhi::LoadOp::DontCare},
    };
    cmd.beginRendering(colorAttachments);

    cmd.bindGraphicsDescriptorSet(set);

    cmd.draw(3);

    cmd.endRendering();
  }

  void LightCombinePass::addToGraph(RenderGraphBuilder& graph) {
    auto& pass = graph.addPass("kt::lightCombine");
    pass.setInterface(this);
  }
} // namespace kt