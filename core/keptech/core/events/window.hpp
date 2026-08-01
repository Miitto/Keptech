#pragma once

#include "event.hpp"
#include <glm/glm.hpp>

namespace kt {
  struct WindowResizeEvent : public Event {
    WindowResizeEvent(glm::uvec2 size) : size(size) {}
    KT_MAKE_EVENT_FNS(WindowResize)
    glm::uvec2 size;
  };
} // namespace kt