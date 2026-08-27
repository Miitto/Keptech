#pragma once

#include "keptech/cameras/orbitCamera.hpp"
#include "keptech/components/camera.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/core/events/event.hpp"
#include "keptech/core/events/window.hpp"
#include "keptech/core/kt-logger.hpp"
#include "keptech/core/layers/layer.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/core/window.hpp"
#include "keptech/gltf/data.hpp"
#include "keptech/gltf/scene.hpp"
#include "keptech/graph/builder.hpp"
#include "keptech/graph/graph.hpp"
#include "keptech/passes/debug.hpp"
#include "keptech/passes/geometry.hpp"
#include "keptech/passes/pointLights.hpp"
#include "keptech/rhi/rhi.hpp"

class ExampleLayer : public kt::Layer {
public:
  ExampleLayer(kt::Window& window, kt::RenderGraphBuilder& builder, kt::rhi::RHI& rhi) : kt::Layer("Example Mesh Shader") {
    // Get a quick reference to the active scene.
    auto& scene = kt::Scene::active();

    // Load the monkey mesh onto the GPU.
    auto monkeyDataRes = kt::gltf::Data::fromFile(ASSET_DIR "meshes/monkey.glb");
    if (!monkeyDataRes) {
      KT_ABORT("Failed to load monkey mesh: {}", monkeyDataRes.error());
    }
    auto& monkeyData = monkeyDataRes.value();

    auto monkeyUploadRes = monkeyData.upload();
    if (!monkeyUploadRes) {
      KT_ABORT("Failed to upload monkey mesh: {}", monkeyUploadRes.error());
    }
    auto& monkeyUpload = monkeyUploadRes.value();
    KT_DEBUG("Uploading monkey mesh to GPU with fence value {}", monkeyUpload.copyFenceValue);
    rhi.waitCopyIdle();
    KT_DEBUG("Upload done");

    // Create an entity for the monkey mesh and add it to the ECS scene.
    auto monkey = scene.createEntity("Monkey");

    // The loaded monkey mesh is technically the entire glTF scene, which may contain multiple nodes. We can add the entire scene to the ECS
    // like this, using a given root entity.
    // In this case, we simply end up with "Monkey -> Suzanne" in the ECS, where "Monkey" is the entity we created above, and "Suzanne" came
    // from the glTF scene. "Suzanne" was created with a `Transform` and `Mesh` component since that is what the corresponding glTF node
    // had.
    monkeyUpload.scene.addToEcsScene(scene, monkey.getHandle());

    // Create an entity for a camera.
    auto camera = scene.createEntity("Camera");
    // Add a transform to the camera.
    camera.addComponent<kt::components::Transform>()
        .getLocalMut()
        .translate(glm::vec3(12.0f, 4.0f, 4.0f))
        .lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Add the camera component to the camera entity. This is a perspective camera with a 90 degree field of view.
    // All camera types are contained within the same `Camera` component.
    auto& camComp =
        camera.addComponent<kt::components::Camera>(kt::components::PerspectiveType::Standard, kt::components::Camera::Params::Common{},
                                                    kt::components::Camera::Params::Perspective{
                                                        .fovY = glm::radians(90.f),
                                                    });
    // Size the camera to the window size so that the aspect ratio is correct.
    // The camera uniforms also contain this viewport size for convinience.
    camComp.sizeToWindowSize(window.getRenderSize());

    // Make this camera the active camera. The active camera is the one that the renderer will use for its inbuilt render passes, and will
    // always write out the active camera's uniforms.
    scene.useCamera(camera);

    // There are two inbuilt camera controllers in Keptech: `OrbitCameraController` and `FreeCameraController`. They take user input (as
    // seen in the `onEvent` function below) and update the camera's transform accordingly. Here we use an orbit camera controller, which
    // orbits around a target point.
    orbitController = kt::cameras::OrbitCameraController(camera, 3, true);

    dataPass.addToGraph(builder);
    geomPass.addToGraph(builder);
    pointLightPass.addToGraph(builder);

    // Here we set the backbuffer source for the render graph. This determines which render pass output will be used as the final image to
    // present to the screen. The geometry pass outputs to multiple G-buffers, and we can choose which one to use as the final output.
    //
    // We also read the environment variable `KT_SURFACE` to allow the user to override this default. Try setting `KT_SURFACE=kt::normal`
    // before running this example to see the normal buffer instead.
    const char* backBufferSourceEnv = std::getenv("KT_SURFACE");

    builder.setBackbufferSource(backBufferSourceEnv ? backBufferSourceEnv : "kt::albedo");

    /// Set the render resolution to the swapchain size. Less efficient but means we can directly copy the backbuffer source to the
    /// swapchain without adding a resize pass. In a more complete example, there would be more than one pass, and the last pass would
    /// output to a swapchain relative image. In normal usage, having a seperate variable for the render resolution means that every render
    /// target does not need to be recreated when the window is resized.
    builder.setRenderResolution(rhi.getSwapchainSize());

    // Debug pass should always be added last so it can read from all the other passes. It uses the backbuffer source at the time it was
    // added to the graph as the dependency tree root.
    debugPass.addToGraph(builder);
  }

  // The `onUpdate` function is called every frame, and is where we update the camera controller. The camera controller will update the
  // camera's transform based on user input, which is handled in the `onEvent` function below.
  void onUpdate(kt::Timestep ts) final { orbitController.update(ts); }

  // The `onEvent` function is called every time an event occurs, such as a key press or mouse movement. We pass the event to the camera
  // controller, which will handle it if it is relevant to the camera. If the camera controller handles the event, we return early to
  // prevent further processing of the event (although this is not strictly necessary in this case, as we don't do anything else
  // regardless).
  void onEvent(kt::Event& event, kt::Timestep ts) final {
    if (orbitController.handleEvent(event, ts))
      return;

    kt::EventDispatcher dispatcher(event);
    dispatcher.dispatch<kt::WindowResizeEvent>([&](kt::WindowResizeEvent& e) {
      kt::RenderGraph::getActiveGraph().onResolutionChanged(e.size);

      return kt::Propagation::Bubble;
    });
  }

private:
  kt::cameras::OrbitCameraController orbitController{};
  kt::DataPass dataPass{};
  kt::GeometryPass geomPass{};
  kt::PointLightPass pointLightPass{};
  kt::DebugPass debugPass{};
  kt::RenderGraph* graph;
};