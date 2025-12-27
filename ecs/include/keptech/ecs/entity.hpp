#pragma once

#include "base.hpp"
#include <string>
#include <utility>

namespace keptech::ecs {
  class Entity {
  public:
    Entity() : handle(INVALID_ENTITY_HANDLE), name("Unnamed") {}
    Entity(EntityHandle handle, std::string name)
        : handle(handle), name(std::move(name)) {}

    operator EntityHandle() const { return handle; }

    [[nodiscard]] const std::string& getName() const { return name; }
    [[nodiscard]] EntityHandle getHandle() const { return handle; }

    Signature& getSignature() { return signature; }

    [[nodiscard]] bool isValid() const {
      return handle != INVALID_ENTITY_HANDLE;
    }

    void onDestroy() {
      handle = INVALID_ENTITY_HANDLE;
      name = "Destroyed";
      signature.reset();
    }

  private:
    EntityHandle handle;
    std::string name;
    Signature signature;
  };
} // namespace keptech::ecs
