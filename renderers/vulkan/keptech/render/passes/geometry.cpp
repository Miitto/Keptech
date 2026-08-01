#include "geometry.hpp"

#include "glm/ext/matrix_float4x4.hpp"
#include "gpuObjects.hpp"
#include "helpers/pipeline.hpp"
#include "keptech/components/camera.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/maths/maths.hpp"
#include "keptech/render/renderer.hpp"
#include "renderGraph/graph.hpp"
#include "renderGraph/pass.hpp"
#include "shaders/keptech/geometry.h"

namespace kt::rdr {
  void GeometryPass::prepare(RenderGraph& graph, Renderer&) {
    const auto& f = graph.getEngineCameraFrustum();

    auto meshView = Scene::active().view<components::Mesh, components::Transform>();

    std::vector<GpuObject> gpuObjects;
    std::vector<VkDrawIndexedIndirectCommand> drawCommands;
    gpuObjects.reserve(meshView.size_hint());
    for (const auto& [entity, mesh, transform] : meshView.each()) {
      glm::mat4 modelMatrix = transform.getGlobal();
      for (const auto& submesh : mesh.getSubmeshes()) {
        gpuObjects.emplace_back(GpuObject{.model = modelMatrix, .meshIndex = submesh.id, .materialIndex = submesh.material.value_or(0)});

        auto bounds = submesh.boundingSphere.apply(modelMatrix);

        if (f.intersects(bounds) == maths::IntersectionType::eNone) {
          continue;
        }

        drawCommands.emplace_back(VkDrawIndexedIndirectCommand{
            .indexCount = submesh.indexCount,
            .instanceCount = 1,
            .firstIndex = submesh.indexOffset,
            .vertexOffset = submesh.vertexOffset,
            .firstInstance = static_cast<uint32_t>(gpuObjects.size() - 1),
        });
      }
    }

    constexpr size_t objectSize = sizeof(GpuObject);
    constexpr size_t drawCommandSize = sizeof(VkDrawIndexedIndirectCommand);
    size_t objectCount = gpuObjects.size();
    drawCommandCount = drawCommands.size();
    size_t objectByteSize = objectCount * objectSize;
    size_t drawCommandByteSize = drawCommandCount * drawCommandSize;
    drawCommandStart = maths::roundToAlignment(objectByteSize, alignof(VkDrawIndexedIndirectCommand));
    size_t bufferSize = drawCommandStart + drawCommandByteSize;
    {
      auto& buffer = graph.getFrameBuffer(objectBufferIndex);
      if (buffer.size() < bufferSize) {
        graph.reallocatePerFrameBuffer(objectBufferIndex, bufferSize, false);
      }
    }

    auto& buffer = graph.getFrameBuffer(objectBufferIndex);
    buffer.write(gpuObjects.data(), gpuObjects.size() * sizeof(GpuObject));
    buffer.write(drawCommands.data(), drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand), drawCommandStart);
  }

  void GeometryPass::execute(RenderGraph& graph, const CommandBuffer& cmd, VkDescriptorSet descriptorSet, glm::uvec2 framebufferSize) {
    auto& r = Renderer::get();
    std::array<VkDescriptorSet, 2> descriptorSets = {r.getGlobalDescriptorSet(), descriptorSet};
    cmd.bindPipeline(pipeline)
        .bindDescriptorSets(pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, descriptorSets)
        .bindRendererVertexIndexBuffers()
        .setViewportScissor(framebufferSize);

    cmd.drawIndexedIndirect(graph.getFrameBuffer(objectBufferIndex), static_cast<uint32_t>(drawCommandCount), drawCommandStart,
                            sizeof(VkDrawIndexedIndirectCommand));
  }

  void GeometryPass::setClearColorBuffers(bool clear) { clearColorBuffers = clear; }
  void GeometryPass::setDepthClearValue(float value) { depthClearValue = value; }

  void GeometryPass::setupDependencies(RenderPassBuilder& self, RenderGraphBuilder&, const Renderer& renderer) {
    auto& f = renderer.getFormats();
    self.addColorOutput("kt::albedo", {.format = f.render.albedo});
    self.addColorOutput("kt::normal", {.format = f.render.normal});
    self.addColorOutput("kt::material", {.format = f.render.metRough});
    self.addColorOutput("kt::emissive", {.format = f.render.emissive});
    self.setDepthStencilOutput("kt::depth", {.format = f.render.depth});

    self.addMappedBuffer(KT_GEOMETRY_PER_FRAME_NAME, sizeof(GpuObject) * 1000 + sizeof(VkDrawIndexedIndirectCommand) * 1000,
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                         VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    self.addStorageReadOnlyInput(KT_GEOMETRY_PER_FRAME_NAME, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    self.addIndirectBufferInput(KT_GEOMETRY_PER_FRAME_NAME);
  }

  namespace {
    enum class GeometryPassAttachment : uint8_t {
      Albedo,
      Normal,
      Material,
      Emissive,
    };
  }

  void GeometryPass::setup(RenderGraph& graph, Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) {
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

    objectBufferIndex = graph.getBufferIndex(KT_GEOMETRY_PER_FRAME_NAME);
  }

  [[nodiscard]] bool GeometryPass::getClearDepthStencil(VkClearDepthStencilValue* value) const {
    if (value)
      *value = {.depth = depthClearValue, .stencil = 0};
    return true;
  }

  [[nodiscard]] bool GeometryPass::getClearColor(size_t, VkClearColorValue* value) const {
    if (!clearColorBuffers)
      return false;
    if (value)
      *value = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
    return true;
  }
} // namespace kt::rdr