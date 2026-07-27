#include "imgui.h"
#include <keptech/app.hpp>
#include <keptech/keptech.hpp>

#include "keptech/render/helpers/pipeline.hpp"
#include "keptech/render/renderGraph/builder.hpp"
#include "keptech/render/renderer.hpp"
#include "shaders/examples/monkey/mesh.h"
#include <expected>
#include <keptech/cameras/freeCamera.hpp>
#include <keptech/components.hpp>
#include <keptech/core/gui.h>
#include <keptech/core/kt-logger.hpp>
#include <keptech/core/window.hpp>
#include <keptech/ecs/entity.hpp>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

kt::SetupInfo kt::configureApp() {
  return {.window = {.title = "Material Editor", .width = WINDOW_WIDTH, .height = WINDOW_HEIGHT},
          .renderer = {.applicationName = "Material Editor", .capabilities = kt::RendererCapabilities::MeshShader}};
}

class GeometryPass : public kt::RenderPassInterface {
public:
  GeometryPass(kt::Scene& scene) : scene(scene) {}

  void setupDependencies(kt::RenderPassBuilder& self, kt::RenderGraphBuilder& graph, const kt::Renderer& renderer) override {
    auto& formats = renderer.getFormats();
    self.addColorOutput("kt::albedo", {.format = formats.render.albedo});
    self.setDepthStencilOutput("kt::depth", {.format = formats.render.depth});
  }

  void setup(kt::Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) override {
    auto& device = renderer.getMembers().vkcore.device;
    auto shaderRes = renderer.createShader(::shaders::mesh);
    if (!shaderRes.isOk()) {
      KT_ABORT("Failed to create mesh shader: {}", shaderRes.error());
    }
    kt::vkh::PipelineLayoutBuilder layoutBuilder{};
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

    kt::vkh::GraphicsPipelineBuilder pipelineBuilder{};
    pipelineBuilder.layout(layoutRes.value())
        .addShaderStages(shaderRes.value().stages)
        .addColorAttachment(formats.render.albedo)
        .depthAttachment(formats.render.depth)
        .cullMode(kt::vkh::CullMode::Back)
        .depthWrite()
        .depthTest(kt::vkh::DepthCompareOp::LessOrEqual);
    auto pipelineRes = renderer.createPipeline(pipelineBuilder);
    if (!pipelineRes.isOk()) {
      shaderRes.value().destroy();
      vkDestroyPipelineLayout(device, layoutRes.value(), nullptr);
      KT_ABORT("Failed to create graphics pipeline: {}", pipelineRes.error());
    }

    pipeline = pipelineRes.value();

    shaderRes.value().destroy();
  }

  void prepare(kt::RenderGraph& graph, kt::Renderer& renderer) override { KT_TRACE("Preparing geometry pass"); }

  void execute(const kt::CommandBuffer& cmd, VkDescriptorSet descriptorSet, glm::uvec2 framebufferSize) override {
    KT_TRACE("Executing geometry pass");
    auto& r = kt::Renderer::get();

    std::array<VkDescriptorSet, 2> descriptorSets = {r.getGlobalDescriptorSet(), descriptorSet};
    constexpr std::array<VkDeviceSize, 2> vertexOffsets = {0, 0};

    cmd.bindPipeline(pipeline)
        .setViewportScissor(framebufferSize)
        .bindDescriptorSets(pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, descriptorSets);

    auto meshView = scene.view<kt::components::Mesh, kt::components::Transform>();

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

  void shutdown(kt::Renderer& renderer) override {
    KT_TRACE("Shutting down geometry pass");
    auto device = renderer.getMembers().vkcore.device;
    vkDestroyPipeline(device, pipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
  }

private:
  kt::Scene& scene; // NOLINT - This never moves. TODO: Make active scene a singleton or something so we don't have to pass it around
  kt::vkh::Pipeline pipeline{};
};

class BenchmarkLayer : public kt::core::layers::Layer {
public:
  BenchmarkLayer(kt::Window& window, kt::RenderGraphBuilder& builder, kt::Renderer& renderer)
      : kt::core::layers::Layer("Monkey"), window(window), scene({}), geometryPass(scene) {
    renderer.setScene(scene);

    auto monkeyMeshRes = renderer.loadMesh(ASSET_DIR "meshes/monkey.glb");
    if (!monkeyMeshRes) {
      KT_CRITICAL("Failed to load monkey mesh: {}", monkeyMeshRes.error());
      abort();
    }

    auto monkey = scene.createEntity("Monkey");
    monkeyMeshRes->addToEcsScene(scene, monkey.getHandle());

    auto smallPointLight = scene.createEntity("Small Point Light");
    auto& smallLightTransform = smallPointLight.addComponent<kt::components::Transform>();
    smallLightTransform.getLocalMut().translate(glm::vec3(12.0f, 4.0f, 4.0f));
    smallPointLight.addComponent<kt::components::PointLight>(kt::components::PointLight{
        .color = {0.95f, 0.95f, 1.f},
        .intensity = 5.f,
        .radius = 50.f,
    });

    auto camera = scene.createEntity("Camera");
    camera.addComponent<kt::components::Transform>()
        .getLocalMut()
        .translate(glm::vec3(12.0f, 4.0f, 4.0f))
        .lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    auto& camComp =
        camera.addComponent<kt::components::Camera>(kt::components::PerspectiveType::Standard, kt::components::Camera::Params::Common{},
                                                    kt::components::Camera::Params::Perspective{
                                                        .fovY = glm::radians(90.f),
                                                    });
    camComp.sizeToWindowSize(window.getRenderSize());

    scene.useCamera(camera);

    auto tView = scene.getEcs().view<kt::components::Transform>();
    for (const auto& [e, t] : tView.each()) {
      t.recalculateDepth();
    }

    scene.getEcs().sort<kt::components::Transform>([](const auto& a, const auto& b) { return a.getDepth() < b.getDepth(); });
    // Sorts meshes to minimize cache misses when iterating with transforms
    scene.getEcs().sort<kt::components::Mesh, kt::components::Transform>();

    freeController = kt::cameras::FreeCameraController(camera);

    setupRenderGraph(builder, renderer);
  }

  void setupRenderGraph(kt::RenderGraphBuilder& builder, kt::Renderer& renderer) {
    using kt::AttachmentSize;
    using kt::QueueType;
    auto& formats = renderer.getFormats();
    auto& geometryPass = builder.addPass("kt::geometry", QueueType::Graphics);
    geometryPass.setInterface(&this->geometryPass);

    builder.setBackbufferSource("kt::albedo");
  }

  void onUpdate(kt::Timestep ts) final {
    float deltaTime = ts / 1000.f;
    float fps = 1.f / deltaTime;

    ImGui::Begin("Monkey");
    ImGui::Text("FPS: %.2f", fps);
    ImGui::Text("Frame Time: %.2f ms", ts);
    ImGui::End();

    freeController.update(ts);
  }

  void onEvent(kt::core::events::Event& event, kt::Timestep ts) final {
    if (freeController.handleEvent(event, ts))
      return;
  }

private:
  kt::Window& window;
  kt::Scene scene;
  kt::cameras::FreeCameraController freeController;

  GeometryPass geometryPass;
};

std::expected<void, std::string> kt::setupAppLayers(core::layers::LayerStack& layerStack, core::window::Window& window,
                                                    kt::RenderGraphBuilder& builder, kt::Renderer& renderer) {

  layerStack.emplaceLayer<BenchmarkLayer>(window, builder, renderer);

  return {};
}
