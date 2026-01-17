#include "editorLayer.hpp"
#include "keptech/core/components/transform.hpp"
#include "keptech/ecs/entity.hpp"

void MaterialEditorLayer::onUpdate(keptech::core::Timestep ts) {
  {
    auto frame = keptech::gui::Frame("Stats");
    double fps = static_cast<double>(1000.f / ts);
    frame.text("FPS: %.1f", fps);

    frame.inputFloat("Sensitivity", orbitController.getSensitivity());
  }

  drawGui();

  renderer.submitScene(scene);
}

void MaterialEditorLayer::drawGui() {
  ImGuiID dockspace_id = ImGui::GetID("Editor Dock");
  ImGuiViewport* viewport = ImGui::GetMainViewport();

  // Create settings
  if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
    ImGuiID dock_id_left = 0;
    ImGuiID dock_id_right = 0;
    ImGuiID dock_id_bottom = 0;
    ImGuiID dock_id_main = dockspace_id;
    ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.20f,
                                &dock_id_right, &dock_id_main);
    ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.20f,
                                &dock_id_bottom, &dock_id_main);
    ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f,
                                &dock_id_left, &dock_id_main);
    ImGui::DockBuilderDockWindow("Game", dock_id_main);
    ImGui::DockBuilderDockWindow("Scene Tree", dock_id_left);
    ImGui::DockBuilderDockWindow("Properties", dock_id_right);
    ImGui::DockBuilderDockWindow("Assets", dock_id_bottom);
    ImGui::DockBuilderFinish(dockspace_id);
  }

  // Submit dockspace
  ImGui::DockSpaceOverViewport(dockspace_id, viewport,
                               ImGuiDockNodeFlags_PassthruCentralNode);

  drawSceneTree();
  drawEntityProperties();
  drawAssetsPanel();
}

namespace {
  struct SceneNode {
    keptech::ecs::EntityHandle id;
    const std::string* name;
    std::vector<SceneNode*> children{};

    ~SceneNode() {
      for (auto& child : children) {
        delete child;
      }
    }
  };
  void addNodeToList(
      keptech::ecs::EntityHandle entity, const keptech::components::Name& name,
      std::vector<SceneNode*>& roots,
      std::unordered_map<keptech::ecs::EntityHandle, SceneNode*>& nodeMap,
      keptech::core::Scene& scene) {
    if (nodeMap.find(entity) != nodeMap.end())
      return;

    SceneNode* node = new SceneNode{.id = entity, .name = &name.name};
    nodeMap.emplace(entity, node);

    auto transform =
        scene.getEcs().try_get<keptech::components::Transform>(entity);
    if (transform != nullptr && transform->getParent().getHandle() !=
                                    keptech::ecs::INVALID_ENTITY_HANDLE) {
      auto parentId = transform->getParent().getHandle();

      if (nodeMap.find(parentId) == nodeMap.end()) {
        auto nameComp = scene.getEcs().get<keptech::components::Name>(parentId);
        addNodeToList(parentId, nameComp, roots, nodeMap, scene);
      }

      nodeMap[parentId]->children.push_back(node);
    } else {
      roots.push_back(node);
    }
  }

  void displaySceneNodeInTree(SceneNode& node,
                              keptech::ecs::EntityHandle& selectedEntity) {
    ImGui::PushID(node.name);
    bool hasChildren = !node.children.empty();

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
    if (!hasChildren)
      flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Leaf;

    if (selectedEntity == node.id) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (ImGui::TreeNodeEx(node.name->c_str(), flags)) {
      if (hasChildren) {
        for (auto& child : node.children) {
          displaySceneNodeInTree(*child, selectedEntity);
        }
        ImGui::TreePop();
      }
    }
    if (ImGui::IsItemClicked()) {
      selectedEntity = node.id;
    }

    ImGui::PopID();
  }
} // namespace

void MaterialEditorLayer::drawSceneTree() {
  auto scenePanel = keptech::gui::Frame("Scene Tree", nullptr,
                                        ImGuiWindowFlags_NoDecoration |
                                            ImGuiWindowFlags_NoMove);

  std::vector<SceneNode*> roots;
  {
    std::unordered_map<keptech::ecs::EntityHandle, SceneNode*> nodeMap;

    auto view = scene.getEcs().view<keptech::components::Name>();
    for (auto [entity, name] : view.each()) {
      addNodeToList(entity, name, roots, nodeMap, scene);
    }
  }

  for (auto& root : roots) {
    displaySceneNodeInTree(*root, selectedEntity);
  }
  for (auto& root : roots) {
    delete root;
  }
}

void MaterialEditorLayer::drawEntityProperties() {
  auto propertiesPanel = keptech::gui::Frame("Properties", nullptr,
                                             ImGuiWindowFlags_NoDecoration |
                                                 ImGuiWindowFlags_NoMove);

  if (selectedEntity == keptech::ecs::INVALID_ENTITY_HANDLE)
    return;

  auto& ecs = scene.getEcs();
  auto entity = keptech::ecs::Entity(selectedEntity, ecs);

  if (entity.hasAllComponents<keptech::components::Transform>()) {
    static keptech::core::Bitflag<
        keptech::components::Transform::TransformGuiFlags>
        flags = keptech::components::Transform::TransformGuiFlags::Editable;
    entity.getComponents<keptech::components::Transform>().guiPane(
        propertiesPanel, flags);
  }
}

void MaterialEditorLayer::drawAssetsPanel() {
  auto assetsPanel = keptech::gui::Frame("Assets", nullptr,
                                         ImGuiWindowFlags_NoDecoration |
                                             ImGuiWindowFlags_NoMove);
}
