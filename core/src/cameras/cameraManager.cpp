#include "keptech/core/cameras/cameraManager.hpp"

namespace keptech::core::cameras {
  CameraManager* CameraManager::singleton{};

  void CameraManager::onUpdate(const ecs::FrameData& frameData) {
    (void)frameData;
  }

  Camera* CameraManager::active() const {
    if (activeCamera == ecs::INVALID_ENTITY_HANDLE) {
      return nullptr;
    }

    auto& ecs = ecs::ECS::get();
    auto camera = ecs.getComponent<Camera>(activeCamera);
    return camera;
  }

  CameraManager& CameraManager::setActive(ecs::EntityHandle cameraEntity) {
    activeCamera = cameraEntity;
    return *this;
  }

  ecs::Entity* CameraManager::activeEntity() const {
    auto& ecs = ecs::ECS::get();
    return ecs.getEntity(activeCamera);
  }
} // namespace keptech::core::cameras
