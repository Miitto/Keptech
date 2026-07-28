#pragma once

// Taken from Hazel Engine's event system

#include "keptech/core/base.hpp"
#include <SDL3/SDL_events.h>
#include <concepts>
#include <cstdint>
#include <memory>
#include <spdlog/fmt/bundled/format.h>

#define KT_MAKE_EVENT_FNS(EVENT_TYPE)                                                                                                      \
  [[nodiscard]] EventType getType() const override { return getStaticType(); }                                                             \
  [[nodiscard]] static constexpr EventType getStaticType() { return EventType::EVENT_TYPE; }

namespace kt {
  enum class EventType : uint8_t {
    WindowResize,
    KeyPressed,
    KeyRepeated,
    KeyReleased,
    MouseButtonPressed,
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

    static std::unique_ptr<Event> fromSdl(const SDL_Event& sdlEvent);

  private:
    bool handled = false;
  };

  template <typename T>
  concept IsEvent = std::is_base_of_v<Event, T> && requires(T a) {
    { a.getType() } -> std::same_as<EventType>;
    { T::getStaticType() } -> std::same_as<EventType>;
  };

  template <typename T, typename F>
  concept IsEventHandler = requires(F f, T& e) {
    { f(e) } -> std::same_as<bool>;
  };

  class EventDispatcher {
  public:
    EventDispatcher(Event& event) : event(event) {}

    template <IsEvent T, typename F>
    bool dispatch(const F& func)
      requires IsEventHandler<T, F>
    {
      if (event.getType() == T::getStaticType()) {
        event.handleIf(func(static_cast<T&>(event)));
        return true;
      }
      return false;
    }

  private:
    Event& event; // NOLINT
  };
} // namespace kt

template <> struct fmt::formatter<kt::EventType> : fmt::formatter<std::string_view> {

  template <typename FormatContext> auto format(const kt::EventType& event, FormatContext& ctx) const {
    std::string_view msg;

    using E = kt::EventType;

    switch (event) {
    case E::WindowResize:
      msg = "WindowResize";
      break;
    case E::KeyPressed:
      msg = "KeyPressed";
      break;
    case E::KeyRepeated:
      msg = "KeyRepeated";
      break;
    case E::KeyReleased:
      msg = "KeyReleased";
      break;
    case E::MouseButtonPressed:
      msg = "MouseButtonPressed";
      break;
    case E::MouseButtonReleased:
      msg = "MouseButtonReleased";
      break;
    case E::MouseMoved:
      msg = "MouseMoved";
      break;
    case E::MouseScrolled:
      msg = "MouseScrolled";
      break;
    }

    return fmt::formatter<std::string_view>::format(msg, ctx);
  }
};
