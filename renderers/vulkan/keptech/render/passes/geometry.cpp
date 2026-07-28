#include "geometry.hpp"

#include "helpers/transitions.hpp"
#include "helpers/viewScissor.hpp"
#include "keptech/render/renderer.hpp"
#include "profile.hpp"

namespace kt::rdr::passes::geometry {
  void draw(const Members& m, VkCommandBuffer cmdBuf, const Target& target, const Payload& payload) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyGraphicsContext, cmdBuf, "Draw Geometry");

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.mesh_shader.pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.mesh_shader.layout, 0, 1,
                            &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

    setFullscreenViewportAndScissor(cmdBuf, target.albedo);

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

      std::array<VkBuffer, 2> vBufs = {m.buffers.vertexPositions, m.buffers.vertexAttribs};
      std::array<VkDeviceSize, 2> offsets = {0, 0};
      vkCmdBindVertexBuffers(cmdBuf, 0, static_cast<uint32_t>(vBufs.size()), vBufs.data(), offsets.data());

      uint32_t taskShaderDispatches = (submesh.meshletCount + 63) / 64;
      vkCmdDrawMeshTasksEXT(cmdBuf, taskShaderDispatches, 1, 1);
    }
  }
} // namespace kt::rdr::passes::geometry