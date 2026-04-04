#include <keptech/app.hpp>
#include <keptech/keptech.hpp>

#include <expected>
#include <keptech/cameras/freeCamera.hpp>
#include <keptech/components.hpp>
#include <keptech/core/gui.h>
#include <keptech/core/window.hpp>
#include <keptech/ecs/entity.hpp>
#include <keptech/renderer.hpp>
#include <utility>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

kt::SetupInfo kt::configureApp() {
  return {.window = {.title = "Material Editor", .width = WINDOW_WIDTH, .height = WINDOW_HEIGHT},
          .renderer = {.applicationName = "Material Editor"}};
}

class BenchmarkLayer : public kt::core::layers::Layer {
public:
  BenchmarkLayer(kt::Window& window, kt::rendering::Renderer& renderer, kt::Scene&& scene, kt::ecs::Entity cameraEntity)
      : kt::core::layers::Layer("Benchkark"), window(window), scene(std::move(scene)), freeController(cameraEntity) {}

private:
  kt::Window& window;
  kt::Scene scene;
  kt::cameras::FreeCameraController freeController;
};

std::expected<void, std::string> kt::setupAppLayers(core::layers::LayerStack& layerStack, core::window::Window& window,
                                                    kt::rendering::Renderer& renderer) {
  kt::Scene scene{};
  auto bistroMeshRes = renderer.loadMesh(ASSET_DIR "meshes/BistroExterior.gltf");
  if (!bistroMeshRes) {
    return std::unexpected(fmt::format("Failed to load bistro mesh: {}", bistroMeshRes.error()));
  }

  auto bistro = scene.createEntity("Bistro");
  bistroMeshRes->addToEcsScene(scene, bistro.getHandle());

  auto camera = scene.createEntity("Camera");
  auto& camTransform = camera.addComponent<kt::components::Transform>();
  auto& localTransform = camTransform.getLocalMut();
  localTransform.translate(glm::vec3(0.0f, 0.0f, 5.0f)).lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  auto& camComp =
      camera.addComponent<kt::components::Camera>(kt::components::PerspectiveType::Standard, kt::components::Camera::Params::Common{},
                                                  kt::components::Camera::Params::Perspective{.fovY = glm::radians(90.f)});
  camComp.sizeToWindowSize(window.getRenderSize());

  scene.useCamera(camera);

  auto pointLight = scene.createEntity("Point Light");
  auto& lightTransform = pointLight.addComponent<kt::components::Transform>();
  lightTransform.getLocalMut().translate(glm::vec3(70.0f, 70.0f, -10.0f));
  pointLight.addComponent<kt::components::PointLight>(kt::components::PointLight{
      .color = {1.f, 0.985f, 0.95f},
      .intensity = 3.f,
      .radius = 500.f,
  });

  layerStack.emplaceLayer<BenchmarkLayer>(window, renderer, std::move(scene), camera);

  return {};
}
