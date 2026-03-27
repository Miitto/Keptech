
#pragma once

#include "keptech/cameraController.hpp"
#include "keptech/core/events/input.hpp"
#include "keptech/input.hpp"

namespace kt::cameras {
  class FreeCameraController : public CameraController {
  public:
    FreeCameraController() = default;
    FreeCameraController(ecs::Entity entity, u8 controlButton = 3)
        : CameraController(entity), controlButton(controlButton) {}

    float& getSensitivity() { return sens; }
    u8& getPanButton() { return controlButton; }

    bool handleEvent(core::events::Event& event, Timestep) override {
      if (!isValid())
        return false;

      using namespace kt::core::events;

      EventDispatcher ed{event};

      auto& camTransform =
          cameraEntity.getComponents<kt::components::Transform>();

      auto& input = Input::get();
      bool moving = input.isMouseButtonDown(controlButton);

      if (moving) {
        bool handled =
            ed.dispatch<MouseMovedEvent>([&, this](MouseMovedEvent& e) {
              if (!moving)
                return false;

              auto& rot = camTransform.getLocalMut().rot();

              auto euler = glm::degrees(glm::eulerAngles(rot));

              float rollFactor = 1.f;

              // Clamp roll
              if (euler.z > 90.f || euler.z < -90.f) {
                euler.z = 180.f;
                rollFactor = -1.f;
              } else {
                euler.z = 0.f;
              }

              euler.y += e.movement.x * sens * rollFactor;
              euler.x += e.movement.y * sens;

              if (rollFactor > 0.5f) {
                euler.x = std::clamp(euler.x, -89.f, 89.f);
              } else {
                if (euler.x < 91.f && euler.x > 0.f)
                  euler.x = 91.f;
                else if (euler.x > -91.f && euler.x < 0.f)
                  euler.x = -91.f;
              }

              rot = glm::quat(glm::radians(euler));

              return true;
            });

        return handled;
      }

      return false;
    }

    void update(Timestep dt) override {
      auto& input = Input::get();

      auto& camTransform =
          cameraEntity.getComponents<kt::components::Transform>();

      float speed = .01f;
      if (input.isKeyDown(SDLK_LSHIFT)) {
        speed *= 5.f;
      } else if (input.isKeyDown(SDLK_LCTRL)) {
        speed *= 0.5f;
      }
      float mul = speed * dt;

      glm::mat3 rotMat = glm::mat3(camTransform.getGlobal());

      glm::vec3 forward = glm::normalize(rotMat * FORWARD);
      glm::vec3 right = glm::normalize(rotMat * RIGHT);

      glm::vec3 movement{0.f};

      if (input.isKeyDown(SDLK_W)) {
        movement += forward;
      }
      if (input.isKeyDown(SDLK_S)) {
        movement += -forward;
      }
      if (input.isKeyDown(SDLK_A)) {
        movement += -right;
      }
      if (input.isKeyDown(SDLK_D)) {
        movement += right;
      }
      if (input.isKeyDown(SDLK_Q)) {
        movement += -UP;
      }
      if (input.isKeyDown(SDLK_E)) {
        movement += UP;
      }

      movement *= mul;

      camTransform.getLocalMut().translate(movement);
    }

    [[nodiscard]] bool moving() const {
      return Input::get().isMouseButtonDown(controlButton);
    }

  private:
    u8 controlButton = 3;
    float sens = 1.f;
  };
} // namespace kt::cameras
