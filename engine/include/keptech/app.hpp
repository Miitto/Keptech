#pragma once

#include <concepts>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>
#include <keptech/core/cameras/cameraManager.hpp>
#include <keptech/core/kt-logger.hpp>
#include <keptech/core/renderer.hpp>
#include <keptech/core/window.hpp>
#include <keptech/ecs/ecs.hpp>

namespace keptech {
  template <typename Fn, typename R, typename Resources>
  concept SetupFn =
      (requires(Fn fn, core::window::Window& window, R& renderer) {
        {
          fn(window, renderer)
        } -> std::same_as<std::expected<Resources, std::string>>;
      } && core::renderer::CRenderer<R>);

  template <typename OnEvent>
  concept EventHandler = std::invocable<OnEvent, core::window::Window&,
                                        const core::window::Event&>;

  template <typename OnEvent, typename Resources>
  concept EventHandlerResources =
      std::invocable<OnEvent, core::window::Window&, const core::window::Event&,
                     Resources&>;

  template <core::renderer::CRenderer R, typename Resources = void, typename SF,
            typename EH>
    requires(SetupFn<SF, R, Resources> &&
             (EventHandler<EH> || EventHandlerResources<EH, Resources>))
  [[nodiscard]]
  bool run(const core::window::CreateInfo& windowCreateInfo,
           const core::renderer::CreateInfo& rendererCreateInfo,
           const SF& setupFn, const EH& eventHandler) {
    core::window::Window window(windowCreateInfo);

    auto& ecs = keptech::ecs::ECS::get();

    ecs.registerSystem<core::cameras::CameraManager>(
        core::cameras::CameraManager::getSignature());

    std::expected<R*, std::string> rendererRes =
        R::create(rendererCreateInfo, window);
    if (!rendererRes) {
      KT_CRITICAL("Failed to create renderer: {}", rendererRes.error());
      return false;
    }

    R& renderer = *rendererRes.value();

    KT_INFO("Created renderer: {}", R::getName());

    auto setupRes = setupFn(window, renderer);
    if (!setupRes) {
      KT_CRITICAL("Failed to run setup function: {}", setupRes.error());
      return false;
    }
    KT_INFO("Setup complete");

    auto& io = ImGui::GetIO();

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

    keptech::ecs::FrameData frameData{};

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

        constexpr bool hasResources = !std::is_same_v<Resources, void>;

        if constexpr (hasResources) {
          eventHandler(window, event, setupRes.value());
        } else {
          eventHandler(window, event);
        }
      }

      if (window.shouldClose()) {
        exitCleanly = true;
        break;
      }

      auto newTime = std::chrono::high_resolution_clock::now();

      float dt = std::chrono::duration<float, std::chrono::seconds::period>(
                     newTime - now)
                     .count();

      frameData.dt = dt;

      now = newTime;

      renderer.newFrame();

      ecs.preUpdateAllSystems(frameData);
      ecs.updateAllSystems(frameData);
      ecs.postUpdateAllSystems(frameData);

      renderer.render();
    }

    KT_INFO("Starting shutdown");

    ecs.destroy();

    return exitCleanly;
  }
} // namespace keptech
