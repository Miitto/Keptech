#include "imgui.h"
#include "keptech/cameras/orbitCamera.hpp"
#include "keptech/core/components/renderObject.hpp"
#include "keptech/core/components/transform.hpp"
#include "keptech/core/events/event.hpp"
#include <keptech/app.hpp>

#include "imgui_internal.h"
#include "keptech/ecs/entity.hpp"
#include <expected>
#include <keptech/components.hpp>
#include <keptech/core/gui.h>
#include <keptech/core/window.hpp>
#include <keptech/keptech.hpp>
#include <keptech/vulkan.hpp>
#include <utility>

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
                            keptech::gui::Frame* frame, Comp& comp);

class MaterialEditorLayer : public keptech::core::layers::Layer {
public:
  enum class ActiveDebugView : uint8_t { Albedo, Normals, Depth, Final };
  struct Resources {
    Meshes meshes;
    Materials materials;
  };

  MaterialEditorLayer(KEPTECH_RENDERER& renderer, keptech::core::Scene&& scene,
                      Resources&& resources);

  static void initMeta();

  void onUpdate(keptech::core::Timestep ts) override;

  void onEvent(keptech::core::events::Event& event,
               keptech::core::Timestep ts) override {
    if (orbitController.handleEvent(event, ts))
      return;
  }

  void meshInspectorUi(keptech::gui::Frame& frame,
                       keptech::components::Mesh& ro);

  void materialInspectorUi(keptech::gui::Frame& frame,
                           keptech::components::Material& ro);

private:
  void drawGui();
  void drawToolbar();
  void drawSceneTree();
  void drawEntityProperties();
  void drawAssetsPanel();

  KEPTECH_RENDERER& renderer;
  keptech::core::Scene scene;
  Resources resources;
  keptech::cameras::OrbitCameraController orbitController;
  keptech::ecs::EntityHandle selectedEntity =
      keptech::ecs::INVALID_ENTITY_HANDLE;

  ActiveDebugView activeDebugView = ActiveDebugView::Final;
};
