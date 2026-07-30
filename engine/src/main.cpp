#include "keptech/app.hpp"

#include "keptech/components/transform.hpp"
#include "keptech/core/gui.h"
#include "keptech/input.hpp"
#include "keptech/render/renderGraph/builder.hpp"
#include "keptech/render/renderGraph/graph.hpp"
#include "keptech/render/renderer.hpp"
#include <expected>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>
#include <keptech/core/kt-logger.hpp>
#include <keptech/core/profile.hpp>
#include <string>

using namespace kt;

int main() {
  if (!Window::init()) {
    KT_CRITICAL("Failed to initialize windowing system");
    return -1;
  }
  auto info = configureApp();

  Window window(info.window);

  bool exitCleanly = false;
  {
    KT_DEBUG("Creating renderer");
    std::expected<void, std::string> rendererRes = rdr::Renderer::init(info.renderer, window);
    if (!rendererRes) {
      KT_CRITICAL("Failed to create renderer: {}", rendererRes.error());
      return -1;
    }
    KT_DEBUG("Renderer created successfully");

    rdr::RenderGraphBuilder rgBuilder{};

    rdr::Renderer& renderer = rdr::Renderer::get();

    renderer.setRenderGraphProps(rgBuilder);

    LayerStack layerStack{};

    auto setupRes = setupAppLayers(layerStack, window, rgBuilder, renderer);
    if (!setupRes) {
      KT_CRITICAL("Failed to set up application layers: {}", setupRes.error());
      return -1;
    }

    auto& ecs = Scene::active().getEcs();

    ecs.sort<kt::components::Transform>([](const auto& a, const auto& b) { return a.getDepth() < b.getDepth(); });
    // Sorts meshes to minimize cache misses when iterating with transforms
    ecs.sort<kt::components::Mesh, kt::components::Transform>();

    rgBuilder.bake(renderer);
#ifndef NDEBUG
    rgBuilder.log();
#endif
    auto rg = rgBuilder.build(renderer);

#ifndef NDEBUG
    rg.log();
#endif

    auto& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | // Enable Keyboard Controls
                      ImGuiConfigFlags_DockingEnable;      // Enable Docking

    // Any UP event is not included, as it's preferable to handle them
    // regardless of if ImGui wants input.
    auto isKeyboardEvent = [](WindowEvent event) {
      switch (event.type) {
      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_TEXT_INPUT:
        return true;
      default:
        return false;
      }
    };

    auto isMouseEvent = [](WindowEvent event) {
      switch (event.type) {
      case SDL_EVENT_MOUSE_MOTION:
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_WHEEL:
      case SDL_EVENT_FINGER_DOWN:
      case SDL_EVENT_FINGER_MOTION:
        return true;
      default:
        return false;
      }
    };

    auto& input = Input::get();
    auto inputProcessEvent = [&](SDL_Event& event) {
      switch (event.type) {
      case SDL_EVENT_KEY_DOWN: {
        if (event.key.repeat)
          break;
        uint32_t key = event.key.key;
        input.registerKeyPress(key);
      } break;
      case SDL_EVENT_KEY_UP: {
        if (event.key.repeat)
          break;
        uint32_t key = event.key.key;
        input.registerKeyRelease(key);
      } break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        uint32_t button = event.button.button;
        input.registerMouseButtonPress(button);
      } break;
      case SDL_EVENT_MOUSE_BUTTON_UP: {
        uint32_t button = event.button.button;
        input.registerMouseButtonRelease(button);
      } break;
      default: {
      } break;
      }
    };

    auto now = std::chrono::high_resolution_clock::now();

    kt::WindowEvent event;
    while (true) {
      KT_PROFILE_SCOPE("Main Loop");
      KT_TRACE("Starting frame");
      auto newTime = std::chrono::high_resolution_clock::now();

      float dt = std::chrono::duration<float, std::chrono::milliseconds::period>(newTime - now).count();

      now = newTime;
      while (window.pollEvent(event)) {
        KT_PROFILE_SCOPE("Event Processing");
        ImGui_ImplSDL3_ProcessEvent(&event);
        if ((io.WantCaptureKeyboard && isKeyboardEvent(event)) || (io.WantCaptureMouse && isMouseEvent(event))) {
          KT_TRACE("Event sent to ImGui");
          continue;
        }
        inputProcessEvent(event);

        if (event.type == SDL_EVENT_WINDOW_RESIZED) {
          KT_DEBUG("Window resized to {}x{}", event.window.data1, event.window.data2);
          auto newSize = glm::uvec2{static_cast<uint32_t>(event.window.data1), static_cast<uint32_t>(event.window.data2)};
          rg.onSwapchainSizeChanged(newSize);
        }

        auto eventPtr = Event::fromSdl(event);
        if (eventPtr.get() == nullptr) {
          continue;
        }
        KT_TRACE("Polled event: {}", eventPtr->getType());
        layerStack.onEvent(*eventPtr, dt);
      }

      if (window.shouldClose()) {
        exitCleanly = true;
        break;
      }

      renderer.newFrame();

      layerStack.onUpdate(dt);

      kt::gui::Frame::processInputPassthrough();

      rg.execute();

      input.endFrame();

      KT_TRACE("Frame complete");
    }

    KT_INFO("Starting shutdown");
    rg.destroy();
  }
  Window::shutdown();

  return exitCleanly;
}
