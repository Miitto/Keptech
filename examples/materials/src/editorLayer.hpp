#pragma once

#include "keptech/cameras/orbitCamera.hpp"
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

template <typename Comp>
void forwardCompInspectorUi(MaterialEditorLayer* layer,
                            keptech::gui::Frame* frame,
                            keptech::ecs::EntityHandle entity);

class MaterialEditorLayer : public keptech::core::layers::Layer {
public:
  using SelectedItem = std::variant<std::monostate, keptech::ecs::EntityHandle,
                                    keptech::core::rendering::Mesh::Handle,
                                    keptech::core::rendering::Material::Handle>;

  enum class ActiveDebugView : uint8_t { Albedo, Normals, Depth, Final };

  MaterialEditorLayer(KEPTECH_RENDERER& renderer, keptech::core::Scene&& scene);

  static void initMeta();

  keptech::core::Scene& getScene() { return scene; }

  void onUpdate(keptech::core::Timestep ts) override;

  void onEvent(keptech::core::events::Event& event,
               keptech::core::Timestep ts) override {
    if (orbitController.handleEvent(event, ts))
      return;
  }

  void meshInspectorUi(keptech::gui::Frame& frame,
                       keptech::components::Mesh& ro);

  void meshInspectorUi(keptech::gui::Frame& frame, KEPTECH_RENDERER::Mesh& ro);

  void materialInspectorUi(keptech::gui::Frame& frame,
                           keptech::components::Material& ro);

  void materialInspectorUi(keptech::gui::Frame& frame,
                           KEPTECH_RENDERER::Material& ro);

  struct SceneNode {
    keptech::ecs::EntityHandle id;
    std::string* name;
    SceneNode* parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children{};
    bool hasChildSelected = false;
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

  void drawSelectedProperties();
  void drawEntityProperties(keptech::gui::Frame& frame,
                            keptech::ecs::EntityHandle entity);
  void drawAssetsPanel();
  void drawLoadedAssetsPanel();

  KEPTECH_RENDERER& renderer;
  keptech::core::Scene scene;
  keptech::cameras::OrbitCameraController orbitController;

  SelectedItem selectedItem = std::monostate{};

  ActiveDebugView activeDebugView = ActiveDebugView::Final;
};
