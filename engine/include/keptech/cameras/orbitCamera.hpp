#pragma once

#include "keptech/cameraController.hpp"
#include "keptech/core/events/input.hpp"

namespace kt::cameras {
  class OrbitCameraController : public CameraController {
  public:
    OrbitCameraController() = default;
    OrbitCameraController(ecs::Entity entity, int panButton = 2) : CameraController(entity), panButton(panButton) {}

    float& getSensitivity() { return sens; }
    int& getPanButton() { return panButton; }

    bool handleEvent(Event& event, Timestep) override {
      if (!isValid())
        return false;

      EventDispatcher ed{event};

      if (ed.dispatch<MouseButtonPressEvent>([this](MouseButtonPressEvent& e) {
            if (e.button == panButton) {
              panning = true;
              return true;
            }
            return false;
          }) ||

          ed.dispatch<MouseButtonReleaseEvent>([this](MouseButtonReleaseEvent& e) {
            if (e.button == panButton) {
              panning = false;
              return true;
            }
            return false;
          }) ||

          ed.dispatch<MouseMovedEvent>([this](MouseMovedEvent& e) {
            if (!panning)
              return false;

            yaw += e.movement.x * sens;
            pitch += e.movement.y * sens;

            if (yaw < 360)
              yaw += 360;
            if (yaw > 360)
              yaw -= 360;
            pitch = std::clamp(pitch, -89.f, 89.f);

            return true;
          }) ||

          ed.dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e) {
            zoom -= e.offset.y;
            zoom = std::max(zoom, 1.f);
            return true;
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
  };
} // namespace kt::cameras
