#pragma once

#include "event.hpp"
#include "keptech/core/input/keys.hpp"
#include <glm/glm.hpp>

namespace keptech::core::events {
  struct MouseMovedEvent : public Event {
    MouseMovedEvent(glm::vec2 movement) : movement(movement) {}
    KT_MAKE_EVENT_FNS(MouseMoved)
    glm::vec2 movement;
  };

  struct MouseScrolledEvent : public Event {
    MouseScrolledEvent(glm::vec2 offset) : offset(offset) {}
    KT_MAKE_EVENT_FNS(MouseScrolled)
    glm::vec2 offset;
  };

  struct MouseButtonEvent : public Event {
    MouseButtonEvent(int button) : button(button) {}
    int button;
  };

  struct MouseButtonPressEvent : public MouseButtonEvent {
    MouseButtonPressEvent(int button) : MouseButtonEvent(button) {}
    KT_MAKE_EVENT_FNS(MouseButtonPressed)
  };

  struct MouseButtonReleaseEvent : public MouseButtonEvent {
    MouseButtonReleaseEvent(int button) : MouseButtonEvent(button) {}
    KT_MAKE_EVENT_FNS(MouseButtonReleased)
  };

  struct KeyEvent : public Event {
    KeyEvent(input::Keys key) : key(key) {}
    input::Keys key;
  };

  struct KeyPressEvent : public KeyEvent {
    KeyPressEvent(input::Keys key) : KeyEvent(key) {}
    KT_MAKE_EVENT_FNS(KeyPressed)
  };

  struct KeyRepeatEvent : public KeyEvent {
    KeyRepeatEvent(input::Keys key) : KeyEvent(key) {}
    KT_MAKE_EVENT_FNS(KeyRepeated)
  };

  struct KeyReleaseEvent : public KeyEvent {
    KeyReleaseEvent(input::Keys key) : KeyEvent(key) {}
    KT_MAKE_EVENT_FNS(KeyReleased)
  };
} // namespace keptech::core::events
