#include "keptech/core/events/event.hpp"

#include "keptech/core/events/input.hpp"
#include <memory>

namespace kt {
  std::unique_ptr<Event> Event::fromSdl(const SDL_Event& sdlEvent) {
    switch (sdlEvent.type) {
    case SDL_EVENT_MOUSE_MOTION: {
      glm::vec2 movement{
          static_cast<float>(sdlEvent.motion.xrel),
          static_cast<float>(sdlEvent.motion.yrel),
      };
      return std::make_unique<MouseMovedEvent>(movement);
    }
    case SDL_EVENT_MOUSE_WHEEL: {
      glm::vec2 offset{
          static_cast<float>(sdlEvent.wheel.x),
          static_cast<float>(sdlEvent.wheel.y),
      };
      return std::make_unique<MouseScrolledEvent>(offset);
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      int button = sdlEvent.button.button;
      return std::make_unique<MouseButtonPressEvent>(button);
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      int button = sdlEvent.button.button;
      return std::make_unique<MouseButtonReleaseEvent>(button);
    }
    case SDL_EVENT_KEY_DOWN: {
      Keys key = static_cast<Keys>(sdlEvent.key.key);
      if (sdlEvent.key.repeat)
        return std::make_unique<KeyRepeatEvent>(key);
      return std::make_unique<KeyPressEvent>(key);
    }
    case SDL_EVENT_KEY_UP: {
      Keys key = static_cast<Keys>(sdlEvent.key.key);
      return std::make_unique<KeyReleaseEvent>(key);
    }
    case SDL_EVENT_TEXT_INPUT: {
      Keys key = static_cast<Keys>(sdlEvent.text.text[0]);
      return std::make_unique<KeyRepeatEvent>(key);
    }
    default:
      return nullptr;
    }
  }
} // namespace kt
