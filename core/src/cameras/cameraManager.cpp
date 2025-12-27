#include "keptech/core/cameras/cameraManager.hpp"

#include "keptech/core/components/camera.hpp"

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
    auto& cameraComp = ecs.getComponentRef<components::Camera>(activeCamera);
    return &*cameraComp;
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
