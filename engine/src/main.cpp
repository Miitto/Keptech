#include "keptech/app.hpp"

#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>
#include <keptech/core/kt-logger.hpp>

using namespace keptech;

int main() {
  core::window::init();
  auto info = configureApp();

  core::window::Window window(info.window);

  std::expected<KEPTECH_RENDERER, std::string> rendererRes =
      KEPTECH_RENDERER::create(info.renderer, window);
  if (!rendererRes) {
    KT_CRITICAL("Failed to create renderer: {}", rendererRes.error());
    return -1;
  }

  KEPTECH_RENDERER& renderer = rendererRes.value();

  KT_INFO("Created renderer: {}", KEPTECH_RENDERER::getName());

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
                                           //
  auto isKeyboardEvent = [](core::window::Event event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
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
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
      return true;
    default:
      return false;
    }
  };

  auto now = std::chrono::high_resolution_clock::now();

  bool exitCleanly = false;

  keptech::core::window::Event event;
  while (true) {
    while (window.pollEvent(event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if ((io.WantCaptureKeyboard && isKeyboardEvent(event)) ||
          (io.WantCaptureMouse && isMouseEvent(event))) {
        continue;
      }
      auto eventPtr = keptech::core::events::sdlEventToKeptechEvent(event);
      if (!eventPtr) {
        continue;
      }
      layerStack.onEvent(*eventPtr);
    }

    if (window.shouldClose()) {
      exitCleanly = true;
      break;
    }

    auto newTime = std::chrono::high_resolution_clock::now();

    float dt = std::chrono::duration<float, std::chrono::seconds::period>(
                   newTime - now)
                   .count();

    now = newTime;

    renderer.newFrame();

    layerStack.onUpdate(dt);

    renderer.render();
  }

  KT_INFO("Starting shutdown");
  core::window::shutdown();

  return exitCleanly;
}
