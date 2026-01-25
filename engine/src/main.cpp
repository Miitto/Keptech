#include "keptech/app.hpp"
#include "keptech/core/gui.h"

#include <expected>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>
#include <keptech/core/kt-logger.hpp>
#include <string>

using namespace keptech;

int main() {
  if (!core::window::init())
    return -1;
  auto info = configureApp();

  core::window::Window window(info.window);

  bool exitCleanly = false;
  {
    std::expected<Renderer, std::string> rendererRes =
        Renderer::create(info.renderer, window);
    if (!rendererRes) {
      KT_CRITICAL("Failed to create renderer: {}", rendererRes.error());
      return -1;
    }

    Renderer renderer = std::move(rendererRes.value());

    core::layers::LayerStack layerStack;

    auto setupRes = setupAppLayers(layerStack, window, renderer);
    if (!setupRes) {
      KT_CRITICAL("Failed to set up application layers: {}", setupRes.error());
      return -1;
    }

    auto& io = ImGui::GetIO();

    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard | // Enable Keyboard Controls
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

    auto now = std::chrono::high_resolution_clock::now();

    keptech::core::window::Event event;
    while (true) {
      KT_TRACE("Starting frame");
      auto newTime = std::chrono::high_resolution_clock::now();

      float dt =
          std::chrono::duration<float, std::chrono::milliseconds::period>(
              newTime - now)
              .count();

      now = newTime;
      while (window.pollEvent(event)) {
        auto eventPtr = keptech::core::events::sdlEventToKeptechEvent(event);
        ImGui_ImplSDL3_ProcessEvent(&event);
        if ((io.WantCaptureKeyboard && isKeyboardEvent(event)) ||
            (io.WantCaptureMouse && isMouseEvent(event))) {
          KT_TRACE("Event sent to ImGui");
          continue;
        }
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

      keptech::gui::Frame::processInputPassthrough();

      renderer.render();

      KT_TRACE("Frame complete");
    }

    KT_INFO("Starting shutdown");
  }
  core::window::shutdown();

  return exitCleanly;
}
