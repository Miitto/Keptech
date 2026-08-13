#pragma once

#include "keptech/cameraController.hpp"
#include "keptech/core/events/input.hpp"
#include "keptech/core/events/window.hpp"

namespace kt::cameras {
  class OrbitCameraController : public CameraController {
  public:
    OrbitCameraController() = default;
    OrbitCameraController(ecs::Entity entity, int panButton = 2, bool sizeToWindow = false)
        : CameraController(entity), panButton(panButton), sizeToWindow(sizeToWindow) {}

    float& getSensitivity() { return sens; }
    int& getPanButton() { return panButton; }

    /// Handle an event and update the camera's transform accordingly. Returns true if the event was handled, false otherwise.
    bool handleEvent(Event& event, Timestep) override {
      if (!isValid())
        return false;

      EventDispatcher ed{event};

      if (sizeToWindow)
        if (ed.dispatch<WindowResizeEvent>([&, this](WindowResizeEvent& e) {
              onViewportResize(e.size);
              return Propagation::Bubble;
            }))
          return false; // Even though we did work, window resizing is kinda important so keep it going.

      if (ed.dispatch<MouseButtonPressEvent>([this](MouseButtonPressEvent& e) {
            if (e.button == panButton) {
              panning = true;
              return Propagation::None;
            }
            return Propagation::Bubble;
          }) ||

          ed.dispatch<MouseButtonReleaseEvent>([this](MouseButtonReleaseEvent& e) {
            if (e.button == panButton) {
              panning = false;
              return Propagation::None;
            }
            return Propagation::Bubble;
          }) ||

          ed.dispatch<MouseMovedEvent>([this](MouseMovedEvent& e) {
            if (!panning)
              return Propagation::Bubble;

            yaw += e.movement.x * sens;
            pitch += e.movement.y * sens;

            if (yaw < 360)
              yaw += 360;
            if (yaw > 360)
              yaw -= 360;
            pitch = std::clamp(pitch, -89.f, 89.f);

            return Propagation::None;
          }) ||

          ed.dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e) {
            zoom -= e.offset.y;
            zoom = std::max(zoom, 1.f);
            return Propagation::None;
          })) {
        auto& camTransform = cameraEntity.getComponents<kt::components::Transform>();

        auto& transform = camTransform.getLocalMut();

        glm::quat rot{glm::vec3(glm::radians(pitch), glm::radians(yaw), 0.f)};
        glm::vec3 dir = rot * kt::FORWARD;

        glm::vec3 pos = -dir * zoom;

        transform.setPosition(pos);
        transform.lookAt(kt::ORIGIN, kt::UP);
        return true;
      }
      return false;
    }

  private:
    int panButton = 2;
    bool panning = false;
    float sens = 1.f;
    float zoom = 5;
    float yaw = 180; // Start looking down -Z, so camera is +Z from target
    float pitch = 0;
    bool sizeToWindow = false;
  };
} // namespace kt::cameras
