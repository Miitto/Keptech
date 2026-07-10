#include "helpers/transitions.hpp"
#include "helpers/viewScissor.hpp"
#include "profile.hpp"
#include "renderer.hpp"

namespace kt::vkh::passes::geometry {
  namespace {

    void deferredToRenderable(VkCommandBuffer cmdBuf, const Target& target) {
      layoutTransitions<5>(cmdBuf,
                           {
                               target.albedo->transition({ImageType::Color, ImageLayout::Undefined, ImageLayout::RenderTarget}),
                               target.normal->transition({ImageType::Color, ImageLayout::Undefined, ImageLayout::RenderTarget}),
                               target.emissive->transition({ImageType::Color, ImageLayout::Undefined, ImageLayout::RenderTarget}),
                               target.metRough->transition({ImageType::Color, ImageLayout::Undefined, ImageLayout::RenderTarget}),
                               target.depth->transition({ImageType::DepthStencil, ImageLayout::Undefined, ImageLayout::RenderTarget}),
                           });
    }

    void deferredBeginRendering(VkCommandBuffer cmdBuf, const Target& target) {
      auto size = target.albedo->extent();
      beginRendering<4>(cmdBuf,
                        VkRect2D{
                            .offset = VkOffset2D{.x = 0, .y = 0},
                            .extent = VkExtent2D{.width = static_cast<uint32_t>(size.width), .height = static_cast<uint32_t>(size.height)},
                        },
                        {
                            clearColorAttachment(*target.albedo),
                            clearColorAttachment(*target.normal),
                            clearColorAttachment(*target.emissive),
                            clearColorAttachment(*target.metRough),
                        },
                        clearDepthAttachment(*target.depth, 1.f));
    }
    void deferredToShaderRead(VkCommandBuffer cmdBuf, const Target& target) {
      layoutTransitions<5>(cmdBuf,
                           {
                               target.albedo->transition({ImageType::Color, ImageLayout::RenderTarget, ImageLayout::ShaderReadOnly}),
                               target.normal->transition({ImageType::Color, ImageLayout::RenderTarget, ImageLayout::ShaderReadOnly}),
                               target.emissive->transition({ImageType::Color, ImageLayout::RenderTarget, ImageLayout::ShaderReadOnly}),
                               target.metRough->transition({ImageType::Color, ImageLayout::RenderTarget, ImageLayout::ShaderReadOnly}),
                               target.depth->transition({ImageType::DepthStencil, ImageLayout::RenderTarget, ImageLayout::ShaderReadOnly}),
                           });
    }
  } // namespace

  void draw(Renderer::Members& m, VkCommandBuffer cmdBuf, const Target& target, const Payload& payload) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyGraphicsContext, cmdBuf, "Draw Geometry");
    deferredToRenderable(cmdBuf, target);
    deferredBeginRendering(cmdBuf, target);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.mesh_shader.pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.mesh_shader.layout, 0, 1,
                            &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

    setFullscreenViewportAndScissor(cmdBuf, *target.albedo);

    vkCmdEndRendering(cmdBuf);

    deferredToShaderRead(cmdBuf, target);
  }
} // namespace kt::vkh::passes::geometry