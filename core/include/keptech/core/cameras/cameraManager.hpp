#pragma once

#include "camera.hpp"
#include "keptech/ecs/base.hpp"
#include <keptech/ecs/ecs.hpp>

namespace keptech::core::cameras {
  class CameraManager : public ecs::System {

  public:
    void onUpdate(const ecs::FrameData& frameData) override;

    explicit CameraManager() {
      assert(singleton == nullptr &&
             "Created more than once CameraManager instance");
      singleton = this;
    }
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;
    CameraManager(CameraManager&&) noexcept = delete;
    CameraManager& operator=(CameraManager&&) noexcept = delete;
    ~CameraManager() override { singleton = nullptr; }

    CameraManager& get() { return *singleton; }

    [[nodiscard]] Camera* active() const;
    CameraManager& setActive(ecs::EntityHandle cameraEntity);
    [[nodiscard]] ecs::Entity* activeEntity() const;

    void onEntityAdded(ecs::EntityHandle entity) override {
      if (activeCamera == ecs::INVALID_ENTITY_HANDLE) {
        activeCamera = entity;
      }
    }
    void onEntityRemoved(ecs::EntityHandle entity) override {
      if (activeCamera == entity) {
        activeCamera = ecs::INVALID_ENTITY_HANDLE;
      }
    }

    [[nodiscard]] static inline ecs::Signature getSignature() {
      auto& ecs = ecs::ECS::get();
      return ecs.signatureFromComponents<Camera>();
    }

  private:
    static CameraManager* singleton;

    ecs::EntityHandle activeCamera = ecs::INVALID_ENTITY_HANDLE;
  };
} // namespace keptech::core::cameras
