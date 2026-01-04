#pragma once

// Taken from Hazel Engine's event system

#include "SDL3/SDL_events.h"

#define KT_MAKE_EVENT_FNS(EVENT_TYPE)                                          \
  [[nodiscard]] EventType getType() const override { return getStaticType(); } \
  [[nodiscard]] static constexpr EventType getStaticType() {                   \
    return EventType::EVENT_TYPE;                                              \
  }

namespace keptech::core::events {
  enum class EventType : uint8_t {
    WindowResize,
    KeyPressed,
    KeyRepeated,
    KeyReleased,
    MouseButtonPressed,
    MouseButtonDoubleClicked,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled,
  };

  class Event {
  public:
    Event() = default;
    Event(const Event&) = default;
    Event(Event&&) = delete;
    Event& operator=(const Event&) = default;
    Event& operator=(Event&&) = delete;
    virtual ~Event() = default;

    [[nodiscard]] virtual EventType getType() const = 0;

    void handle() { handled = true; }
    void handleIf(bool condition) { handled |= condition; }
    [[nodiscard]] bool isHandled() const { return handled; }

  private:
    bool handled = false;
  };

  template <typename T>
  concept IsEvent = std::is_base_of_v<Event, T> && requires(T a) {
    { a.getType() } -> std::same_as<EventType>;
    { T::getStaticType() } -> std::same_as<EventType>;
  };

  class EventDispatcher {
  public:
    EventDispatcher(Event& event) : event(event) {}

    template <IsEvent T, typename F> bool dispatch(const F& func) {
      if (event.getType() == T::getStaticType()) {
        event.handleIf(func(static_cast<T&>(event)));
        return true;
      }
      return false;
    }

  private:
    Event& event; // NOLINT
  };

  std::unique_ptr<Event> sdlEventToKeptechEvent(const SDL_Event& sdlEvent);
} // namespace keptech::core::events
