#include "geometry.hpp"

#include "helpers/transitions.hpp"
#include "helpers/viewScissor.hpp"
#include "keptech/vulkan/buffers.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "profile.hpp"

namespace kt::vkh::passes::geometry {
  using rendering::ImageLayout;
  using rendering::ImageType;

  namespace {

    void deferredToRenderable(VkCommandBuffer cmdBuf, const Target& target) {
      layoutTransitions<5>(cmdBuf, {
                                       target.albedo->transition({ImageType::Color, ImageLayout::Undefined, ImageLayout::RenderTarget}),
                                       target.normal->transition({ImageType::Color, ImageLayout::Undefined, ImageLayout::RenderTarget}),
                                       target.emissive->transition({ImageType::Color, ImageLayout::Undefined, ImageLayout::RenderTarget}),
                                       target.metRough->transition({ImageType::Color, ImageLayout::Undefined, ImageLayout::RenderTarget}),
                                       target.depth->transition({ImageType::Depth, ImageLayout::Undefined, ImageLayout::RenderTarget}),
                                   });
    }

    void deferredBeginRendering(VkCommandBuffer cmdBuf, const Target& target) {
      auto size = target.albedo->extent();
      beginRendering<4>(cmdBuf,
                        VkRect2D{
                            .offset = VkOffset2D{.x = 0, .y = 0},
                            .extent = VkExtent2D{.width = size.x, .height = size.y},
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
                               target.depth->transition({ImageType::Depth, ImageLayout::RenderTarget, ImageLayout::ShaderReadOnly}),
                           });
    }
  } // namespace

  void draw(const Members& m, VkCommandBuffer cmdBuf, const Target& target, const Payload& payload) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyGraphicsContext, cmdBuf, "Draw Geometry");
    deferredToRenderable(cmdBuf, target);
    deferredBeginRendering(cmdBuf, target);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.mesh_shader.pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.mesh_shader.layout, 0, 1,
                            &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

    setFullscreenViewportAndScissor(cmdBuf, *target.albedo);

    for (size_t i = 0; i < payload.submeshes.size(); ++i) {
      const auto& submesh = payload.submeshes[i];
      const auto& modelMatrix = payload.modelMatrices[i];

      struct PC {
        glm::mat4 modelMatrix;
        uint32_t materialIndex;
        uint32_t meshletCount;
      } pc{
          .modelMatrix = modelMatrix,
          .materialIndex = submesh.material.value_or(0),
          .meshletCount = submesh.meshletCount,
      };

      vkCmdPushConstants(cmdBuf, m.pipelines.mesh_shader.layout,
                         VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PC), &pc);

      std::array<VkBuffer, 2> vBufs = {m.buffers.vertexPositions->buffer, m.buffers.vertexAttribs->buffer};
      std::array<VkDeviceSize, 2> offsets = {0, 0};
      vkCmdBindVertexBuffers(cmdBuf, 0, static_cast<uint32_t>(vBufs.size()), vBufs.data(), offsets.data());

      uint32_t taskShaderDispatches = (submesh.meshletCount + 63) / 64;
      vkCmdDrawMeshTasksEXT(cmdBuf, taskShaderDispatches, 1, 1);
    }

    vkCmdEndRendering(cmdBuf);

    deferredToShaderRead(cmdBuf, target);
  }
} // namespace kt::vkh::passes::geometry