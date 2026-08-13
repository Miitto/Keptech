#pragma once

#include "keptech/graph/builder.hpp"
#include "keptech/rhi/helpers/pipeline.hpp"
#include "keptech/rhi/rhi.hpp"
#include "shaders/examples/monkey/mesh.h"
#include <keptech/components.hpp>
#include <keptech/core/kt-logger.hpp>
#include <keptech/graph/passInterface.hpp>

class TrianglePass : public kt::rhi::RenderPassInterface {
public:
  TrianglePass() = default;

  void setupDependencies(kt::rhi::RenderPassBuilder& self, kt::rhi::RenderGraphBuilder& graph, const kt::rhi::Renderer& renderer) override {
    auto& formats = renderer.getFormats();
    self.addColorOutput("kt::albedo", {.format = formats.render.albedo});
    self.setDepthStencilOutput("kt::depth", {.format = formats.render.depth});
  }

  void setup(kt::rhi::RenderGraph&, kt::rhi::Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) override {
    auto& device = renderer.getMembers().vkcore.device;
    auto shaderRes = renderer.createShader(::shaders::mesh);
    if (!shaderRes.isOk()) {
      KT_ABORT("Failed to create mesh shader: {}", shaderRes.error());
    }
    kt::rhi::PipelineLayoutBuilder layoutBuilder{};
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

    kt::rhi::GraphicsPipelineBuilder pipelineBuilder{};
    pipelineBuilder.layout(layoutRes.value())
        .addShaderStages(shaderRes.value().stages)
        .addColorAttachment(formats.render.albedo)
        .depthAttachment(formats.render.depth)
        .cullMode(kt::rhi::CullMode::Back)
        .depthWrite()
        .depthTest(kt::rhi::DepthCompareOp::LessOrEqual);
    auto pipelineRes = renderer.createPipeline(pipelineBuilder);
    if (!pipelineRes.isOk()) {
      shaderRes.value().destroy();
      vkDestroyPipelineLayout(device, layoutRes.value(), nullptr);
      KT_ABORT("Failed to create graphics pipeline: {}", pipelineRes.error());
    }

    pipeline = pipelineRes.value();

    shaderRes.value().destroy();
  }

  void prepare(kt::rhi::RenderGraph& graph, kt::rhi::Renderer& renderer) override { KT_TRACE("Preparing geometry pass"); }

  void execute(kt::rhi::RenderGraph&, const kt::rhi::CommandBuffer& cmd, VkDescriptorSet descriptorSet,
               glm::uvec2 framebufferSize) override {
    KT_TRACE("Executing geometry pass");
    auto& r = kt::rhi::RHI::get();

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
        if (submesh.meshletCount == 0) {
          continue;
        }

        vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           sizeof(glm::mat4), sizeof(uint32_t), &submesh.id);

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

  void shutdown(kt::rhi::RenderGraph&, kt::rhi::Renderer& renderer) override {
    KT_TRACE("Shutting down geometry pass");
    auto device = renderer.getMembers().vkcore.device;
    vkDestroyPipeline(device, pipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
  }

private:
  kt::rhi::Pipeline pipeline{};
};