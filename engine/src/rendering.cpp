#include "keptech/core/components/renderObject.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/renderer.hpp"
#include "keptech/shaders/shader.h"
#include <keptech/core/kt-logger.hpp>
#include <keptech/core/rendering/commandBuffer.hpp>
#include <keptech/core/scene.hpp>

namespace keptech {

  void Renderer::render() {
#ifdef KT_ADD_RESOURCE_INFO
    m.triCount = 0;
    m.drawCallCount = 0;
#endif

    FrameData frame;

    auto graphicsCmdBuf_res = m.backend->startFrame();
    if (!graphicsCmdBuf_res) {
      KT_CRITICAL("Failed to create graphics command buffer: {}",
                  graphicsCmdBuf_res.error());
      return;
    }
    frame.graphicsCmdBuf = std::move(graphicsCmdBuf_res.value());

    if (!m.scene) {
      KT_WARN("No scene set for renderer, skipping render.");
      frame.graphicsCmdBuf->end();
      m.backend->endFrame(std::move(frame.graphicsCmdBuf));
      return;
    }

    {
      auto activeCameraEntity = m.scene->getActiveCamera();
      if (!activeCameraEntity
               .hasAllComponents<components::Camera, components::Transform>()) {
        KT_ERROR(
            "Active camera entity is missing Camera or Transform component.");
        frame.graphicsCmdBuf->end();
        m.backend->endFrame(std::move(frame.graphicsCmdBuf));
        return;
      }

      // Quickly blitz through and recalc all the transforms so we don't have to
      // worry about it later.
      m.scene->getEcs().view<components::Transform>().each(
          [](components::Transform& transform) {
            transform.recalculateGlobalTransform();
          });

      auto [camera, transform] =
          activeCameraEntity
              .getComponents<components::Camera, components::Transform>();

      frame.cameraData.camera = &camera;
      frame.cameraData.transform = &transform;

      glm::mat4 invViewMatrix = transform.getGlobal();
      glm::mat4 viewMatrix = glm::inverse(invViewMatrix);

      glm::mat4 projectionMatrix = camera.getProjectionMatrix();
      glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
      glm::mat4 invProjectionMatrix = glm::inverse(projectionMatrix);
      glm::mat4 invViewProjectionMatrix = invViewMatrix * invProjectionMatrix;

      glm::vec2 viewportSize = {camera.getViewport().width,
                                camera.getViewport().height};

      components::Camera::Uniforms uniforms{
          .projectionMatrix = projectionMatrix,
          .viewMatrix = viewMatrix,
          .viewProjectionMatrix = viewProjectionMatrix,
          .invProjectionMatrix = invProjectionMatrix,
          .invViewMatrix = invViewMatrix,
          .invViewProjectionMatrix = invViewProjectionMatrix,
          .viewportSize = viewportSize,
      };

      m.backend->writeCameraMatrices(frame.graphicsCmdBuf, uniforms);
    }

    drawDeferredPass(frame);
    drawLightingPass(frame);
    combineDeferredPass(frame);
    drawForwardPass(frame);
    drawTransparentPass(frame);
    drawUIPass(frame);

    m.backend->endFrame(std::move(frame.graphicsCmdBuf));
  }

  void Renderer::drawDeferredPass(const FrameData& frame) {
    m.backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = m.gBuffers.albedo.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = m.gBuffers.normal.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = m.gBuffers.emissiveAo.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = m.gBuffers.metallicRoughness.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = m.gBuffers.depth.get(),
            },
        });

    CommandBufferBeginRenderingInfo info{
        .renderAreaExtent =
            {
                m.gBuffers.albedo->getSize().x,
                m.gBuffers.albedo->getSize().y,
            },
        .colorAttachments = {{
                                 .texture = m.gBuffers.albedo.get(),
                                 .loadOp = AttachmentLoadOp::Clear,
                             },
                             {
                                 .texture = m.gBuffers.normal.get(),
                                 .loadOp = AttachmentLoadOp::Clear,
                             },
                             {
                                 .texture = m.gBuffers.emissiveAo.get(),
                                 .loadOp = AttachmentLoadOp::Clear,
                             },
                             {
                                 .texture = m.gBuffers.metallicRoughness.get(),
                                 .clearColor = glm::vec4(1.f, 1.f, 0.f, 1.f),
                                 .loadOp = AttachmentLoadOp::Clear,
                             }},
        .depthAttachment =
            {
                .texture = m.gBuffers.depth.get(),
                .clearDepth = 1.f,
                .loadOp = AttachmentLoadOp::Clear,
            },
    };
    frame.graphicsCmdBuf->beginRendering(info);
    frame.graphicsCmdBuf->setViewport(
        glm::vec2(0.0f, 0.0f), glm::vec2(m.gBuffers.albedo->getSize().x,
                                         m.gBuffers.albedo->getSize().y));
    frame.graphicsCmdBuf->setScissor(
        glm::ivec2(0, 0), glm::uvec2(m.gBuffers.albedo->getSize().x,
                                     m.gBuffers.albedo->getSize().y));

    auto view = m.scene->getEcs()
                    .view<components::Transform, components::Mesh,
                          components::Material>();

    size_t instanceOffset = 0;

    frame.graphicsCmdBuf->bindIndexBuffer(*m.buffers.index.get(), 0);
    frame.graphicsCmdBuf->bindVertexBuffer(0, {m.buffers.vertex.get()}, {0});

    IPipeline* lastPipeline = nullptr;

    for (auto [entity, transform, mesh, material] : view.each()) {
      if (!mesh || !material || !material->pipeline) {
        continue;
      }

      if (material->pipeline->getStage() != PipelineStage::Deferred) {
        continue;
      }

      if (material->pipeline.get() != lastPipeline) {
        frame.graphicsCmdBuf->bindPipeline(*material->pipeline);
        m.backend->bindGlobalDescriptorSets(
            frame.graphicsCmdBuf, *material->pipeline,
            Bitflag<shaders::ShaderStages>(shaders::ShaderStages::Vertex |
                                           shaders::ShaderStages::Fragment));
        lastPipeline = material->pipeline.get();
      }

      InstanceData instanceData{
          .modelMatrix = transform.getGlobal(),
      };

      memcpy(static_cast<uint8_t*>(m.buffers.instance->getMapping()) +
                 (instanceOffset * sizeof(InstanceData)),
             &instanceData, sizeof(InstanceData));

      DeferredPushConstantData pushConstantData{
          .instanceAddress = m.buffers.instance->getDeviceAddress(),
          .instanceOffset = static_cast<uint32_t>(instanceOffset),
          .materialAddress =
              m.buffers.material->getDeviceAddress() + material->offset,
      };

      frame.graphicsCmdBuf->writePushConstants(
          *material->pipeline,
          Bitflag<shaders::ShaderStages>(shaders::ShaderStages::Vertex |
                                         shaders::ShaderStages::Fragment),
          0, sizeof(DeferredPushConstantData), &pushConstantData);

      frame.graphicsCmdBuf->drawIndexed(
          mesh->getIndexCount(), 1, mesh->getIndexOffset(),
          static_cast<int32_t>(mesh->getVertexOffset()), 0);

#ifdef KT_ADD_RESOURCE_INFO
      m.triCount += mesh->getIndexCount() / 3;
      ++m.drawCallCount;
#endif

      ++instanceOffset;
    }

    frame.graphicsCmdBuf->endRendering();

    m.backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = m.gBuffers.albedo.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = m.gBuffers.normal.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = m.gBuffers.emissiveAo.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = m.gBuffers.metallicRoughness.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = m.gBuffers.depth.get(),
            },
        });
  }

  void Renderer::drawLightingPass(const FrameData& frame) {
    m.backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = m.lightingBuffers.diffuse.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = m.lightingBuffers.specular.get(),
            },
        });

    CommandBufferBeginRenderingInfo info{
        .renderAreaExtent =
            {
                m.lightingBuffers.diffuse->getSize().x,
                m.lightingBuffers.diffuse->getSize().y,
            },
        .colorAttachments =
            {
                {
                    .texture = m.lightingBuffers.diffuse.get(),
                    .loadOp = AttachmentLoadOp::Clear,
                },
                {
                    .texture = m.lightingBuffers.specular.get(),
                    .loadOp = AttachmentLoadOp::Clear,
                },
            },
    };
    frame.graphicsCmdBuf->beginRendering(info);
    frame.graphicsCmdBuf->setViewport(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(m.lightingBuffers.diffuse->getSize().x,
                  m.lightingBuffers.diffuse->getSize().y));
    frame.graphicsCmdBuf->setScissor(
        glm::ivec2(0, 0), glm::uvec2(m.lightingBuffers.diffuse->getSize().x,
                                     m.lightingBuffers.diffuse->getSize().y));

    drawPointLights(frame);

    frame.graphicsCmdBuf->endRendering();
    m.backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = m.lightingBuffers.diffuse.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = m.lightingBuffers.specular.get(),
            },
        });
  }

  void Renderer::drawPointLights(const FrameData& frame) {
    auto view =
        m.scene->getEcs().view<components::Transform, components::PointLight>();

    frame.graphicsCmdBuf->bindPipeline(*m.pipelines.pointLight);
    m.backend->bindGlobalDescriptorSets(
        frame.graphicsCmdBuf, *m.pipelines.pointLight,
        Bitflag<shaders::ShaderStages>(shaders::ShaderStages::Vertex |
                                       shaders::ShaderStages::Fragment));

    GBufferImageIndexData gBufferIndices{
        .albedoIndex = m.gBuffers.albedo->getIndex(),
        .normalIndex = m.gBuffers.normal->getIndex(),
        .emissiveAoIndex = m.gBuffers.emissiveAo->getIndex(),
        .metallicRoughnessIndex = m.gBuffers.metallicRoughness->getIndex(),
        .depthIndex = m.gBuffers.depth->getIndex(),
    };

    frame.graphicsCmdBuf->writePushConstants(
        *m.pipelines.pointLight,
        shaders::ShaderStages::Vertex | shaders::ShaderStages::Fragment,
        sizeof(PointLightPushConstantData), sizeof(GBufferImageIndexData),
        &gBufferIndices);

    for (auto [entity, transform, pointLight] : view.each()) {
      glm::vec4 position = transform.getGlobal()[3];
      position.w = pointLight.radius;
      PointLightPushConstantData pointLightData{
          .positionAndRadius = position,
          .colorAndIntensity =
              glm::vec4(pointLight.color, pointLight.intensity),
      };

      frame.graphicsCmdBuf->writePushConstants(
          *m.pipelines.pointLight,
          shaders::ShaderStages::Vertex | shaders::ShaderStages::Fragment, 0,
          sizeof(PointLightPushConstantData), &pointLightData);

      frame.graphicsCmdBuf->draw(36);
    }
  }

  void Renderer::combineDeferredPass(const FrameData& frame) {
    m.backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {TextureTransition{
            .type = TextureTransitionType::UndefinedToRenderable,
            .texture = m.lightCombinedBuffer.get(),
        }});

    CommandBufferBeginRenderingInfo info{
        .renderAreaExtent =
            {
                m.lightCombinedBuffer->getSize().x,
                m.lightCombinedBuffer->getSize().y,
            },
        .colorAttachments = {{
            .texture = m.lightCombinedBuffer.get(),
            .loadOp = AttachmentLoadOp::DontCare,
        }},
    };

    frame.graphicsCmdBuf->beginRendering(info);
    frame.graphicsCmdBuf->setViewport(
        glm::vec2(0.0f, 0.0f), glm::vec2(m.lightCombinedBuffer->getSize().x,
                                         m.lightCombinedBuffer->getSize().y));
    frame.graphicsCmdBuf->setScissor(
        glm::ivec2(0, 0), glm::uvec2(m.lightCombinedBuffer->getSize().x,
                                     m.lightCombinedBuffer->getSize().y));

    frame.graphicsCmdBuf->bindPipeline(*m.pipelines.combineDeferred);
    m.backend->bindGlobalDescriptorSets(
        frame.graphicsCmdBuf, *m.pipelines.combineDeferred,
        Bitflag<shaders::ShaderStages>(shaders::ShaderStages::Fragment));

    GBufferImageIndexData gBufferIndices{
        .albedoIndex = m.gBuffers.albedo->getIndex(),
        .normalIndex = m.gBuffers.normal->getIndex(),
        .emissiveAoIndex = m.gBuffers.emissiveAo->getIndex(),
        .metallicRoughnessIndex = m.gBuffers.metallicRoughness->getIndex(),
        .depthIndex = m.gBuffers.depth->getIndex(),
    };

    LightBufferImageIndexData lData{
        .diffuseIndex = m.lightingBuffers.diffuse->getIndex(),
        .specularIndex = m.lightingBuffers.specular->getIndex(),
    };

    struct CombinePushConstantData {
      GBufferImageIndexData gBufferIndices;
      LightBufferImageIndexData lightBufferIndices;
    } pushConstantData{
        .gBufferIndices = gBufferIndices,
        .lightBufferIndices = lData,
    };

    frame.graphicsCmdBuf->writePushConstants(
        *m.pipelines.combineDeferred, shaders::ShaderStages::Fragment, 0,
        sizeof(CombinePushConstantData), &pushConstantData);

    frame.graphicsCmdBuf->draw(3);

    frame.graphicsCmdBuf->endRendering();

    m.backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {TextureTransition{
            .type = TextureTransitionType::RenderableToShaderRead,
            .texture = m.lightCombinedBuffer.get(),
        }});
  }

  void Renderer::drawForwardPass(const FrameData& frame) {}

  void Renderer::drawTransparentPass(const FrameData& frame) {}

  void Renderer::drawUIPass(const FrameData& frame) {
    m.backend->renderImGui(frame.graphicsCmdBuf);
  }
} // namespace keptech
