#include "geometry.hpp"

#include "glm/ext/matrix_float4x4.hpp"
#include "helpers/pipeline.hpp"
#include "keptech/components/camera.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/render/renderer.hpp"
#include "renderGraph/pass.hpp"
#include "shaders/keptech/geometry.h"

namespace kt::rdr {
  void GeometryPass::execute(const CommandBuffer& cmd, VkDescriptorSet descriptorSet, glm::uvec2 framebufferSize) {
    auto& r = Renderer::get();
    std::array<VkDescriptorSet, 2> descriptorSets = {r.getGlobalDescriptorSet(), descriptorSet};
    cmd.bindPipeline(pipeline)
        .bindDescriptorSets(pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, descriptorSets)
        .bindRendererVertexIndexBuffers()
        .setViewportScissor(framebufferSize);

    auto meshView = Scene::active().view<components::Mesh, components::Transform>();

    for (const auto& [entity, mesh, transform] : meshView.each()) {
      const glm::mat4 modelMatrix = transform.getGlobal();
      cmd.pushConstants(pipeline, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4), &modelMatrix);

      for (const auto& submesh : mesh.getSubmeshes()) {
        uint32_t materialIndex = submesh.material.has_value() ? submesh.material.value() : 0;
        cmd.pushConstants(pipeline, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(uint32_t),
                          &materialIndex);

        cmd.drawIndexed(submesh.indexCount, submesh.indexOffset, submesh.vertexOffset);
      }
    }
  }

  void GeometryPass::setupDependencies(RenderPassBuilder& self, RenderGraphBuilder&, const Renderer& renderer) {
    auto& f = renderer.getFormats();
    self.addColorOutput("kt::albedo", {.format = f.render.albedo});
    self.addColorOutput("kt::normal", {.format = f.render.normal});
    self.addColorOutput("kt::material", {.format = f.render.metRough});
    self.addColorOutput("kt::emissive", {.format = f.render.emissive});
    self.setDepthStencilOutput("kt::depth", {.format = f.render.depth});
  }

  namespace {
    enum class GeometryPassAttachment : uint8_t {
      Albedo,
      Normal,
      Material,
      Emissive,
    };
  }

  void GeometryPass::setup(Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) {
    auto& device = renderer.getMembers().vkcore.device;
    auto shaderRes = renderer.createShader(::shaders::geometry);
    if (!shaderRes.isOk()) {
      KT_ABORT("Failed to create mesh shader: {}", shaderRes.error());
    }
    PipelineLayoutBuilder layoutBuilder{};
    layoutBuilder.addDescriptorSetLayout(renderer.getGlobalDescriptorSetLayout())
        .addDescriptorSetLayout(descriptorSetLayout)
        .addPushConstantRange<glm::mat4, uint32_t>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    auto layoutRes = renderer.createPipelineLayout(layoutBuilder);
    if (!layoutRes.isOk()) {
      shaderRes.value().destroy();
      KT_ABORT("Failed to create pipeline layout: {}", layoutRes.error());
    }

    auto& formats = renderer.getFormats();

    auto cameraEntity = Scene::active().getActiveCamera();
    auto& camera = cameraEntity.getComponents<components::Camera>();

    bool isReverseZ = camera.getProjectionType() == components::ProjectionType::PerspectiveInfinite;

    DepthCompareOp depthCompareOp = isReverseZ ? DepthCompareOp::GreaterOrEqual : DepthCompareOp::LessOrEqual;
    if (isReverseZ) {
      depthClearValue = 0.0f;
    }

    auto vertexInput = kt::rdr::Shader::getVertexInput(::shaders::geometry);

    GraphicsPipelineBuilder pipelineBuilder{};
    pipelineBuilder.layout(layoutRes.value())
        .addShaderStages(shaderRes.value().stages)
        .addVertexInputAttributes(vertexInput.attributes)
        .addVertexInputBindings(vertexInput.bindings)
        .addColorAttachment(formats.render.albedo)
        .addColorAttachment(formats.render.normal)
        .addColorAttachment(formats.render.metRough)
        .addColorAttachment(formats.render.emissive)
        .depthAttachment(formats.render.depth)
        .cullMode(CullMode::Back)
        .depthWrite()
        .depthTest(depthCompareOp);
    auto pipelineRes = renderer.createPipeline(pipelineBuilder);
    if (!pipelineRes.isOk()) {
      shaderRes.value().destroy();
      vkDestroyPipelineLayout(device, layoutRes.value(), nullptr);
      KT_ABORT("Failed to create graphics pipeline: {}", pipelineRes.error());
    }

    pipeline = pipelineRes.value();

    shaderRes.value().destroy();
  }

  [[nodiscard]] bool GeometryPass::getClearDepthStencil(VkClearDepthStencilValue* value) const {
    if (value)
      *value = {.depth = depthClearValue, .stencil = 0};
    return true;
  }

  [[nodiscard]] bool GeometryPass::getClearColor(size_t, VkClearColorValue*) const { return false; }
} // namespace kt::rdr