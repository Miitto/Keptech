#include "imgui.h"
#include <keptech/app.hpp>
#include <keptech/keptech.hpp>

#include <expected>
#include <keptech/cameras/freeCamera.hpp>
#include <keptech/components.hpp>
#include <keptech/core/gui.h>
#include <keptech/core/kt-logger.hpp>
#include <keptech/core/window.hpp>
#include <keptech/ecs/entity.hpp>
#include <keptech/renderer.hpp>

#include "keptech/vulkan/helpers/pipeline.hpp"
#include "shaders/keptech/mesh_shader.h"

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

kt::SetupInfo kt::configureApp() {
  return {.window = {.title = "Material Editor", .width = WINDOW_WIDTH, .height = WINDOW_HEIGHT},
          .renderer = {.applicationName = "Material Editor"}};
}

class GeometryPass : public kt::rendering::RenderPassInterface {
public:
  void setupDependencies(kt::rendering::RenderPassBuilder& self, kt::rendering::RenderGraphBuilder& graph,
                         const kt::rendering::Renderer& renderer) override {
    auto& formats = renderer.getFormats();
    self.addColorOutput("kt::albedo", {.format = formats.render.albedo});
    self.addColorOutput("kt::normal", {.format = formats.render.normal});
    self.addColorOutput("kt::material", {.format = formats.render.metRought});
    self.addColorOutput("kt::emissive", {.format = formats.render.emissive});
    self.setDepthStencilOutput("kt::depth", {.format = formats.render.depth});
  }

  void setup(kt::rendering::Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) override {
    auto& device = renderer.getMembers().vkcore.device;
    auto shaderRes = renderer.createShader(::shaders::mesh_shader);
    if (!shaderRes.isOk()) {
      KT_ABORT("Failed to create mesh shader: {}", shaderRes.error());
    }
    kt::vkh::PipelineLayoutBuilder layoutBuilder{};
    layoutBuilder.addDescriptorSetLayout(renderer.getGlobalDescriptorSetLayout())
        .addDescriptorSetLayout(descriptorSetLayout)
        .addPushConstantRange<glm::mat4, uint32_t, uint32_t>(
            VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    auto layoutRes = renderer.createPipelineLayout(layoutBuilder);
    if (!layoutRes.isOk()) {
      shaderRes.value().destroy(device);
      KT_ABORT("Failed to create pipeline layout: {}", layoutRes.error());
    }

    auto& formats = renderer.getFormats();

    auto vertexInput = kt::vkh::Shader::getVertexInput(::shaders::mesh_shader);

    kt::vkh::GraphicsPipelineBuilder pipelineBuilder{};
    pipelineBuilder.layout(layoutRes.value())
        .addShaderStages(shaderRes.value().stages)
        .addColorAttachment(formats.render.albedo)
        .addColorAttachment(formats.render.normal)
        .addColorAttachment(formats.render.metRought)
        .addColorAttachment(formats.render.emissive)
        .depthAttachment(formats.render.depth)
        .addVertexInputAttributes(vertexInput.attributes)
        .addVertexInputBindings(vertexInput.bindings)
        .cullMode(kt::vkh::CullMode::Back)
        .depthWrite()
        .depthTest();
    auto pipelineRes = renderer.createPipeline(pipelineBuilder);
    if (!pipelineRes.isOk()) {
      shaderRes.value().destroy(device);
      vkDestroyPipelineLayout(device, layoutRes.value(), nullptr);
      KT_ABORT("Failed to create graphics pipeline: {}", pipelineRes.error());
    }

    pipeline = pipelineRes.value();

    shaderRes.value().destroy(renderer.getMembers().vkcore.device);
  }

  void prepare(kt::rendering::RenderGraph& graph, kt::rendering::Renderer& renderer) override { KT_TRACE("Preparing geometry pass"); }

  void execute(const kt::rendering::CommandBuffer& cmd, VkDescriptorSet descriptorSet, glm::uvec2 framebufferSize) override {
    KT_TRACE("Executing geometry pass");
  }

  bool getClearColor(size_t attachmentIndex, VkClearColorValue* value) const override {
    if (value) {
      *value = {
          .float32 = {1.0f, 0.0f, 0.0f, 1.0f},
      };
    }
    return true;
  }

  bool getClearDepthStencil(VkClearDepthStencilValue* value) const override {
    if (value) {
      *value = {};
    }
    return true;
  }

  void shutdown(kt::rendering::Renderer& renderer) override {
    KT_TRACE("Shutting down geometry pass");
    auto device = renderer.getMembers().vkcore.device;
    vkDestroyPipeline(device, pipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
  }

private:
  kt::vkh::Pipeline pipeline{};
};

class BenchmarkLayer : public kt::core::layers::Layer {
public:
  BenchmarkLayer(kt::Window& window, kt::rendering::RenderGraphBuilder& builder, kt::rendering::Renderer& renderer)
      : kt::core::layers::Layer("Monkey"), window(window), scene({}) {
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

  void setupRenderGraph(kt::rendering::RenderGraphBuilder& builder, kt::rendering::Renderer& renderer) {
    using kt::rendering::AttachmentSize;
    using kt::rendering::QueueType;
    auto& formats = renderer.getFormats();
    auto& geometryPass = builder.addPass("kt::geometry", QueueType::Graphics);

    geometryPass.setInterface(&this->geometryPass);

    auto& lightingPass = builder.addPass("kt::lighting", QueueType::Graphics);
    lightingPass.addTextureInput("kt::albedo");
    lightingPass.addTextureInput("kt::normal");
    lightingPass.addTextureInput("kt::material");
    lightingPass.addTextureInput("kt::emissive");
    lightingPass.addTextureInput("kt::depth");
    lightingPass.setDepthStencilInput("kt::depth");
    lightingPass.addColorOutput("kt::diffuse", {.format = formats.render.emissive});
    lightingPass.addColorOutput("kt::specular", {.format = formats.render.emissive});

    lightingPass.setBuildCallback([&](auto cmd, auto set, auto framebufferSize) { KT_TRACE("Building lighting pass"); });

    auto& ssaoPass = builder.addPass("kt::ssao", QueueType::AsyncCompute);
    ssaoPass.addTextureInput("kt::depth");
    ssaoPass.addTextureInput("kt::normal");
    ssaoPass.addStorageImageOutput("kt::ssao", {.format = VK_FORMAT_R8_UNORM});

    auto& lightCombinePass = builder.addPass("kt::lightCombine", QueueType::Graphics);
    lightCombinePass.addTextureInput("kt::albedo");
    lightCombinePass.addTextureInput("kt::diffuse");
    lightCombinePass.addTextureInput("kt::specular");
    lightCombinePass.addTextureInput("kt::ssao");
    lightCombinePass.addColorOutput("kt::lighting", {.format = formats.render.emissive}, "kt::emissive");

    lightCombinePass.setBuildCallback([&](auto cmd, auto set, auto framebufferSize) { KT_TRACE("Building light combine pass"); });

    auto& tonemapPass = builder.addPass("kt::tonemap", QueueType::Graphics);
    tonemapPass.addTextureInput("kt::lighting");
    tonemapPass.addColorOutput("kt::tonemapped", {.sizeType = AttachmentSize::SwapchainRelative, .format = formats.swapchain});

    tonemapPass.setBuildCallback([&](auto cmd, auto set, auto framebufferSize) { KT_TRACE("Building tonemap pass"); });

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
                                                    kt::rendering::RenderGraphBuilder& builder, kt::rendering::Renderer& renderer) {

  layerStack.emplaceLayer<BenchmarkLayer>(window, builder, renderer);

  return {};
}
