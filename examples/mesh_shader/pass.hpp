#pragma once

#include "keptech/graph/builder.hpp"
#include "keptech/graph/pass.hpp"
#include "keptech/passes/data.hpp"
#include "keptech/rhi/rhi.hpp"
#include "shaders/examples/mesh/mesh.h"
#include <keptech/buffers.hpp>
#include <keptech/core/kt-logger.hpp>
#include <keptech/graph/graph.hpp>
#include <keptech/graph/passInterface.hpp>
#include <keptech/rhi/pipeline.hpp>
#include <keptech/rhi/pipelineBuilder.hpp>

class MeshPass : public kt::RenderPassInterface {
public:
  MeshPass() = default;

  void setupDependencies(kt::RenderPassBuilder& self, kt::RenderGraphBuilder& graph) override {
    self.addColorOutput("color", {
                                     .sizeType = kt::AttachmentSize::SwapchainRelative,
                                     .format = kt::rhi::ImageFormat::R8G8B8A8_UNORM,
                                 });
    self.setDepthStencilOutput("depth", {
                                            .sizeType = kt::AttachmentSize::SwapchainRelative,
                                            .format = kt::rhi::ImageFormat::D32_FLOAT,
                                        });

    self.addUniformInput("kt::camera");
    self.addStorageReadOnlyInput("kt::objects", sizeof(kt::Object));
  }

  void setup(kt::RenderGraph& g, const kt::rhi::DescriptorLayout& layout) override {
    kt::rhi::PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::mesh)
        .addColorAttachment(kt::rhi::ImageFormat::R8G8B8A8_UNORM)
        .setDepthAttachment(kt::rhi::ImageFormat::D32_FLOAT)
        .setName("Mesh");

    auto pipeline_res = pipelineBuilder.build();
    if (!pipeline_res.has_value()) {
      KT_ABORT("Failed to create graphics pipeline: {}", pipeline_res.error());
    }
    pipeline = pipeline_res.value();

    colorImageIndex = g.getImageIndex("color");
    depthImageIndex = g.getImageIndex("depth");
    cameraIndex = g.getBufferIndex("kt::camera");

    writtenObjects = g.getUserData<kt::Vec<kt::Object>>("kt::data::writtenObjects");
  }

  void execute(kt::RenderGraph& g, kt::rhi::CommandBuffer& cmd, const kt::rhi::DescriptorSet& set, glm::uvec2 framebufferSize) override {
    auto& color = g.getImage(colorImageIndex);
    auto& depth = g.getImage(depthImageIndex);
    auto& camera = g.getBuffer(cameraIndex);

    auto& buffers = kt::Buffers::get();

    cmd.bindGraphicsPipeline(pipeline);
    cmd.bindGraphicsDescriptorSet(set, 1);
    cmd.bindGraphicsDescriptorSet(buffers.descriptor, 2);
    cmd.setViewport({static_cast<float>(framebufferSize.x), static_cast<float>(framebufferSize.y)});
    cmd.setScissor({framebufferSize.x, framebufferSize.y});

    kt::rhi::CommandBuffer::ColorAttachmentDesc colorAttachmentDesc{
        .imageRef = color,
        .loadOp = kt::rhi::LoadOp::Clear,
        .clearColor = {0.05f, 0.05f, 0.05f, 1.0f},
    };

    cmd.beginRendering({&colorAttachmentDesc, 1}, kt::rhi::CommandBuffer::DepthStencilAttachmentDesc{
                                                      .imageRef = depth,
                                                      .loadOp = kt::rhi::LoadOp::Clear,
                                                      .clearDepth = 1.0f,
                                                      .clearStencil = 0,
                                                  });

    uint32_t index = 0;
    for (auto& object : *writtenObjects) {
      cmd.writeGraphicsPushConstants(index++);

      uint32_t taskShaderDispatches = (object.submesh.meshletCount + 63) / 64;
      cmd.drawMeshTasks(taskShaderDispatches, 1, 1);
    }

    cmd.endRendering();
  }

  bool getClearColor(size_t attachmentIndex, kt::rhi::ColorClearValue* value) const override { return true; }

  bool getClearDepthStencil(kt::rhi::DepthClearValue* value) const override { return true; }

  void shutdown(kt::RenderGraph&) override {}

private:
  kt::rhi::Pipeline pipeline{};
  size_t colorImageIndex = 0;
  size_t depthImageIndex = 0;
  size_t objectsIndex = 0;
  size_t cameraIndex = 0;

  const kt::Vec<kt::Object>* writtenObjects = nullptr;
};