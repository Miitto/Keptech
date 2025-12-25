#pragma once

#include "base.hpp"
#include "frameData.hpp"
#include <set>

namespace keptech::ecs {
  class SystemManager;

  class System {
    friend class SystemManager;

  public:
    System() = default;
    System(const System&) = default;
    System(System&&) = delete;
    System& operator=(const System&) = default;
    System& operator=(System&&) = delete;

    virtual void preUpdate(const FrameData& frameData) { (void)frameData; }
    virtual void onUpdate(const FrameData& frameData) { (void)frameData; }
    virtual void postUpdate(const FrameData& frameData) { (void)frameData; }

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

    virtual ~System() = default;

  protected:
    std::set<EntityHandle> entities;
  };
} // namespace keptech::ecs
