#include "editorLayer.hpp"
#include "keptech/core/components/transform.hpp"
#include "keptech/ecs/entity.hpp"

#include <spdlog/fmt/bundled/format.h>

template <>
struct fmt::formatter<MaterialEditorLayer::ActiveDebugView>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const MaterialEditorLayer::ActiveDebugView& view,
              FormatContext& ctx) const {
    std::string_view name = "";
    switch (view) {
    case MaterialEditorLayer::ActiveDebugView::Albedo:
      name = "Albedo";
      break;
    case MaterialEditorLayer::ActiveDebugView::Normals:
      name = "Normals";
      break;
    case MaterialEditorLayer::ActiveDebugView::Depth:
      name = "Depth";
      break;
    case MaterialEditorLayer::ActiveDebugView::Final:
      name = "Final";
      break;
    }
    return formatter<std::string_view>::format(name, ctx);
  }
};

void MaterialEditorLayer::onUpdate(keptech::core::Timestep ts) {
  {
    auto frame = keptech::gui::Frame("Stats");
    double fps = static_cast<double>(1000.f / ts);
    frame.text("FPS: %.1f", fps);

    frame.inputFloat("Sensitivity", orbitController.getSensitivity());
  }

  drawGui();
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
    ImGuiID dock_id_toolbar = 0;
    ImGuiID dock_id_main = dockspace_id;
    ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.20f,
                                &dock_id_right, &dock_id_main);
    ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.20f,
                                &dock_id_bottom, &dock_id_main);
    ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f,
                                &dock_id_left, &dock_id_main);
    ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Up, 0.05f,
                                &dock_id_toolbar, &dock_id_main);
    ImGui::DockBuilderSetNodeSize(
        dock_id_toolbar, ImVec2(viewport->Size.x, ImGui::GetFontSize() * 2.5f));

    ImGui::DockBuilderDockWindow("Toolbar", dock_id_toolbar);
    ImGui::DockBuilderDockWindow("Game", dock_id_main);
    ImGui::DockBuilderDockWindow("Scene Tree", dock_id_left);
    ImGui::DockBuilderDockWindow("Properties", dock_id_right);
    ImGui::DockBuilderDockWindow("Assets", dock_id_bottom);
    ImGui::DockBuilderFinish(dockspace_id);
  }

  ImGui::DockSpaceOverViewport(dockspace_id, viewport,
                               ImGuiDockNodeFlags_PassthruCentralNode);

  {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    auto gamePanel = keptech::gui::Frame("Game", nullptr,
                                         ImGuiWindowFlags_NoDecoration |
                                             ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(1);

    if (ImGui::IsWindowHovered()) {
      auto& io = ImGui::GetIO();
      io.WantCaptureMouse = false;
      ImGui::SetNextFrameWantCaptureMouse(false);
    }

    auto& gbuffer = renderer.getImGuiGBufferHandles();

    ImVec2 size = ImGui::GetContentRegionAvail();

    switch (activeDebugView) {
    case ActiveDebugView::Final:
    case ActiveDebugView::Albedo:
      ImGui::Image(gbuffer.albedo, size);
      break;
    case ActiveDebugView::Normals:
      ImGui::Image(gbuffer.normal, size);
      break;
    case ActiveDebugView::Depth:
      ImGui::Image(gbuffer.depth, size);
      break;
    }
  }

  drawToolbar();
  drawSceneTree();
  drawEntityProperties();
  drawAssetsPanel();
}

void MaterialEditorLayer::drawToolbar() {
  ImGuiWindowClass window_class;
  window_class.DockingAllowUnclassed = true;
  window_class.DockNodeFlagsOverrideSet |=
      ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_HiddenTabBar |
      ImGuiDockNodeFlags_NoDockingOverMe |
      ImGuiDockNodeFlags_NoDockingOverOther | ImGuiDockNodeFlags_NoResizeY |
      ImGuiDockNodeFlags_NoResizeX;
  ImGui::SetNextWindowClass(&window_class);
  auto toolbarPanel = keptech::gui::Frame(
      "Toolbar", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
          ImGuiWindowFlags_NoScrollbar);

  {
    std::string debugViewStr = fmt::format("{}", activeDebugView);
    auto combo = toolbarPanel.combo("Debug View", debugViewStr.c_str());
    if (combo.item("Final", activeDebugView == ActiveDebugView::Final)) {
      activeDebugView = ActiveDebugView::Final;
    }
    if (combo.item("Albedo", activeDebugView == ActiveDebugView::Albedo)) {
      activeDebugView = ActiveDebugView::Albedo;
    }
    if (combo.item("Normals", activeDebugView == ActiveDebugView::Normals)) {
      activeDebugView = ActiveDebugView::Normals;
    }
    if (combo.item("Depth", activeDebugView == ActiveDebugView::Depth)) {
      activeDebugView = ActiveDebugView::Depth;
    }
  }
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
