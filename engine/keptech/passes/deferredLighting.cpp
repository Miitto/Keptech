#include "deferredLighting.hpp"

#include "graph/graph.hpp"
#include "keptech/buffers.hpp"
#include "keptech/components/pointLight.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/graph/builder.hpp"
#include "keptech/maths/intersection.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/drawCommands.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/pipelineBuilder.hpp"
#include "shaders/keptech/pointLight.h"

namespace kt {

  void DeferredLightingPass::setupDependencies(RenderPassBuilder& self, RenderGraphBuilder&) {
    self.addColorOutput("kt::diffuse", {.format = rhi::ImageFormat::R11G11B10_FLOAT});
    self.addColorOutput("kt::specular", {.format = rhi::ImageFormat::R11G11B10_FLOAT});

    self.addUniformInput("kt::camera");

    self.addMappedBuffer("kt::lights", sizeof(GpuLight) * 100);
    self.addStorageReadOnlyInput("kt::lights");

    self.addTextureInput("kt::albedo");
    self.addTextureInput("kt::normal");
    self.addTextureInput("kt::material");
    self.addTextureInput("kt::emissive");
    self.addTextureInput("kt::depth");
  }

  bool DeferredLightingPass::validate(RenderPassBuilder&, RenderGraphBuilder& graph) {
    if (!graph.hasBufferResource("kt::camera")) {
      KT_ERROR("Deferred lighting pass requires a uniform buffer resource named 'kt::camera'. Either add kt::DataPass or manage the camera "
               "buffer resource "
               "manually.");
      return false;
    }

    if (!graph.hasTextureResource("kt::albedo")) {
      KT_ERROR("Deferred lighting pass requires a texture resource named 'kt::albedo'. Either add kt::GeometryPass or manage the albedo "
               "texture resource "
               "manually.");
      return false;
    }
    if (!graph.hasTextureResource("kt::normal")) {
      KT_ERROR("Deferred lighting pass requires a texture resource named 'kt::normal'. Either add kt::GeometryPass or manage the normal "
               "texture resource "
               "manually.");
      return false;
    }
    if (!graph.hasTextureResource("kt::material")) {
      KT_ERROR("Deferred lighting pass requires a texture resource named 'kt::material'. Either add kt::GeometryPass or manage the "
               "material texture resource "
               "manually.");
      return false;
    }
    if (!graph.hasTextureResource("kt::emissive")) {
      KT_ERROR("Deferred lighting pass requires a texture resource named 'kt::emissive'. Either add kt::GeometryPass or manage the "
               "emissive texture resource "
               "manually.");
      return false;
    }
    if (!graph.hasTextureResource("kt::depth")) {
      KT_ERROR("Deferred lighting pass requires a texture resource named 'kt::depth'. Either add kt::GeometryPass or manage the depth "
               "texture resource "
               "manually.");
      return false;
    }

    return true;
  }

  void DeferredLightingPass::setup(RenderGraph& graph) {
    diffuseIndex = graph.getImageIndex("kt::diffuse");
    specularIndex = graph.getImageIndex("kt::specular");

    kt::rhi::PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::kt::pointLight)
        .addColorAttachment(kt::rhi::ImageFormat::R11G11B10_FLOAT)
        .addColorAttachment(kt::rhi::ImageFormat::R11G11B10_FLOAT)
        .setDepthCompareOp(rhi::DepthCompareOp::Greater);

    auto pipeline_res = pipelineBuilder.build();
    if (!pipeline_res.has_value()) {
      KT_ABORT("Failed to create graphics pipeline: {}", pipeline_res.error());
    }
    pipeline = pipeline_res.value();

    cameraFrustum = graph.getUserData<maths::Frustum>("kt::data::cameraFrustum");

    KT_ASSERT(cameraFrustum != nullptr,
              "Deferred lighting pass requires a pointer to a camera frustum. Make sure that the DataPass is added "
              "to the render graph before the DeferredLightingPass, or that \"kt::data::cameraFrustum\" is set in the "
              "render graph user data before the DeferredLightingPass was set up.");

    KT_DEBUG("Deferred lighting pass setup");
  }

  void DeferredLightingPass::prepare(RenderGraph& graph) {
    auto frustum = *cameraFrustum;

    std::vector<GpuLight> lights;

    auto view = Scene::active().view<components::PointLight, components::Transform>();

    for (const auto& [entity, pointLight, transform] : view.each()) {
      glm::vec3 position = transform.getGlobal()[3];
      if (frustum.intersects(maths::Sphere{.center = position, .radius = pointLight.radius}) != maths::IntersectionType::eNone) {
        lights.push_back({.position = position, .radius = pointLight.radius, .color = pointLight.color, .intensity = pointLight.intensity});
      }
    }

    size_t lightBufferSize = lights.size() * sizeof(GpuLight);
    {
      auto& lightBuffer = graph.getFrameBuffer(graph.getBufferIndex("kt::lights"));
      if (lightBuffer.size() < lightBufferSize) {
        graph.reallocateBuffer(graph.getBufferIndex("kt::lights"), lightBufferSize, false);
      }
    }
    auto& lightBuffer = graph.getFrameBuffer(graph.getBufferIndex("kt::lights"));
    memcpy(lightBuffer.mapping(), lights.data(), lightBufferSize);

    lightCount = lights.size();
  }

  void DeferredLightingPass::execute(RenderGraph& graph, rhi::CommandBuffer& cmd, rhi::DescriptorSet& set, glm::uvec2 framebufferSize) {

    auto& diffuse = graph.getImage(diffuseIndex);
    auto& specular = graph.getImage(specularIndex);
    if (lightCount == 0) {
      cmd.clearColorImage(diffuse, {0.0f, 0.0f, 0.0f, 1.0f});
      cmd.clearColorImage(specular, {0.0f, 0.0f, 0.0f, 1.0f});
      return;
    }

    cmd.bindGraphicsPipeline(pipeline);
    cmd.setViewport({static_cast<float>(framebufferSize.x), static_cast<float>(framebufferSize.y)});
    cmd.setScissor({framebufferSize.x, framebufferSize.y});

    std::array<rhi::CommandBuffer::ColorAttachmentDesc, 4> colorAttachments = {
        rhi::CommandBuffer::ColorAttachmentDesc{.imageRef = diffuse, .loadOp = rhi::LoadOp::Clear, .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}},
        rhi::CommandBuffer::ColorAttachmentDesc{.imageRef = specular, .loadOp = rhi::LoadOp::Clear, .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}},
    };
    cmd.beginRendering(colorAttachments);

    cmd.bindGraphicsDescriptorSet(set);

    for (uint32_t i = 0; i < lightCount; ++i) {
      cmd.draw(36, 1, 0, i);
    }

    cmd.endRendering();
  }

  void DeferredLightingPass::addToGraph(RenderGraphBuilder& graph) {
    auto& pass = graph.addPass("kt::geometry");
    pass.setInterface(this);
  }
} // namespace kt