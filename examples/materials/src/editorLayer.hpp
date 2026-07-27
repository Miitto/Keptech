#pragma once

#include "keptech/cameras/freeCamera.hpp"
#include "keptech/components.hpp"
#include "keptech/core/events/event.hpp"
#include <keptech/app.hpp>

#include "keptech/ecs/entity.hpp"
#include "keptech/rendering/pipeline.hpp"
#include <keptech/components.hpp>
#include <keptech/core/gui.h>
#include <keptech/core/window.hpp>
#include <keptech/keptech.hpp>
#include <keptech/render.hpp>

class MaterialEditorLayer;

class MaterialEditorLayer : public kt::core::layers::Layer {
public:
  using SelectedItem = std::variant<std::monostate, kt::ecs::EntityHandle, kt::MeshPtr, kt::PipelinePtr, kt::ImgPtr>;

  enum class ActiveDebugView : uint8_t { Albedo, Normals, EmissiveAo, MetallicRoughness, Depth, Diffuse, Specular, Final };

  MaterialEditorLayer(kt::Window& window, kt::Renderer& renderer, std::unique_ptr<kt::Scene>&& scene, std::vector<kt::MeshPtr>&& meshes,
                      std::vector<kt::PipelinePtr>&& pipelines);

  static void initMeta();

  kt::Scene& getScene() { return *scene; }

  void onUpdate(kt::Timestep ts) final;

  void onEvent(kt::core::events::Event& event, kt::Timestep ts) final {
    if (freeController.handleEvent(event, ts))
      return;
  }

  void inspectorUi(kt::gui::Frame& frame, kt::components::Mesh& ro);

  void inspectorUi(kt::gui::Frame& frame, kt::components::Material& ro);

  void inspectorUi(kt::gui::Frame& frame, kt::PipelinePtr& ro);

  void inspectorUi(kt::gui::Frame& frame, kt::components::Camera& camera);

  void inspectorUi(kt::gui::Frame& frame, kt::components::PointLight& light);

  struct SceneNode {
    kt::ecs::EntityHandle id;
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
  void addNodeToList(kt::ecs::EntityHandle entity, kt::components::Name& name,
                     std::vector<std::unique_ptr<MaterialEditorLayer::SceneNode>>& roots,
                     std::unordered_map<kt::ecs::EntityHandle, MaterialEditorLayer::SceneNode*>& nodeMap);
  void drawSceneNodeInTree(SceneNode& node);
  /// Returns true if the node was deleted
  bool drawSceneTreeEntityContextMenu(SceneNode& node);

  bool reloadShader(kt::PipelinePtr& pipeline);

  void drawSelectedProperties();
  void drawEntityProperties(kt::gui::Frame& frame, kt::ecs::EntityHandle entity);

  kt::Window& window;
  kt::Renderer& renderer;
  std::unique_ptr<kt::Scene> scene;
  kt::cameras::FreeCameraController freeController;

  SelectedItem selectedItem = std::monostate{};

  Directory assetsRootDir;

  ActiveDebugView activeDebugView = ActiveDebugView::Final;
};

template <typename Comp> void forwardCompInspectorUi(MaterialEditorLayer* layer, kt::gui::Frame* frame, kt::ecs::EntityHandle entity) {
  auto& comp = layer->getScene().getEcs().get<Comp>(entity);
  layer->inspectorUi(*frame, comp);
}
