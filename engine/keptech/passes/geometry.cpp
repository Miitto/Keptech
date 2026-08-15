#include "geometry.hpp"

#include "graph/graph.hpp"
#include "keptech/buffers.hpp"
#include "keptech/components/camera.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/graph/builder.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/pipelineBuilder.hpp"
#include "keptech/rhi/wrappers/cmdBuf.hpp"
#include "shaders/keptech/geometry.h"

namespace kt {

  void GeometryPass::setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) {
    self.addColorOutput("kt::albedo", {.format = rhi::ImageFormat::R8G8B8A8_UNORM});
    self.addColorOutput("kt::normal", {.format = rhi::ImageFormat::R11G11B10_FLOAT});
    self.addColorOutput("kt::material", {.format = rhi::ImageFormat::R8G8_UNORM});
    self.addColorOutput("kt::emissive", {.format = rhi::ImageFormat::R16G16B16A16_FLOAT});
    self.setDepthStencilOutput("kt::depth", {.format = rhi::ImageFormat::D32_FLOAT});
  }

  void GeometryPass::setup(RenderGraph& graph) {
    albedoIndex = graph.getImageIndex("kt::albedo");
    normalIndex = graph.getImageIndex("kt::normal");
    materialIndex = graph.getImageIndex("kt::material");
    emissiveIndex = graph.getImageIndex("kt::emissive");
    depthIndex = graph.getImageIndex("kt::depth");

    kt::rhi::PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::kt::geometry)
        .addColorAttachment(kt::rhi::ImageFormat::R8G8B8A8_UNORM)
        .addColorAttachment(kt::rhi::ImageFormat::R11G11B10_FLOAT)
        .addColorAttachment(kt::rhi::ImageFormat::R8G8_UNORM)
        .addColorAttachment(kt::rhi::ImageFormat::R16G16B16A16_FLOAT)
        .setDepthAttachment(kt::rhi::ImageFormat::D32_FLOAT);

    auto pipeline_res = pipelineBuilder.build();
    if (!pipeline_res.has_value()) {
      KT_ABORT("Failed to create graphics pipeline: {}", pipeline_res.error());
    }
    pipeline = pipeline_res.value();

    KT_DEBUG("Geometry pass setup");
  }

  void GeometryPass::prepare(RenderGraph& graph) {}

  void GeometryPass::execute(RenderGraph& graph, rhi::CommandBuffer& cmd, glm::uvec2 framebufferSize) {
    auto& albedo = graph.getImage(albedoIndex);
    auto& normal = graph.getImage(normalIndex);
    auto& material = graph.getImage(materialIndex);
    auto& emissive = graph.getImage(emissiveIndex);
    auto& depth = graph.getImage(depthIndex);

    cmd.bindGraphicsPipeline(pipeline);
    cmd.setViewport({static_cast<float>(framebufferSize.x), static_cast<float>(framebufferSize.y)});
    cmd.setScissor({framebufferSize.x, framebufferSize.y});

    std::array<rhi::CommandBuffer::ColorAttachmentDesc, 4> colorAttachments = {
        rhi::CommandBuffer::ColorAttachmentDesc{.imageRef = albedo, .loadOp = rhi::LoadOp::Clear, .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}},
        rhi::CommandBuffer::ColorAttachmentDesc{.imageRef = normal, .loadOp = rhi::LoadOp::Clear, .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}},
        rhi::CommandBuffer::ColorAttachmentDesc{.imageRef = material, .loadOp = rhi::LoadOp::Clear, .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}},
        rhi::CommandBuffer::ColorAttachmentDesc{.imageRef = emissive, .loadOp = rhi::LoadOp::Clear, .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}},
    };
    cmd.beginRendering(colorAttachments, rhi::CommandBuffer::DepthStencilAttachmentDesc{
                                             .imageRef = depth, .loadOp = rhi::LoadOp::Clear, .clearDepth = 1.0f, .clearStencil = 0});

    auto& buffers = kt::Buffers::get();
    std::array<rhi::CommandBuffer::VertexBufferBinding, 2> vertexBindings = {
        rhi::CommandBuffer::VertexBufferBinding{.buffer = buffers.positions, .stride = sizeof(glm::vec3), .offset = 0},
        rhi::CommandBuffer::VertexBufferBinding{.buffer = buffers.vertexAttribs, .stride = sizeof(kt::VertexAttribs), .offset = 0},
    };
    cmd.bindVertexBuffers(0, vertexBindings);
    cmd.bindIndexBuffer(buffers.indices);

    auto sceneView = kt::Scene::active().view<kt::Mesh, kt::components::Transform>();

    auto camera = kt::Scene::active().getActiveCamera();

    auto [camT, cam] = camera.getComponents<kt::components::Transform, kt::components::Camera>();

    glm::mat4 viewMatrix = glm::inverse(camT.getGlobal());
    cam.recalculateProjectionMatrix();
    glm::mat4 projectionMatrix = cam.getProjectionMatrix();

    auto vpMatrix = projectionMatrix * viewMatrix;

    for (const auto& [entity, mesh, transform] : sceneView.each()) {
      auto modelMatrix = transform.getGlobal();
      auto mvpMatrix = vpMatrix * modelMatrix;
      cmd.writePushConstants<glm::mat4>(mvpMatrix);
      for (auto& submesh : mesh.getSubmeshes()) {
        cmd.drawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset, 0);
      }
    }

    cmd.endRendering();
  }

  void GeometryPass::addToGraph(RenderGraphBuilder& graph) {
    auto& pass = graph.addPass("kt::geometry");
    pass.setInterface(this);
  }
} // namespace kt