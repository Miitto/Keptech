#pragma once

#include "keptech/core/base.hpp"
#include <keptech/core/bitflag.hpp>
#include <keptech/core/macros.hpp>
#include <unordered_map>

namespace keptech {

  enum class KeyState : uint8_t {
    /// Key is not pressed
    Up = 0,
    /// Key was released this frame
    Release = BIT(0),
    /// Key is being held down
    Down = BIT(1),
    /// Key was pressed this frame
    Press = BIT(0) | BIT(1),
  };
}

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::KeyState)

namespace keptech {
  class Input {
  public:
    static inline Input& get() { return singleton; }

    [[nodiscard]] KeyState getKeyState(int key) const;
    [[nodiscard]] bool isKeyDown(int key) const;

    [[nodiscard]] bool isKeyUp(int key) const;

    [[nodiscard]] bool hasKeyChanged(int key) const;

    void registerKeyPress(uint32_t key);
    void registerKeyRelease(uint32_t key);
    void registerMouseButtonPress(uint32_t button);
    void registerMouseButtonRelease(uint32_t button);
    [[nodiscard]] bool isMouseButtonDown(uint32_t button) const;

    void endFrame();

  private:
    static Input singleton;
    std::unordered_map<u32, Bitflag<KeyState>> keyStates;
    u32 mouseButtonMask = 0;
    glm::vec3 mousePosition{0.f};
    glm::vec3 mouseDelta{0.f};
  };
} // namespace keptech
