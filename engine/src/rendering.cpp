#include "keptech/core/components/renderObject.hpp"
#include "keptech/renderer.hpp"
#include "keptech/shaders/shader.h"
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

      // Quickly blitz through and recalc all the transforms so we don't have to
      // worry about it later.
      scene->getEcs().view<components::Transform>().each(
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
                .texture = gBuffers.emissiveAo.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = gBuffers.metallicRoughness.get(),
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
        .colorAttachments = {{
                                 .texture = gBuffers.albedo.get(),
                                 .loadOp = AttachmentLoadOp::Clear,
                             },
                             {
                                 .texture = gBuffers.normal.get(),
                                 .loadOp = AttachmentLoadOp::Clear,
                             },
                             {
                                 .texture = gBuffers.emissiveAo.get(),
                                 .loadOp = AttachmentLoadOp::Clear,
                             },
                             {
                                 .texture = gBuffers.metallicRoughness.get(),
                                 .loadOp = AttachmentLoadOp::Clear,
                             }},
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

    frame.graphicsCmdBuf->bindIndexBuffer(*buffers.index.get(), 0);
    frame.graphicsCmdBuf->bindVertexBuffer(0, {buffers.vertex.get()}, {0});

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
        backend->bindGlobalDescriptorSets(
            frame.graphicsCmdBuf, *material->pipeline,
            Bitflag<shaders::ShaderStages>(shaders::ShaderStages::Vertex |
                                           shaders::ShaderStages::Fragment));
        lastPipeline = material->pipeline.get();
      }

      InstanceData instanceData{
          .modelMatrix = transform.getGlobal(),
      };

      memcpy(static_cast<uint8_t*>(buffers.instance->getMapping()) +
                 (instanceOffset * sizeof(InstanceData)),
             &instanceData, sizeof(InstanceData));

      DeferredPushConstantData pushConstantData{
          .instanceAddress = buffers.instance->getDeviceAddress(),
          .instanceOffset = static_cast<uint32_t>(instanceOffset),
          .materialAddress =
              buffers.material->getDeviceAddress() + material->offset,
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
                .texture = gBuffers.emissiveAo.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = gBuffers.metallicRoughness.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = gBuffers.depth.get(),
            },
        });
  }

  void Renderer::drawLightingPass(const FrameData& frame) {
    backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = lightingBuffers.diffuse.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::UndefinedToRenderable,
                .texture = lightingBuffers.specular.get(),
            },
        });

    CommandBufferBeginRenderingInfo info{
        .renderAreaExtent =
            {
                lightingBuffers.diffuse->getSize().x,
                lightingBuffers.diffuse->getSize().y,
            },
        .colorAttachments =
            {
                {
                    .texture = lightingBuffers.diffuse.get(),
                    .loadOp = AttachmentLoadOp::Clear,
                },
                {
                    .texture = lightingBuffers.specular.get(),
                    .loadOp = AttachmentLoadOp::Clear,
                },
            },
    };
    frame.graphicsCmdBuf->beginRendering(info);
    frame.graphicsCmdBuf->setViewport(
        glm::vec2(0.0f, 0.0f), glm::vec2(lightingBuffers.diffuse->getSize().x,
                                         lightingBuffers.diffuse->getSize().y));
    frame.graphicsCmdBuf->setScissor(
        glm::ivec2(0, 0), glm::uvec2(lightingBuffers.diffuse->getSize().x,
                                     lightingBuffers.diffuse->getSize().y));

    drawPointLights(frame);

    frame.graphicsCmdBuf->endRendering();
    backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = lightingBuffers.diffuse.get(),
            },
            TextureTransition{
                .type = TextureTransitionType::RenderableToShaderRead,
                .texture = lightingBuffers.specular.get(),
            },
        });
  }

  void Renderer::drawPointLights(const FrameData& frame) {
    auto view =
        scene->getEcs().view<components::Transform, components::PointLight>();

    frame.graphicsCmdBuf->bindPipeline(*pipelines.pointLight);
    backend->bindGlobalDescriptorSets(
        frame.graphicsCmdBuf, *pipelines.pointLight,
        Bitflag<shaders::ShaderStages>(shaders::ShaderStages::Vertex |
                                       shaders::ShaderStages::Fragment));

    GBufferImageIndexData gBufferIndices{
        .albedoIndex = gBuffers.albedo->getIndex(),
        .normalIndex = gBuffers.normal->getIndex(),
        .emissiveAoIndex = gBuffers.emissiveAo->getIndex(),
        .metallicRoughnessIndex = gBuffers.metallicRoughness->getIndex(),
        .depthIndex = gBuffers.depth->getIndex(),
    };

    frame.graphicsCmdBuf->writePushConstants(
        *pipelines.pointLight,
        shaders::ShaderStages::Vertex | shaders::ShaderStages::Fragment,
        sizeof(PointLightPushConstantData), sizeof(GBufferImageIndexData),
        &gBufferIndices);

    for (auto [entity, transform, pointLight] : view.each()) {
      glm::vec4 position = transform.getGlobal()[3];
      position.w = pointLight.radius;
      struct PointLightData {
        glm::vec4 positionAndRadius;
        glm::vec4 colorAndIntensity;
      } pointLightData{
          .positionAndRadius = position,
          .colorAndIntensity =
              glm::vec4(pointLight.color, pointLight.intensity),
      };

      frame.graphicsCmdBuf->writePushConstants(
          *pipelines.pointLight,
          shaders::ShaderStages::Vertex | shaders::ShaderStages::Fragment, 0,
          sizeof(PointLightData), &pointLightData);

      frame.graphicsCmdBuf->draw(36);
    }
  }

  void Renderer::combineDeferredPass(const FrameData& frame) {
    backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {TextureTransition{
            .type = TextureTransitionType::UndefinedToRenderable,
            .texture = lightCombinedBuffer.get(),
        }});

    CommandBufferBeginRenderingInfo info{
        .renderAreaExtent =
            {
                lightCombinedBuffer->getSize().x,
                lightCombinedBuffer->getSize().y,
            },
        .colorAttachments = {{
            .texture = lightCombinedBuffer.get(),
            .loadOp = AttachmentLoadOp::DontCare,
        }},
    };

    frame.graphicsCmdBuf->beginRendering(info);
    frame.graphicsCmdBuf->setViewport(
        glm::vec2(0.0f, 0.0f), glm::vec2(lightCombinedBuffer->getSize().x,
                                         lightCombinedBuffer->getSize().y));
    frame.graphicsCmdBuf->setScissor(
        glm::ivec2(0, 0), glm::uvec2(lightCombinedBuffer->getSize().x,
                                     lightCombinedBuffer->getSize().y));

    frame.graphicsCmdBuf->bindPipeline(*pipelines.combineDeferred);
    backend->bindGlobalDescriptorSets(
        frame.graphicsCmdBuf, *pipelines.combineDeferred,
        Bitflag<shaders::ShaderStages>(shaders::ShaderStages::Fragment));

    GBufferImageIndexData gBufferIndices{
        .albedoIndex = gBuffers.albedo->getIndex(),
        .normalIndex = gBuffers.normal->getIndex(),
        .emissiveAoIndex = gBuffers.emissiveAo->getIndex(),
        .metallicRoughnessIndex = gBuffers.metallicRoughness->getIndex(),
        .depthIndex = gBuffers.depth->getIndex(),
    };

    LightBufferImageIndexData lData{
        .diffuseIndex = lightingBuffers.diffuse->getIndex(),
        .specularIndex = lightingBuffers.specular->getIndex(),
    };

    struct CombinePushConstantData {
      GBufferImageIndexData gBufferIndices;
      LightBufferImageIndexData lightBufferIndices;
    } pushConstantData{
        .gBufferIndices = gBufferIndices,
        .lightBufferIndices = lData,
    };

    frame.graphicsCmdBuf->writePushConstants(
        *pipelines.combineDeferred, shaders::ShaderStages::Fragment, 0,
        sizeof(CombinePushConstantData), &pushConstantData);

    frame.graphicsCmdBuf->draw(3);

    frame.graphicsCmdBuf->endRendering();

    backend->textureLayoutTransition(
        frame.graphicsCmdBuf,
        {TextureTransition{
            .type = TextureTransitionType::RenderableToShaderRead,
            .texture = lightCombinedBuffer.get(),
        }});
  }

  void Renderer::drawForwardPass(const FrameData& frame) {}

  void Renderer::drawTransparentPass(const FrameData& frame) {}

  void Renderer::drawUIPass(const FrameData& frame) {
    backend->renderImGui(frame.graphicsCmdBuf);
  }
} // namespace keptech
