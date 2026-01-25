#include "keptech/renderer.hpp"
#include <keptech/core/kt-logger.hpp>
#include <keptech/core/rendering/commandBuffer.hpp>
#include <keptech/core/scene.hpp>

namespace keptech {

  void Renderer::render() {
    if (!scene) {
      KT_WARN("No scene set for renderer, skipping render.");
      backend->present();
      return;
    }

    FrameData frame;
    {
      auto activeCameraEntity = scene->getActiveCamera();
      if (!activeCameraEntity
               .hasAllComponents<components::Camera, components::Transform>()) {
        KT_ERROR(
            "Active camera entity is missing Camera or Transform component.");
        backend->present();
        return;
      }

      auto [camera, transform] =
          activeCameraEntity
              .getComponents<components::Camera, components::Transform>();

      frame.cameraData.camera = &camera;
      frame.cameraData.transform = &transform;
    }

    auto graphicsCmdBuf_res = backend->createGraphicsCmdBuffer();
    if (!graphicsCmdBuf_res) {
      KT_ERROR("Failed to create graphics command buffer: {}",
               graphicsCmdBuf_res.error());
      backend->present();
      return;
    }
    frame.graphicsCmdBuf = std::move(graphicsCmdBuf_res.value());

    frame.graphicsCmdBuf->begin();

    drawDeferredPass(frame);
    drawLightingPass(frame);
    combineDeferredPass(frame);
    drawForwardPass(frame);
    drawTransparentPass(frame);
    drawUIPass(frame);

    frame.graphicsCmdBuf->end();

    backend->endFrame(std::move(frame.graphicsCmdBuf));
  }

  void Renderer::drawDeferredPass(const FrameData& frame) {
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
    frame.graphicsCmdBuf->endRendering();
  }

  void Renderer::drawLightingPass(const FrameData& frame) {}

  void Renderer::combineDeferredPass(const FrameData& frame) {}

  void Renderer::drawForwardPass(const FrameData& frame) {}

  void Renderer::drawTransparentPass(const FrameData& frame) {}

  void Renderer::drawUIPass(const FrameData& frame) {
    backend->renderImGui(frame.graphicsCmdBuf);
  }
} // namespace keptech
