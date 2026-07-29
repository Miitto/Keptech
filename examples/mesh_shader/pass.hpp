#pragma once

#include "keptech/render/helpers/pipeline.hpp"
#include "keptech/render/renderGraph/builder.hpp"
#include "keptech/render/renderer.hpp"
#include "shaders/examples/monkey/mesh.h"
#include <keptech/components.hpp>
#include <keptech/core/kt-logger.hpp>
#include <keptech/render/renderGraph/passInterface.hpp>

class GeometryPass : public kt::rdr::RenderPassInterface {
public:
  GeometryPass() = default;

  void setupDependencies(kt::rdr::RenderPassBuilder& self, kt::rdr::RenderGraphBuilder& graph, const kt::rdr::Renderer& renderer) override {
    auto& formats = renderer.getFormats();
    self.addColorOutput("kt::albedo", {.format = formats.render.albedo});
    self.setDepthStencilOutput("kt::depth", {.format = formats.render.depth});
  }

  void setup(kt::rdr::Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) override {
    auto& device = renderer.getMembers().vkcore.device;
    auto shaderRes = renderer.createShader(::shaders::mesh);
    if (!shaderRes.isOk()) {
      KT_ABORT("Failed to create mesh shader: {}", shaderRes.error());
    }
    kt::rdr::PipelineLayoutBuilder layoutBuilder{};
    layoutBuilder.addDescriptorSetLayout(renderer.getGlobalDescriptorSetLayout())
        .addDescriptorSetLayout(descriptorSetLayout)
        .addPushConstantRange<glm::mat4, uint32_t>(
            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    auto layoutRes = renderer.createPipelineLayout(layoutBuilder);
    if (!layoutRes.isOk()) {
      shaderRes.value().destroy();
      KT_ABORT("Failed to create pipeline layout: {}", layoutRes.error());
    }

    auto& formats = renderer.getFormats();

    kt::rdr::GraphicsPipelineBuilder pipelineBuilder{};
    pipelineBuilder.layout(layoutRes.value())
        .addShaderStages(shaderRes.value().stages)
        .addColorAttachment(formats.render.albedo)
        .depthAttachment(formats.render.depth)
        .cullMode(kt::rdr::CullMode::Back)
        .depthWrite()
        .depthTest(kt::rdr::DepthCompareOp::LessOrEqual);
    auto pipelineRes = renderer.createPipeline(pipelineBuilder);
    if (!pipelineRes.isOk()) {
      shaderRes.value().destroy();
      vkDestroyPipelineLayout(device, layoutRes.value(), nullptr);
      KT_ABORT("Failed to create graphics pipeline: {}", pipelineRes.error());
    }

    pipeline = pipelineRes.value();

    shaderRes.value().destroy();
  }

  void prepare(kt::rdr::RenderGraph& graph, kt::rdr::Renderer& renderer) override { KT_TRACE("Preparing geometry pass"); }

  void execute(const kt::rdr::CommandBuffer& cmd, VkDescriptorSet descriptorSet, glm::uvec2 framebufferSize) override {
    KT_TRACE("Executing geometry pass");
    auto& r = kt::rdr::Renderer::get();

    std::array<VkDescriptorSet, 2> descriptorSets = {r.getGlobalDescriptorSet(), descriptorSet};
    constexpr std::array<VkDeviceSize, 2> vertexOffsets = {0, 0};

    cmd.bindPipeline(pipeline)
        .setViewportScissor(framebufferSize)
        .bindDescriptorSets(pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, descriptorSets);

    auto meshView = kt::Scene::active().view<kt::components::Mesh, kt::components::Transform>();

    for (const auto& [entity, mesh, transform] : meshView.each()) {
      const glm::mat4 modelMatrix = transform.getGlobal();
      vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(glm::mat4), &modelMatrix);

      for (const auto& submesh : mesh.getSubmeshes()) {
        uint32_t meshletCount = submesh.meshletCount;
        if (meshletCount == 0) {
          continue;
        }

        vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           sizeof(glm::mat4), sizeof(uint32_t), &meshletCount);

        uint32_t taskShaderDispatches = (submesh.meshletCount + 63) / 64;
        vkCmdDrawMeshTasksEXT(cmd, taskShaderDispatches, 1, 1);

        r.registerMeshletDrawCall(submesh.meshletCount, submesh.meshletTriangleCount,
                                  static_cast<size_t>(submesh.meshletTriangleCount) * 3);
      }
    }
  }

  bool getClearColor(size_t attachmentIndex, VkClearColorValue* value) const override {
    if (value) {
      *value = {
          .float32 = {0.0f, 0.0f, 0.0f, 1.0f},
      };
    }
    return true;
  }

  bool getClearDepthStencil(VkClearDepthStencilValue* value) const override {
    if (value) {
      *value = {
          .depth = 1.0f,
          .stencil = 0,
      };
    }
    return true;
  }

  void shutdown(kt::rdr::Renderer& renderer) override {
    KT_TRACE("Shutting down geometry pass");
    auto device = renderer.getMembers().vkcore.device;
    vkDestroyPipeline(device, pipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
  }

private:
  kt::rdr::Pipeline pipeline{};
};