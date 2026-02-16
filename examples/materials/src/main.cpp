#include "keptech/core/components/transform.hpp"
#include <keptech/app.hpp>

#include "editorLayer.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/ecs/entity.hpp"
#include <expected>
#include <keptech/components.hpp>
#include <keptech/core/gui.h>
#include <keptech/core/window.hpp>
#include <keptech/keptech.hpp>
#include <keptech/vulkan.hpp>
#include <memory>
#include <utility>

namespace shaders {
#include "shaders/basic.h"
} // namespace shaders

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

keptech::SetupInfo keptech::configureApp() {
  return {.window = {.title = "Material Editor",
                     .width = WINDOW_WIDTH,
                     .height = WINDOW_HEIGHT},
          .renderer = {.applicationName = "Material Editor"}};
}

std::expected<void, std::string>
keptech::setupAppLayers(core::layers::LayerStack& layerStack,
                        core::window::Window& window,
                        keptech::Renderer& renderer) {
  MaterialEditorLayer::initMeta();

  std::unique_ptr scene = std::make_unique<keptech::Scene>();

  auto bistroMeshRes = renderer.loadMesh(ASSET_DIR "meshes/BistroExterior.glb");
  if (!bistroMeshRes) {
    return std::unexpected(
        fmt::format("Failed to load bistro mesh: {}", bistroMeshRes.error()));
  }

  std::vector<MeshPtr> allMeshes{};
  allMeshes.insert(allMeshes.end(), bistroMeshRes.value().meshes.begin(),
                   bistroMeshRes.value().meshes.end());

  auto bistro = scene->createEntity("Bistro");
  bistroMeshRes->addToEcsScene(*scene, bistro.getHandle());

  auto camera = scene->createEntity("Camera");
  auto& camTransform = camera.addComponent<keptech::components::Transform>();
  auto& localTransform = camTransform.getLocalMut();
  localTransform.translate(glm::vec3(0.0f, 0.0f, 5.0f))
      .lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  auto& camComp = camera.addComponent<keptech::components::Camera>(
      keptech::components::PerspectiveType::Standard,
      keptech::components::Camera::Params::Common{},
      keptech::components::Camera::Params::Perspective{.fovY =
                                                           glm::radians(90.f)});
  camComp.sizeToWindowSize(window.getRenderSize());

  scene->useCamera(camera);

  layerStack.emplaceLayer<MaterialEditorLayer>(
      renderer, std::move(scene), std::move(allMeshes),
      std::vector<keptech::PipelinePtr>{});

  return {};
}
