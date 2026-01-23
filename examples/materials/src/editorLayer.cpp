#include "editorLayer.hpp"

#include "imgui.h"
#include "keptech/core/components/transform.hpp"
#include "keptech/core/slotmap.hpp"
#include "keptech/ecs/entity.hpp"
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <keptech/core/rendering/material.hpp>

#include <spdlog/fmt/bundled/format.h>

template <>
void forwardCompInspectorUi<keptech::components::Mesh>(
    MaterialEditorLayer* layer, keptech::gui::Frame* frame,
    keptech::ecs::EntityHandle entity) {
  auto& comp =
      layer->getScene().getEcs().get<keptech::components::Mesh>(entity);
  layer->meshInspectorUi(*frame, comp);
}

template <>
void forwardCompInspectorUi<keptech::components::Material>(
    MaterialEditorLayer* layer, keptech::gui::Frame* frame,
    keptech::ecs::EntityHandle entity) {

  auto& comp =
      layer->getScene().getEcs().get<keptech::components::Material>(entity);
  layer->materialInspectorUi(*frame, comp);
}

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

namespace {
  template <typename Comp> void metaFunc() {
    keptech::ecs::MetaFactory<Comp>() // NOLINT
        .template func<&forwardCompInspectorUi<Comp>>(
            entt::hashed_string("inspectorUi"));
  }
} // namespace

void MaterialEditorLayer::initMeta() {
  metaFunc<keptech::components::Mesh>();
  metaFunc<keptech::components::Material>();
}

MaterialEditorLayer::MaterialEditorLayer(KEPTECH_RENDERER& renderer,
                                         keptech::core::Scene&& scene)
    : keptech::core::layers::Layer("MaterialEditorLayer"), renderer(renderer),
      scene(std::move(scene)), orbitController(this->scene.getActiveCamera()) {
  renderer.setScene(this->scene);
}

void MaterialEditorLayer::onUpdate(keptech::core::Timestep ts) {
  if (auto frame = keptech::gui::Frame("Stats"); frame.isOpen()) {
    double fps = static_cast<double>(1000.f / ts);
    frame.text("FPS: %.1f", fps);

    frame.inputFloat("Sensitivity", orbitController.getSensitivity());
  }

  drawGui();
}

void MaterialEditorLayer::initDocks() {
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
    ImGui::DockBuilderDockWindow("Loaded Assets", dock_id_bottom);
    ImGui::DockBuilderFinish(dockspace_id);
  }

  ImGui::DockSpaceOverViewport(dockspace_id, viewport,
                               ImGuiDockNodeFlags_PassthruCentralNode);
}

void MaterialEditorLayer::drawGui() {
  initDocks();

  drawViewport();
  drawToolbar();
  drawSceneTree();
  drawSelectedProperties();
  drawAssetsPanel();
  drawLoadedAssetsPanel();
}

void MaterialEditorLayer::drawViewport() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  auto gamePanel = keptech::gui::Frame("Game", nullptr,
                                       ImGuiWindowFlags_NoDecoration |
                                           ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoScrollbar);

  if (!gamePanel.isOpen()) {
    ImGui::PopStyleVar(1);
    return;
  }

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
  ImGui::PopStyleVar(1);
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

  if (!toolbarPanel.isOpen())
    return;

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

void MaterialEditorLayer::addNodeToList(
    keptech::ecs::EntityHandle entity, keptech::components::Name& name,
    std::vector<std::unique_ptr<MaterialEditorLayer::SceneNode>>& roots,
    std::unordered_map<keptech::ecs::EntityHandle,
                       MaterialEditorLayer::SceneNode*>& nodeMap) {
  if (nodeMap.find(entity) != nodeMap.end())
    return;

  std::unique_ptr node = std::make_unique<MaterialEditorLayer::SceneNode>(
      MaterialEditorLayer::SceneNode{.id = entity, .name = &name.name});
  nodeMap.emplace(entity, node.get());

  auto transform =
      scene.getEcs().try_get<keptech::components::Transform>(entity);
  if (transform != nullptr && transform->getParent().getHandle() !=
                                  keptech::ecs::INVALID_ENTITY_HANDLE) {
    auto parentId = transform->getParent().getHandle();

    if (nodeMap.find(parentId) == nodeMap.end()) {
      auto& nameComp = scene.getEcs().get<keptech::components::Name>(parentId);
      addNodeToList(parentId, nameComp, roots, nodeMap);
    }

    node->parent = nodeMap[parentId];

    nodeMap[parentId]->children.push_back(std::move(node));
  } else {
    roots.push_back(std::move(node));
  }
}

void MaterialEditorLayer::drawSceneNodeInTree(SceneNode& node) {
  ImGui::PushID(node.name);
  bool hasChildren = !node.children.empty();

  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
  if (!hasChildren)
    flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Leaf;

  if (selectedItem.index() == 1) {
    keptech::ecs::EntityHandle selectedEntity =
        std::get<keptech::ecs::EntityHandle>(selectedItem);
    if (selectedEntity == node.id)
      flags |= ImGuiTreeNodeFlags_Selected;
  }

  if (node.hasChildSelected) {
    ImGui::SetNextItemOpen(true, ImGuiCond_Always);
  }
  auto open = ImGui::TreeNodeEx(node.name->c_str(), flags);
  if (ImGui::IsItemClicked()) {
    selectedItem = node.id;
  }

  if (drawSceneTreeEntityContextMenu(node)) {
    if (open && hasChildren) {
      ImGui::TreePop();
    }
    ImGui::PopID();
    return;
  }

  if (open && hasChildren) {
    for (auto& child : node.children) {
      drawSceneNodeInTree(*child);
    }
    ImGui::TreePop();
  }

  ImGui::PopID();
}

bool MaterialEditorLayer::drawSceneTreeEntityContextMenu(SceneNode& node) {
  // Need to double buffer the string, since ImGui uses the name as the
  // widget ID. If the label changes, the ID changes, and the popup closes
  // immediately - i.e. whenever you'd type.
  struct RenameBuffer {
    keptech::ecs::EntityHandle entity;
    std::string newName;
  };

  static std::optional<RenameBuffer> renameBuffer;

  if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight |
                                                ImGuiPopupFlags_NoReopen)) {
    if (!renameBuffer.has_value() || renameBuffer->entity != node.id) {
      renameBuffer = RenameBuffer{
          .entity = node.id,
          .newName = *node.name,
      };
    }
    if (ImGui::InputText("Rename", &renameBuffer->newName,
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      node.name->assign(renameBuffer->newName);
    }

    if (ImGui::Button("Create Child")) {
      auto thisEntity = keptech::ecs::Entity(node.id, scene.getEcs());

      if (!thisEntity.hasAllComponents<keptech::components::Transform>()) {
        thisEntity.addComponent<keptech::components::Transform>();
      }

      auto child = scene.createEntity("Unnamed Entity");
      auto& transform = child.addComponent<keptech::components::Transform>();
      transform.setParent(keptech::ecs::Entity(node.id, scene.getEcs()));
      selectedItem = child.getHandle();
      ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Create Sibling")) {
      auto sibling = scene.createEntity("Unnamed Entity");
      if (node.parent != nullptr) {
        auto& transform =
            sibling.addComponent<keptech::components::Transform>();
        transform.setParent(
            keptech::ecs::Entity(node.parent->id, scene.getEcs()));
      }
      selectedItem = sibling.getHandle();
      ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Delete")) {
      keptech::ecs::Entity(node.id, scene.getEcs()).destroy();
      if (selectedItem.index() == 1) {
        keptech::ecs::EntityHandle selectedEntity =
            std::get<keptech::ecs::EntityHandle>(selectedItem);
        if (selectedEntity == node.id) {
          selectedItem = std::monostate{};
        }
      }
      for (auto& child : node.children) {
        auto e = keptech::ecs::Entity(child->id, scene.getEcs());
        auto& transform = e.getComponents<keptech::components::Transform>();
        transform.setParent(keptech::ecs::Entity{});
      }
      renameBuffer.reset();
      ImGui::EndPopup();
      return true;
    }

    ImGui::EndPopup();
  } else if (renameBuffer.has_value() && renameBuffer->entity == node.id) {
    renameBuffer.reset();
  }
  return false;
}

void MaterialEditorLayer::drawSceneTree() {
  auto scenePanel = keptech::gui::Frame("Scene Tree", nullptr,
                                        ImGuiWindowFlags_NoDecoration |
                                            ImGuiWindowFlags_NoMove);
  if (!scenePanel.isOpen())
    return;

  if (ImGui::BeginPopupContextWindow()) {

    if (ImGui::Button("New Entity")) {
      scene.createEntity("Unnamed Entity");
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  std::vector<std::unique_ptr<SceneNode>> roots;
  {
    std::unordered_map<keptech::ecs::EntityHandle, SceneNode*> nodeMap;

    auto view = scene.getEcs().view<keptech::components::Name>();
    for (auto [entity, name] : view.each()) {
      addNodeToList(entity, name, roots, nodeMap);
    }

    if (selectedItem.index() == 1) {
      keptech::ecs::EntityHandle selectedEntity =
          std::get<keptech::ecs::EntityHandle>(selectedItem);
      auto it = nodeMap.find(selectedEntity);
      if (it != nodeMap.end()) {
        SceneNode* node = it->second;
        while (node->parent != nullptr) {
          node->parent->hasChildSelected = true;
          node = node->parent;
        }
      }
    }
  }

  for (auto& root : roots) {
    drawSceneNodeInTree(*root);
  }
}

void MaterialEditorLayer::drawSelectedProperties() {
  auto propertiesPanel = keptech::gui::Frame("Properties", nullptr,
                                             ImGuiWindowFlags_NoDecoration |
                                                 ImGuiWindowFlags_NoMove);
  if (!propertiesPanel.isOpen())
    return;

  std::visit(
      keptech::core::overloaded{
          [&](std::monostate) {},
          [&](keptech::ecs::EntityHandle entity) {
            drawEntityProperties(propertiesPanel, entity);
          },
          [&](keptech::core::rendering::Mesh::Handle meshHandle) {
            auto meshPtr = renderer.getMeshData(meshHandle);
            if (meshPtr == nullptr) {
              propertiesPanel.separatorText("Invalid Mesh");
              return;
            }
            auto label = fmt::format("Mesh: {}", meshPtr->getName());
            propertiesPanel.separatorText(label.c_str());
            meshInspectorUi(propertiesPanel, *meshPtr);
          },
          [&](keptech::core::rendering::Material::Handle materialHandle) {
            auto materialPtr = renderer.getMaterialData(materialHandle);
            if (materialPtr == nullptr) {
              propertiesPanel.separatorText("Invalid Material");
              return;
            }

            auto label = fmt::format("Material: {}", materialPtr->name);
            propertiesPanel.separatorText(label.c_str());
            materialInspectorUi(propertiesPanel, *materialPtr);
          },
      },
      selectedItem);
}

void MaterialEditorLayer::drawEntityProperties(
    keptech::gui::Frame& propertiesPanel,
    keptech::ecs::EntityHandle selectedEntity) {
  auto& ecs = scene.getEcs();
  auto entity = keptech::ecs::Entity(selectedEntity, ecs);

  auto transform =
      entity.getEcs().try_get<keptech::components::Transform>(entity);
  if (transform != nullptr) {
    transform->inspectorUi(propertiesPanel, false);
  }

  for (auto [id, storage] : ecs.storage()) {
    auto type = entt::resolve(storage.info());

    auto func = type.func(entt::hashed_string("inspectorUi"));
    if (func) {
      if (storage.contains(entity.getHandle())) {
        func.invoke({}, this, &propertiesPanel, entity.getHandle());
      }
    }
  }
  ImGui::Separator();

  ImGui::Button("Add Component");

  if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft |
                                                ImGuiPopupFlags_NoReopen)) {

    if (!entity.hasAllComponents<keptech::components::Transform>()) {
      if (ImGui::Button("Transform")) {
        entity.addComponent<keptech::components::Transform>();
        ImGui::CloseCurrentPopup();
      }
    }

    if (!entity.hasAllComponents<keptech::components::Mesh>()) {
      if (ImGui::Button("Mesh")) {
        entity.addComponent<keptech::components::Mesh>();
        ImGui::CloseCurrentPopup();
      }
    }

    if (!entity.hasAllComponents<keptech::components::Material>()) {
      if (ImGui::Button("Material")) {
        entity.addComponent<keptech::components::Material>();
        ImGui::CloseCurrentPopup();
      }
    }

    ImGui::EndPopup();
  }
}

void MaterialEditorLayer::drawAssetsPanel() {
  auto assetsPanel = keptech::gui::Frame("Assets", nullptr,
                                         ImGuiWindowFlags_NoDecoration |
                                             ImGuiWindowFlags_NoMove);
  if (!assetsPanel.isOpen())
    return;
}

void MaterialEditorLayer::drawLoadedAssetsPanel() {
  auto loadedAssetsPanel = keptech::gui::Frame("Loaded Assets", nullptr,
                                               ImGuiWindowFlags_NoDecoration |
                                                   ImGuiWindowFlags_NoMove);
  if (!loadedAssetsPanel.isOpen())
    return;

  enum class AssetType : uint8_t { Mesh, Material, Texture };
  static AssetType activeAssetType = AssetType::Mesh;

  ImVec2 textSize = ImGui::CalcTextSize("Materials");

  {
    auto child = loadedAssetsPanel.child("Asset Type Selector",
                                         ImVec2(textSize.x + 10.f, 0));
    if (child.selectable("Meshes", activeAssetType == AssetType::Mesh)) {
      activeAssetType = AssetType::Mesh;
    }
    if (child.selectable("Materials", activeAssetType == AssetType::Material)) {
      activeAssetType = AssetType::Material;
    }
    if (child.selectable("Textures", activeAssetType == AssetType::Texture)) {
      activeAssetType = AssetType::Texture;
    }
  }
  {
    loadedAssetsPanel.sameLine();
    auto child = loadedAssetsPanel.child("Asset List");

    auto winSize = ImGui::GetContentRegionAvail();

    int cols = std::clamp(static_cast<int>(winSize.x / 80.f), 1, 511);

    if (ImGui::BeginTable("Assets Table", cols, 0, {0, 0}, 5.f)) {
      switch (activeAssetType) {
      case AssetType::Mesh: {
        renderer.operateOnAllMeshes(
            [&](keptech::core::rendering::Mesh::Handle handle,
                KEPTECH_RENDERER::Mesh& mesh) {
              ImGui::TableNextColumn();

              bool selected = false;
              if (selectedItem.index() == 2) {
                selected = std::get<keptech::core::rendering::Mesh::Handle>(
                               selectedItem) == handle;
              }

              if (child.selectable(mesh.getName().c_str(), selected)) {
                selectedItem = handle;
              }
            });
      } break;
      case AssetType::Material: {
        renderer.operateOnAllMaterials(
            [&](keptech::core::rendering::Material::Handle handle,
                KEPTECH_RENDERER::Material& material) {
              ImGui::TableNextColumn();

              bool selected = false;
              if (selectedItem.index() == 3) {
                selected = std::get<keptech::core::rendering::Material::Handle>(
                               selectedItem) == handle;
              }

              if (child.selectable(material.name.c_str(), selected)) {
                selectedItem = handle;
              }
            });

      } break;
      case AssetType::Texture:
        break;
      }
      ImGui::EndTable();
    }
  }
}

void MaterialEditorLayer::meshInspectorUi(keptech::gui::Frame& frame,
                                          keptech::components::Mesh& mesh) {
  frame.separatorText("Mesh");

  auto meshPtr = renderer.getMeshData(mesh);

  const char* meshName = (meshPtr != nullptr) ? meshPtr->getName().c_str() : "";

  {
    auto combo = frame.combo("Mesh", meshName);
    renderer.operateOnAllMeshes(
        [&](keptech::core::rendering::Mesh::Handle handle,
            KEPTECH_RENDERER::Mesh& m) {
          if (combo.item(m.getName().c_str(), mesh == handle)) {
            mesh = handle;
          }
        });
  }

  if (meshPtr != nullptr)
    meshInspectorUi(frame, *meshPtr);
}

void MaterialEditorLayer::meshInspectorUi(keptech::gui::Frame& frame,
                                          KEPTECH_RENDERER::Mesh& mesh) {

  frame.text("Vertices: %zu", mesh.getVertexCount());
  frame.text("Indices: %zu", mesh.getIndexCount());
  frame.text("Triangles: %zu",
             (mesh.getIndexCount() == 0 ? mesh.getVertexCount()
                                        : mesh.getIndexCount()) /
                 3);
  frame.text("Submeshes: %zu", mesh.getSubmeshes().size());
}

void MaterialEditorLayer::materialInspectorUi(
    keptech::gui::Frame& frame, keptech::components::Material& material) {

  frame.separatorText("Material");

  auto materialPtr = renderer.getMaterialData(material);

  const char* materialName =
      (materialPtr != nullptr) ? materialPtr->name.c_str() : "";

  {
    auto combo = frame.combo("Material", materialName);
    renderer.operateOnAllMaterials(
        [&](keptech::core::rendering::Material::Handle handle,
            KEPTECH_RENDERER::Material& m) {
          if (combo.item(m.name.c_str(), material == handle)) {
            material = handle;
          }
        });
  }

  if (materialPtr != nullptr)
    materialInspectorUi(frame, *materialPtr);
}

void MaterialEditorLayer::materialInspectorUi(
    keptech::gui::Frame& frame, KEPTECH_RENDERER::Material& material) {

  auto stageStr = fmt::format("Stage: {}", material.stage);
  frame.text(stageStr.c_str());

  using S = keptech::core::rendering::Material::Stage;
  if (material.mode == keptech::shaders::RenderingMode::Forward) {
    bool checked = (material.stage != S::Opaque);
    if (ImGui::Checkbox("Transparent", &checked)) {
      material.stage = checked ? S::Transparent : S::Opaque;
    }
  }
}
