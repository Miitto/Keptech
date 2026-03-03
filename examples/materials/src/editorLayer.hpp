#pragma once

#include "keptech/cameras/freeCamera.hpp"
#include "keptech/core/components/renderObject.hpp"
#include "keptech/core/components/transform.hpp"
#include "keptech/core/events/event.hpp"
#include <keptech/app.hpp>

#include "keptech/ecs/entity.hpp"
#include <keptech/components.hpp>
#include <keptech/core/gui.h>
#include <keptech/core/window.hpp>
#include <keptech/keptech.hpp>
#include <keptech/vulkan.hpp>

class MaterialEditorLayer;

class MaterialEditorLayer : public keptech::core::layers::Layer {
public:
  using SelectedItem =
      std::variant<std::monostate, keptech::ecs::EntityHandle, keptech::MeshPtr,
                   keptech::PipelinePtr, keptech::ImgPtr>;

  enum class ActiveDebugView : uint8_t {
    Albedo,
    Normals,
    EmissiveAo,
    MetallicRoughness,
    Depth,
    Diffuse,
    Specular,
    Final
  };

  MaterialEditorLayer(keptech::Window& window, keptech::Renderer& renderer,
                      std::unique_ptr<keptech::Scene>&& scene,
                      std::vector<keptech::MeshPtr>&& meshes,
                      std::vector<keptech::PipelinePtr>&& pipelines);

  static void initMeta();

  keptech::Scene& getScene() { return *scene; }

  void onUpdate(keptech::Timestep ts) final;

  void onEvent(keptech::core::events::Event& event,
               keptech::Timestep ts) final {
    if (freeController.handleEvent(event, ts))
      return;
  }

  void inspectorUi(keptech::gui::Frame& frame, keptech::components::Mesh& ro);

  void inspectorUi(keptech::gui::Frame& frame,
                   keptech::components::Material& ro);

  void inspectorUi(keptech::gui::Frame& frame, keptech::PipelinePtr& ro);

  void inspectorUi(keptech::gui::Frame& frame,
                   keptech::components::Camera& camera);

  void inspectorUi(keptech::gui::Frame& frame,
                   keptech::components::PointLight& light);

  struct SceneNode {
    keptech::ecs::EntityHandle id;
    std::string* name;
    SceneNode* parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children{};
    bool hasChildSelected = false;
  };

  enum class FileType : uint8_t {
    Unknown,
    Image,
    Mesh,
  };

  struct File {
    std::string name;
    FileType type;
  };

  struct Directory {
    std::string name;
    std::vector<File> files;
    std::vector<Directory> directories;
  };

private:
  void initDocks();
  void drawGui();
  void drawViewport();
  void drawToolbar();
  void drawSceneTree();
  void addNodeToList(
      keptech::ecs::EntityHandle entity, keptech::components::Name& name,
      std::vector<std::unique_ptr<MaterialEditorLayer::SceneNode>>& roots,
      std::unordered_map<keptech::ecs::EntityHandle,
                         MaterialEditorLayer::SceneNode*>& nodeMap);
  void drawSceneNodeInTree(SceneNode& node);
  /// Returns true if the node was deleted
  bool drawSceneTreeEntityContextMenu(SceneNode& node);

  bool reloadShader(keptech::PipelinePtr& pipeline);

  void drawSelectedProperties();
  void drawEntityProperties(keptech::gui::Frame& frame,
                            keptech::ecs::EntityHandle entity);
  void refreshAssetsDirectory();
  void drawAssetsDirectory(keptech::gui::Frame& frame,
                           const Directory& directory, float depth);
  void drawAssetsPanel();
  void drawLoadedAssetsPanel();

  keptech::Window& window;
  keptech::Renderer& renderer;
  std::unique_ptr<keptech::Scene> scene;
  keptech::cameras::FreeCameraController freeController;

  SelectedItem selectedItem = std::monostate{};

  std::vector<keptech::MeshPtr> loadedMeshes;
  std::vector<keptech::PipelinePtr> loadedPipelines;

  Directory assetsRootDir;

  ActiveDebugView activeDebugView = ActiveDebugView::Final;
};

template <typename Comp>
void forwardCompInspectorUi(MaterialEditorLayer* layer,
                            keptech::gui::Frame* frame,
                            keptech::ecs::EntityHandle entity) {
  auto& comp = layer->getScene().getEcs().get<Comp>(entity);
  layer->inspectorUi(*frame, comp);
}
