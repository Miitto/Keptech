#pragma once

#include "base.hpp"
#include <set>

namespace keptech::ecs {
  class SystemManager;

  class System {
    friend class SystemManager;

  protected:
    std::set<EntityHandle> entities;
  };
} // namespace keptech::ecs
