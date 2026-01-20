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

struct Materials {
  keptech::MaterialHandle basic;
  keptech::MaterialHandle deferred;
};

struct Meshes {
  keptech::MeshHandle triangle;
  keptech::MeshHandle monkey;
};

class MaterialEditorLayer;

template <typename Comp>
void forwardCompInspectorUi(MaterialEditorLayer* layer,
                            keptech::gui::Frame* frame,
                            keptech::ecs::EntityHandle entity);

class MaterialEditorLayer : public keptech::core::layers::Layer {
public:
  struct RawMeshHandle {
    keptech::core::SlotMapHandle handle;

    RawMeshHandle(keptech::core::SlotMapHandle h) : handle(h) {}
    bool operator==(const keptech::core::SlotMapHandle& other) const {
      return handle == other;
    }
    operator keptech::core::SlotMapHandle() const { return handle; }
  };
  struct RawMaterialHandle {
    keptech::core::SlotMapHandle handle;
    RawMaterialHandle(keptech::core::SlotMapHandle h) : handle(h) {}
    bool operator==(const keptech::core::SlotMapHandle& other) const {
      return handle == other;
    }
    operator keptech::core::SlotMapHandle() const { return handle; }
  };

  using SelectedItem = std::variant<std::monostate, keptech::ecs::EntityHandle,
                                    RawMeshHandle, RawMaterialHandle>;

  enum class ActiveDebugView : uint8_t { Albedo, Normals, Depth, Final };
  struct Resources {
    Meshes meshes;
    Materials materials;
  };

  MaterialEditorLayer(KEPTECH_RENDERER& renderer, keptech::core::Scene&& scene,
                      Resources&& resources);

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

private:
  void initDocks();
  void drawGui();
  void drawViewport();
  void drawToolbar();
  void drawSceneTree();
  void drawSelectedProperties();
  void drawEntityProperties(keptech::gui::Frame& frame,
                            keptech::ecs::EntityHandle entity);
  void drawAssetsPanel();
  void drawLoadedAssetsPanel();

  KEPTECH_RENDERER& renderer;
  keptech::core::Scene scene;
  Resources resources;
  keptech::cameras::OrbitCameraController orbitController;

  SelectedItem selectedItem = std::monostate{};

  ActiveDebugView activeDebugView = ActiveDebugView::Final;
};
