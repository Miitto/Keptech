#include "geometry.hpp"

#include "graph/graph.hpp"
#include "keptech/buffers.hpp"
#include "keptech/graph/builder.hpp"
#include "keptech/maths/intersection.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/drawCommands.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/pipelineBuilder.hpp"
#include "shaders/keptech/geometry.h"

namespace kt {

  void GeometryPass::setupDependencies(RenderPassBuilder& self, RenderGraphBuilder&) {
    self.addColorOutput("kt::albedo", {.format = rhi::ImageFormat::R8G8B8A8_UNORM});
    self.addColorOutput("kt::normal", {.format = rhi::ImageFormat::R11G11B10_FLOAT});
    self.addColorOutput("kt::material", {.format = rhi::ImageFormat::R8G8_UNORM});
    self.addColorOutput("kt::emissive", {.format = rhi::ImageFormat::R11G11B10_FLOAT});
    self.setDepthStencilOutput("kt::depth", {.format = rhi::ImageFormat::D32_FLOAT});

    self.addUniformInput("kt::camera");
    self.addStorageReadOnlyInput("kt::objects", sizeof(Object));

    self.addMappedBuffer("kt::geom::drawCommands", sizeof(kt::rhi::DrawIndexedCommand) * 1000);
    self.addIndirectBufferInput("kt::geom::drawCommands");
  }

  bool GeometryPass::validate(RenderPassBuilder&, RenderGraphBuilder& graph) {
    if (!graph.hasBufferResource("kt::camera")) {
      KT_ERROR("Geometry pass requires a buffer resource named 'kt::camera'. Either add kt::DataPass or manage the camera buffer resource "
               "manually.");
      return false;
    }
    if (!graph.hasBufferResource("kt::objects")) {
      KT_ERROR("Geometry pass requires a buffer resource named 'kt::objects'. Either add kt::DataPass or manage the objects buffer "
               "resource manually.");
      return false;
    }

    return true;
  }

  void GeometryPass::setup(RenderGraph& graph, const rhi::DescriptorLayout&) {
    albedoIndex = graph.getImageIndex("kt::albedo");
    normalIndex = graph.getImageIndex("kt::normal");
    materialIndex = graph.getImageIndex("kt::material");
    emissiveIndex = graph.getImageIndex("kt::emissive");
    depthIndex = graph.getImageIndex("kt::depth");
    drawCommandsIndex = graph.getBufferIndex("kt::geom::drawCommands");
    cameraIndex = graph.getBufferIndex("kt::camera");
    objectsIndex = graph.getBufferIndex("kt::objects");

    kt::rhi::PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::kt::geometry)
        .addColorAttachment(kt::rhi::ImageFormat::R8G8B8A8_UNORM)
        .addColorAttachment(kt::rhi::ImageFormat::R11G11B10_FLOAT)
        .addColorAttachment(kt::rhi::ImageFormat::R8G8_UNORM)
        .addColorAttachment(kt::rhi::ImageFormat::R11G11B10_FLOAT)
        .setDepthAttachment(kt::rhi::ImageFormat::D32_FLOAT);

    auto pipeline_res = pipelineBuilder.build();
    if (!pipeline_res.has_value()) {
      KT_ABORT("Failed to create graphics pipeline: {}", pipeline_res.error());
    }
    pipeline = pipeline_res.value();

    writtenObjects = graph.getUserData<std::vector<Object>>("kt::data::writtenObjects");
    cameraFrustum = graph.getUserData<maths::Frustum>("kt::data::cameraFrustum");

    KT_ASSERT(writtenObjects != nullptr,
              "Geometry pass requires a pointer to a vector of objects. Make sure that the DataPass is added "
              "to the render graph before the GeometryPass, or that \"kt::data::writtenObjects\" is set in the render graph user data "
              "before the GeometryPass was set up.");

    KT_ASSERT(cameraFrustum != nullptr, "Geometry pass requires a pointer to a camera frustum. Make sure that the DataPass is added "
                                        "to the render graph before the GeometryPass, or that \"kt::data::cameraFrustum\" is set in the "
                                        "render graph user data before the GeometryPass was set up.");

    KT_DEBUG("Geometry pass setup");
  }

  void GeometryPass::prepare(RenderGraph& graph) {
    auto frustum = *cameraFrustum;

    std::vector<kt::rhi::DrawIndexedCommand> drawCommands;
    drawCommands.reserve(writtenObjects->size());
    for (uint32_t i = 0; i < writtenObjects->size(); ++i) {
      auto& obj = (*writtenObjects)[i];
      if (frustum.intersects(obj.submesh.boundingSphere) != maths::IntersectionType::eNone) {
        drawCommands.emplace_back(obj.submesh.indexCount, 1, obj.submesh.indexOffset, obj.submesh.vertexOffset, i);
      }
    }

    size_t drawCommandsBufferSize = drawCommands.size() * sizeof(kt::rhi::DrawIndexedCommand);
    {
      auto& drawCommandsBuffer = graph.getFrameBuffer(drawCommandsIndex);
      if (drawCommandsBuffer.size() < drawCommandsBufferSize) {
        graph.reallocateBuffer(drawCommandsIndex, drawCommandsBufferSize, false);
      }
    }
    auto& drawCommandsBuffer = graph.getFrameBuffer(drawCommandsIndex);
    memcpy(drawCommandsBuffer.mapping(), drawCommands.data(), drawCommandsBufferSize);

    drawCommandCount = static_cast<uint32_t>(drawCommands.size());
  }

  void GeometryPass::execute(RenderGraph& graph, rhi::CommandBuffer& cmd, const rhi::DescriptorSet& set, glm::uvec2 framebufferSize) {
    auto& albedo = graph.getImage(albedoIndex);
    auto& normal = graph.getImage(normalIndex);
    auto& material = graph.getImage(materialIndex);
    auto& emissive = graph.getImage(emissiveIndex);
    auto& depth = graph.getImage(depthIndex);

    cmd.bindGraphicsPipeline(pipeline);
    cmd.bindGraphicsDescriptorSet(set);
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

    cmd.drawIndexedIndirect(graph.getFrameBuffer(drawCommandsIndex), drawCommandCount);

    cmd.endRendering();
  }

  void GeometryPass::addToGraph(RenderGraphBuilder& graph) {
    auto& pass = graph.addPass("kt::geometry");
    pass.setInterface(this);
  }
} // namespace kt