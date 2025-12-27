#pragma once

#include "base.hpp"
#include "frameData.hpp"
#include <functional>
#include <set>

namespace keptech::ecs {
  class SystemManager;

  class System {
    friend class SystemManager;

  public:
    System() = default;
    System(const System&) = default;
    System(System&&) noexcept = default;
    System& operator=(const System&) = default;
    System& operator=(System&&) = default;

    virtual void preUpdate(const FrameData& frameData) { (void)frameData; }
    virtual void onUpdate(const FrameData& frameData) { (void)frameData; }
    virtual void postUpdate(const FrameData& frameData) { (void)frameData; }

    virtual void onEntityAdded(EntityHandle entity) { entities.insert(entity); }

    virtual void onEntityRemoved(EntityHandle entity) {
      entities.erase(entity);
    }

    std::function<void(const FrameData&)> getPreUpdateFunction() {
      return [this](const FrameData& frameData) { this->preUpdate(frameData); };
    }
    std::function<void(const FrameData&)> getUpdateFunction() {
      return [this](const FrameData& frameData) { this->onUpdate(frameData); };
    }
    std::function<void(const FrameData&)> getPostUpdateFunction() {
      return
          [this](const FrameData& frameData) { this->postUpdate(frameData); };
    }

    std::function<void(EntityHandle)> getOnEntityAddedFunction() {
      return [this](EntityHandle entity) { this->onEntityAdded(entity); };
    }
    std::function<void(EntityHandle)> getOnEntityRemovedFunction() {
      return [this](EntityHandle entity) { this->onEntityRemoved(entity); };
    }

    virtual ~System() = default;

  protected:
    std::set<EntityHandle> entities;
  };
} // namespace keptech::ecs
