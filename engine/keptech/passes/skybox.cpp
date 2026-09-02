#include "skybox.hpp"

#include "graph/graph.hpp"
#include "keptech/graph/builder.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/pipelineBuilder.hpp"
#include "shaders/keptech/skybox.h"

namespace kt {

  void SkyboxPass::setupDependencies(RenderPassBuilder& self, RenderGraphBuilder&) {
    self.addColorOutput("kt::skybox", {.format = rhi::ImageFormat::R8G8B8A8_UNORM});

    self.addUniformInput("kt::camera");
    self.addTextureInput(srcTexName);
    self.addTextureInput("kt::depth");
  }

  void SkyboxPass::setup(RenderGraph& graph, const rhi::DescriptorLayout&) {
    resultIndex = graph.getImageIndex("kt::skybox");

    kt::rhi::PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::kt::skybox).addColorAttachment(kt::rhi::ImageFormat::R8G8B8A8_UNORM);

    auto pipeline_res = pipelineBuilder.build();
    if (!pipeline_res.has_value()) {
      KT_ABORT("Failed to create graphics pipeline: {}", pipeline_res.error());
    }
    pipeline = pipeline_res.value();
  }

  void SkyboxPass::execute(RenderGraph& graph, rhi::CommandBuffer& cmd, const rhi::DescriptorSet& set, glm::uvec2 framebufferSize) {
    auto& skyboxTex = graph.getImage(resultIndex);

    cmd.bindGraphicsPipeline(pipeline);
    cmd.setViewport({static_cast<float>(framebufferSize.x), static_cast<float>(framebufferSize.y)});
    cmd.setScissor({framebufferSize.x, framebufferSize.y});

    std::array<rhi::CommandBuffer::ColorAttachmentDesc, 1> colorAttachments = {
        rhi::CommandBuffer::ColorAttachmentDesc{.imageRef = skyboxTex, .loadOp = rhi::LoadOp::DontCare},
    };
    cmd.beginRendering(colorAttachments);

    cmd.bindGraphicsDescriptorSet(set);

    cmd.draw(3);

    cmd.endRendering();
  }

  void SkyboxPass::addToGraph(RenderGraphBuilder& graph, std::string src) {
    auto& pass = graph.addPass("kt::skybox");
    pass.setInterface(this);
    srcTexName = std::move(src);
  }
} // namespace kt