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

    virtual void onRender(const FrameData& frameData) { (void)frameData; }

    virtual ~System() = default;

  protected:
    std::set<EntityHandle> entities;
  };
} // namespace keptech::ecs
