#pragma once

#include <keptech/core/base.hpp>
#include <keptech/core/components/camera.hpp>
#include <keptech/core/components/transform.hpp>
#include <keptech/core/events/event.hpp>
#include <keptech/core/scene.hpp>
#include <keptech/ecs/entity.hpp>

namespace keptech {
  class CameraController {
  public:
    CameraController() = default;
    CameraController(const CameraController&) = default;
    CameraController(CameraController&&) = delete;
    CameraController& operator=(const CameraController&) = default;
    CameraController& operator=(CameraController&&) = delete;
    virtual ~CameraController() = default;

    CameraController(ecs::Entity entity) { attachTo(entity); }

    [[nodiscard]] bool isValid() const {
      if (!cameraEntity.isValid())
        return false;

      return cameraEntity
          .hasAllComponents<components::Transform, components::Camera>();
    }

    bool attachTo(ecs::Entity entity) {
      if (!entity.isValid())
        return false;
      if (!entity.hasAllComponents<components::Transform, components::Camera>())
        return false;
      this->cameraEntity = entity;
      return true;
    }

    virtual bool handleEvent(core::events::Event& event, Timestep ts) = 0;
    virtual void update(Timestep) {}

  protected:
    ecs::Entity cameraEntity{};
  };
} // namespace keptech
