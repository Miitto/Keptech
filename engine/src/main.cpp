#include "keptech/app.hpp"

#include "keptech/core/gui.h"
#include "keptech/input.hpp"
#include <expected>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>
#include <keptech/core/kt-logger.hpp>
#include <keptech/core/profile.hpp>
#include <string>

#include "keptech/renderer.hpp"

using namespace kt;

int main() {
  if (!core::window::init())
    return -1;
  auto info = configureApp();

  core::window::Window window(info.window);

  bool exitCleanly = false;
  {
    KT_DEBUG("Creating renderer");
    std::expected<rendering::Renderer, std::string> rendererRes = rendering::Renderer::create(info.renderer, window);
    if (!rendererRes) {
      KT_CRITICAL("Failed to create renderer: {}", rendererRes.error());
      return -1;
    }
    KT_DEBUG("Renderer created successfully");

    rendering::RenderGraphBuilder rgBuilder{};

    rendering::Renderer renderer = std::move(rendererRes.value());

    renderer.setRenderGraphProps(rgBuilder);

    core::layers::LayerStack layerStack;

    auto setupRes = setupAppLayers(layerStack, window, rgBuilder, renderer);
    if (!setupRes) {
      KT_CRITICAL("Failed to set up application layers: {}", setupRes.error());
      return -1;
    }

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
    auto isKeyboardEvent = [](core::window::Event event) {
      switch (event.type) {
      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_TEXT_INPUT:
        return true;
      default:
        return false;
      }
    };

    auto isMouseEvent = [](core::window::Event event) {
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

    kt::core::window::Event event;
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

        auto eventPtr = kt::core::events::sdlEventToKeptechEvent(event);
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
  core::window::shutdown();

  return exitCleanly;
}
