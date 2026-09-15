#include "colorGrade.hpp"

#include "graph/graph.hpp"
#include "keptech/graph/builder.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/pipelineBuilder.hpp"
#include "shaders/keptech/colorGrade.h"

namespace kt {

  void ColorGradePass::setupDependencies(RenderPassBuilder& self, RenderGraphBuilder&) {
    self.addColorOutput("kt::colorGraded", {.format = rhi::ImageFormat::R8G8B8A8_UNORM});

    self.addUniformInput("kt::camera");
    self.addTextureInput(srcTexName);
    self.addTextureInput("kt::depth");
  }

  void ColorGradePass::setup(RenderGraph& graph, const rhi::DescriptorLayout&) {
    resultIndex = graph.getImageIndex("kt::colorGraded");

    kt::rhi::PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::kt::colorGrade).addColorAttachment(kt::rhi::ImageFormat::R8G8B8A8_UNORM);

    auto pipeline_res = pipelineBuilder.build();
    if (!pipeline_res.has_value()) {
      KT_ABORT("Failed to create graphics pipeline: {}", pipeline_res.error());
    }
    pipeline = pipeline_res.value();
  }

  void ColorGradePass::execute(RenderGraph& graph, rhi::CommandBuffer& cmd, const rhi::DescriptorSet& set, glm::uvec2 framebufferSize) {
    auto& renderTex = graph.getImage(resultIndex);

    cmd.bindGraphicsPipeline(pipeline);
    cmd.setViewport({static_cast<float>(framebufferSize.x), static_cast<float>(framebufferSize.y)});
    cmd.setScissor({framebufferSize.x, framebufferSize.y});

    std::array<rhi::CommandBuffer::ColorAttachmentDesc, 1> colorAttachments = {
        rhi::CommandBuffer::ColorAttachmentDesc{.imageRef = renderTex, .loadOp = rhi::LoadOp::DontCare},
    };

    cmd.beginRendering(colorAttachments);

    cmd.bindGraphicsDescriptorSet(set);

    cmd.draw(3);

    cmd.endRendering();
  }

  void ColorGradePass::addToGraph(RenderGraphBuilder& graph, std::string src) {
    auto& pass = graph.addPass("kt::colorGrade");
    pass.setInterface(this);
    srcTexName = std::move(src);
  }
} // namespace kt