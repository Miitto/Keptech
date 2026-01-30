#include "keptech/core/components/renderObject.hpp"
#include "keptech/renderer.hpp"
#include <keptech/core/kt-logger.hpp>
#include <keptech/core/rendering/commandBuffer.hpp>
#include <keptech/core/scene.hpp>

namespace keptech {

  void Renderer::render() {
#ifdef KT_ADD_RESOURCE_INFO
    triCount = 0;
    drawCallCount = 0;
#endif

    FrameData frame;

    auto graphicsCmdBuf_res = backend->startFrame();
    if (!graphicsCmdBuf_res) {
      KT_CRITICAL("Failed to create graphics command buffer: {}",
                  graphicsCmdBuf_res.error());
      return;
    }
    frame.graphicsCmdBuf = std::move(graphicsCmdBuf_res.value());

    if (!scene) {
      KT_WARN("No scene set for renderer, skipping render.");
      frame.graphicsCmdBuf->end();
      backend->endFrame(std::move(frame.graphicsCmdBuf));
      return;
    }

    {
      auto activeCameraEntity = scene->getActiveCamera();
      if (!activeCameraEntity
               .hasAllComponents<components::Camera, components::Transform>()) {
        KT_ERROR(
            "Active camera entity is missing Camera or Transform component.");
        frame.graphicsCmdBuf->end();
        backend->endFrame(std::move(frame.graphicsCmdBuf));
        return;
      }

      auto [camera, transform] =
          activeCameraEntity
              .getComponents<components::Camera, components::Transform>();

      transform.recalculateGlobalTransform();

      frame.cameraData.camera = &camera;
      frame.cameraData.transform = &transform;

      glm::mat4 invViewMatrix = transform.getGlobal().toMatrix(true);
      auto cPos = transform.getGlobal().pos();
      glm::mat4 viewMatrix = glm::inverse(invViewMatrix);

      glm::mat4 projectionMatrix = camera.getProjectionMatrix();
      glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
      glm::mat4 invProjectionMatrix = glm::inverse(projectionMatrix);
      glm::mat4 invViewProjectionMatrix = invViewMatrix * invProjectionMatrix;

      components::Camera::Uniforms uniforms{
          .projectionMatrix = projectionMatrix,
          .viewMatrix = viewMatrix,
          .viewProjectionMatrix = viewProjectionMatrix,
          .invProjectionMatrix = invProjectionMatrix,
          .invViewMatrix = invViewMatrix,
          .invViewProjectionMatrix = invViewProjectionMatrix,
      };

      memcpy(buffers.cameraStaging->getMapping(), &uniforms,
             sizeof(components::Camera::Uniforms));

      backend->writeCameraMatrices(frame.graphicsCmdBuf, buffers.cameraStaging);
    }

    drawDeferredPass(frame);
    drawLightingPass(frame);
    combineDeferredPass(frame);
    drawForwardPass(frame);
    drawTransparentPass(frame);
    drawUIPass(frame);

    backend->endFrame(std::move(frame.graphicsCmdBuf));
  }

  void Renderer::drawDeferredPass(const FrameData& frame) {
    backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = gBuffers.albedo.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = gBuffers.normal.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = gBuffers.depth.get(),
            },
        });

    CommandBufferBeginRenderingInfo info{
        .renderAreaExtent =
            {
                gBuffers.albedo->getSize().x,
                gBuffers.albedo->getSize().y,
            },
        .colorAttachments =
            {
                {
                    .texture = gBuffers.albedo.get(),
                    .loadOp = AttachmentLoadOp::Clear,
                },
                {
                    .texture = gBuffers.normal.get(),
                    .loadOp = AttachmentLoadOp::Clear,
                },
            },
        .depthAttachment =
            {
                .texture = gBuffers.depth.get(),
                .clearDepth = 1.f,
                .loadOp = AttachmentLoadOp::Clear,
            },
    };
    frame.graphicsCmdBuf->beginRendering(info);
    frame.graphicsCmdBuf->setViewport(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(gBuffers.albedo->getSize().x, gBuffers.albedo->getSize().y));
    frame.graphicsCmdBuf->setScissor(
        glm::ivec2(0, 0),
        glm::uvec2(gBuffers.albedo->getSize().x, gBuffers.albedo->getSize().y));

    auto view = scene->getEcs()
                    .view<components::Transform, components::Mesh,
                          components::Material>();

    size_t instanceOffset = 0;
    for (auto [entity, transform, mesh, material] : view.each()) {
      if (!mesh || !material || !material->pipeline) {
        continue;
      }

      if (material->pipeline->getStage() != PipelineStage::Deferred) {
        continue;
      }

      transform.recalculateGlobalTransform();

      frame.graphicsCmdBuf->bindPipeline(*material->pipeline);

      frame.graphicsCmdBuf->bindIndexBuffer(*buffers.index.get(), 0);
      frame.graphicsCmdBuf->bindVertexBuffer(0, {buffers.vertex.get()}, {0});

      uint32_t albedoTexIndex = ~0u;
      uint32_t normalTexIndex = ~0u;

      if (material->instanceData.size() >= 1) {
        auto& albedoData = std::get<TexPtr>(material->instanceData[0]);
        if (albedoData != nullptr) {
          albedoTexIndex = albedoData->getIndex();
        }
      }
      if (material->instanceData.size() >= 2) {
        auto& normalData = std::get<TexPtr>(material->instanceData[1]);
        if (normalData != nullptr) {
          normalTexIndex = normalData->getIndex();
        }
      }

      InstanceData instanceData{
          .modelMatrix = transform.getGlobal().toMatrix(),
          .albedoTextureIndex = albedoTexIndex,
          .normalTextureIndex = normalTexIndex,
      };

      memcpy(static_cast<uint8_t*>(buffers.instance->getMapping()) +
                 (instanceOffset * sizeof(InstanceData)),
             &instanceData, sizeof(InstanceData));

      struct PushConstantData {
        uint64_t instanceAddress;
        uint32_t instanceOffset;
      } pushConstantData{
          .instanceAddress = buffers.instance->getDeviceAddress(),
          .instanceOffset = static_cast<uint32_t>(instanceOffset),
      };

      frame.graphicsCmdBuf->writePushConstants(
          *material->pipeline,
          Bitflag<shaders::ShaderStages>(shaders::ShaderStages::Vertex), 0,
          sizeof(PushConstantData), &pushConstantData);

      backend->bindGlobalDescriptorSets(
          frame.graphicsCmdBuf, *material->pipeline,
          Bitflag<shaders::ShaderStages>(shaders::ShaderStages::Vertex |
                                         shaders::ShaderStages::Fragment));

      frame.graphicsCmdBuf->drawIndexed(
          mesh->getIndexCount(), 1, mesh->getIndexOffset(),
          static_cast<int32_t>(mesh->getVertexOffset()), 0);

#ifdef KT_ADD_RESOURCE_INFO
      triCount += mesh->getIndexCount() / 3;
      ++drawCallCount;
#endif

      ++instanceOffset;
    }

    frame.graphicsCmdBuf->endRendering();

    backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = gBuffers.albedo.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = gBuffers.normal.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = gBuffers.depth.get(),
            },
        });
  }

  void Renderer::drawLightingPass(const FrameData& frame) {}

  void Renderer::combineDeferredPass(const FrameData& frame) {}

  void Renderer::drawForwardPass(const FrameData& frame) {}

  void Renderer::drawTransparentPass(const FrameData& frame) {}

  void Renderer::drawUIPass(const FrameData& frame) {
    backend->renderImGui(frame.graphicsCmdBuf);
  }
} // namespace keptech
