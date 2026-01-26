#include "editorLayer.hpp"

#include "imgui.h"
#include "keptech/core/components/transform.hpp"
#include "keptech/core/slotmap.hpp"
#include "keptech/ecs/entity.hpp"
#include <filesystem>
#include <imgui/misc/cpp/imgui_stdlib.h>

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
void forwardCompInspectorUi<keptech::components::Camera>(
    MaterialEditorLayer* layer, keptech::gui::Frame* frame,
    keptech::ecs::EntityHandle entity) {

  auto& comp =
      layer->getScene().getEcs().get<keptech::components::Camera>(entity);
  layer->cameraInspectorUi(*frame, comp);
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
  metaFunc<keptech::components::Camera>();
}

namespace {
  using Directory = MaterialEditorLayer::Directory;
  using File = MaterialEditorLayer::File;
  MaterialEditorLayer::Directory
  parseDirectory(const std::filesystem::path& path) {
    Directory dir;

    dir.name = path.filename().string();

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
      if (entry.is_directory()) {
        dir.directories.push_back(parseDirectory(entry.path()));
      } else if (entry.is_regular_file()) {
        File file;
        file.name = entry.path().filename().string();
        auto ext = entry.path().extension().string();
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
          file.type = MaterialEditorLayer::FileType::Image;
        } else if (ext == ".glb" || ext == ".gltf") {
          file.type = MaterialEditorLayer::FileType::Mesh;
        } else {
          file.type = MaterialEditorLayer::FileType::Unknown;
        }
        dir.files.push_back(file);
      }
    }

    return dir;
  }
} // namespace

MaterialEditorLayer::MaterialEditorLayer(
    keptech::Renderer& renderer, keptech::Scene&& scene,
    std::vector<keptech::MeshPtr>&& meshes,
    std::vector<keptech::PipelinePtr>&& pipelines)
    : keptech::core::layers::Layer("MaterialEditorLayer"), renderer(renderer),
      scene(std::move(scene)), orbitController(this->scene.getActiveCamera()),
      loadedMeshes(std::move(meshes)), loadedPipelines(std::move(pipelines)) {
  renderer.setScene(&this->scene);

  gBufferImGuiHandles.albedo =
      renderer.getImGuiTextureHandle(renderer.getGBuffers().albedo);
  gBufferImGuiHandles.normal =
      renderer.getImGuiTextureHandle(renderer.getGBuffers().normal);
  gBufferImGuiHandles.depth =
      renderer.getImGuiTextureHandle(renderer.getGBuffers().depth);

  refreshAssetsDirectory();
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

  ImVec2 size = ImGui::GetContentRegionAvail();

  switch (activeDebugView) {
  case ActiveDebugView::Final:
  case ActiveDebugView::Albedo:
    ImGui::Image(gBufferImGuiHandles.albedo, size);
    break;
  case ActiveDebugView::Normals:
    ImGui::Image(gBufferImGuiHandles.normal, size);
    break;
  case ActiveDebugView::Depth:
    ImGui::Image(gBufferImGuiHandles.depth, size);
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

  std::visit(keptech::core::overloaded{
                 [&](std::monostate) {},
                 [&](keptech::ecs::EntityHandle entity) {
                   drawEntityProperties(propertiesPanel, entity);
                 },
                 [&](keptech::MeshPtr meshPtr) {
                   if (meshPtr == nullptr) {
                     propertiesPanel.separatorText("Invalid Mesh");
                     return;
                   }
                   auto label =
                       fmt::format("Mesh: {}", meshPtr->getDebugName());
                   propertiesPanel.separatorText(label.c_str());
                   meshInspectorUi(propertiesPanel, meshPtr);
                 },
                 [&](keptech::PipelinePtr pipelinePtr) {
                   if (pipelinePtr == nullptr) {
                     propertiesPanel.separatorText("Invalid Material");
                     return;
                   }

                   auto label =
                       fmt::format("Pipeline: {}", pipelinePtr->getDebugName());
                   propertiesPanel.separatorText(label.c_str());
                   pipelineInspectorUi(propertiesPanel, *pipelinePtr);
                 },
                 [&](keptech::TexPtr texturePtr) {
                   if (texturePtr == nullptr) {
                     propertiesPanel.separatorText("Invalid Texture");
                     return;
                   }

                   auto label =
                       fmt::format("Texture: {}", texturePtr->getDebugName());
                   propertiesPanel.separatorText(label.c_str());
                   std::string formatStr =
                       fmt::format("Format: {}", texturePtr->getFormat());
                   propertiesPanel.text(formatStr.c_str());
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

#ifndef NDEBUG
namespace {
  void printDirectory(const MaterialEditorLayer::Directory& dir, size_t depth) {
    std::string indent(depth * 2, ' ');
    SPDLOG_INFO("{}{}", indent, dir.name);
    for (auto& d : dir.directories)
      printDirectory(d, depth + 1);

    std::string fileIndent((depth + 1) * 2, ' ');
    for (const auto& file : dir.files) {
      SPDLOG_INFO("{}{}", fileIndent, file.name);
    }
  }
} // namespace
#endif

void MaterialEditorLayer::refreshAssetsDirectory() {
  assetsRootDir = parseDirectory(ASSET_DIR);
  assetsRootDir.name = "Assets";

#ifndef NDEBUG
  printDirectory(assetsRootDir, 0);
#endif
}

void MaterialEditorLayer::drawAssetsDirectory(keptech::gui::Frame& frame,
                                              const Directory& directory,
                                              float depth) {
  if (directory.name.empty())
    return;
  ImGui::PushID(directory.name.c_str());

  ImGui::Unindent();
  ImGui::Indent(depth * 7.f);
  if (ImGui::TreeNodeEx(directory.name.c_str(),
                        ImGuiTreeNodeFlags_SpanFullWidth |
                            ImGuiTreeNodeFlags_FramePadding |
                            ImGuiTreeNodeFlags_DrawLinesFull)) {

    for (const auto& dir : directory.directories) {
      drawAssetsDirectory(frame, dir, depth + 1);
    }

    ImGui::Unindent();
    ImGui::Indent(depth * 7.f);
    for (const auto& file : directory.files) {
      if (file.name.empty())
        continue;
      ImGui::PushID(file.name.c_str());
      ImGui::TreeNodeEx(file.name.c_str(),
                        ImGuiTreeNodeFlags_Leaf |
                            ImGuiTreeNodeFlags_NoTreePushOnOpen |
                            ImGuiTreeNodeFlags_SpanFullWidth);
      ImGui::PopID();
    }

    ImGui::TreePop();
  }
  ImGui::PopID();
}

void MaterialEditorLayer::drawAssetsPanel() {
  auto assetsPanel = keptech::gui::Frame("Assets", nullptr,
                                         ImGuiWindowFlags_NoDecoration |
                                             ImGuiWindowFlags_NoMove);
  if (!assetsPanel.isOpen())
    return;

  if (ImGui::BeginPopupContextWindow()) {
    if (ImGui::Button("Refresh")) {
      refreshAssetsDirectory();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  drawAssetsDirectory(assetsPanel, assetsRootDir, 0);
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
        for (auto& meshPtr : loadedMeshes) {
          ImGui::TableNextColumn();

          bool selected = false;
          if (selectedItem.index() == 2) {
            selected = std::get<keptech::MeshPtr>(selectedItem) == meshPtr;
          }

          if (child.selectable(meshPtr->getDebugName().c_str(), selected)) {
            selectedItem = meshPtr;
          }
        }
      } break;
      case AssetType::Material: {
        for (auto& pipelinePtr : loadedPipelines) {
          ImGui::TableNextColumn();

          bool selected = false;
          if (selectedItem.index() == 3) {
            selected =
                std::get<keptech::PipelinePtr>(selectedItem) == pipelinePtr;
          }

          if (child.selectable(pipelinePtr->getDebugName().c_str(), selected)) {
            selectedItem = pipelinePtr;
          }
        }
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

  const char* meshName = (mesh != nullptr) ? mesh->getDebugName().c_str() : "";

  {
    auto combo = frame.combo("Mesh", meshName);
    for (auto& m : loadedMeshes) {
      if (combo.item(m->getDebugName().c_str(), mesh == m)) {
        mesh = m;
      }
    }
  }

  frame.text("Vertices: %zu", mesh->getVertexCount());
  frame.text("Indices: %zu", mesh->getIndexCount());
  frame.text("Triangles: %zu",
             (mesh->getIndexCount() == 0 ? mesh->getVertexCount()
                                         : mesh->getIndexCount()) /
                 3);
  frame.text("Submeshes: %zu", mesh->getSubmeshes().size());
  frame.text("Vertex Offset: %zu", mesh->getVertexOffset());
}

void MaterialEditorLayer::materialInspectorUi(
    keptech::gui::Frame& frame, keptech::components::Material& material) {

  frame.separatorText("Material");

  if (ImGui::CollapsingHeader("Pipeline##header")) {

    const char* pipelineName = (material.pipeline != nullptr)
                                   ? material.pipeline->getDebugName().c_str()
                                   : "";

    {
      auto combo = frame.combo("Pipeline", pipelineName);
      for (auto& p : loadedPipelines) {
        if (combo.item(p->getDebugName().c_str(), material.pipeline == p)) {
          material.pipeline = p;
        }
      }
    }

    if (material.pipeline != nullptr)
      pipelineInspectorUi(frame, *material.pipeline);
  }

  for (auto& data : material.instanceData) {
    switch (data.index()) {
    case static_cast<size_t>(keptech::shaders::DataType::Uint): {
      auto texturePtr = std::get<keptech::TexPtr>(data);

      {
        auto combo =
            frame.combo("Texture", (texturePtr != nullptr)
                                       ? texturePtr->getDebugName().c_str()
                                       : "Invalid");
      }

    } break;
    default:
      break;
    }
  }
}

void MaterialEditorLayer::pipelineInspectorUi(keptech::gui::Frame& frame,
                                              keptech::IPipeline& pipeline) {

  auto stageStr = fmt::format("Stage: {}", pipeline.getStage());
  frame.text(stageStr.c_str());

  using S = keptech::PipelineStage;
  if (pipeline.getRenderingMode() == keptech::shaders::RenderingMode::Forward) {
    bool checked = (pipeline.getStage() != S::Opaque);
    if (ImGui::Checkbox("Transparent", &checked)) {
      pipeline.setStage(checked ? S::Transparent : S::Opaque);
    }
  }
}

void MaterialEditorLayer::cameraInspectorUi(keptech::gui::Frame& frame,
                                            keptech::components::Camera& cam) {
  frame.separatorText("Camera");

  auto& params = cam.getParams();

  if (cam.isPerspective()) {
    float fovY = params.perspective.fovY;

    if (frame.inputFloat("FovY", fovY)) {
    }
  }
}
