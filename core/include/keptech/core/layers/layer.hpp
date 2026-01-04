#pragma once

#include "keptech/core/base.hpp"
#include "keptech/core/events/event.hpp"

namespace keptech::core::layers {
  class Layer {
  public:
#ifndef NDEBUG
    Layer(std::string name) : debugName(std::move(name)) {}
    [[nodiscard]] const std::string& getDebugName() const { return debugName; }
#else
    Layer([[maybe_unused]] std::string) : Layer() {}
#endif
    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;
    Layer(Layer&&) = delete;
    Layer& operator=(Layer&&) = delete;

    virtual ~Layer() = default;

    virtual void onAttach() {}
    virtual void onDetach() {}
    virtual void onUpdate([[maybe_unused]] Timestep ts) {}
    virtual bool onEvent([[maybe_unused]] events::Event& event) {
      return false;
    }

  private:
#ifndef NDEBUG
    std::string debugName;
#endif
  };
} // namespace keptech::core::layers
