#include "keptech/input.hpp"
#include "keptech/core/kt-logger.hpp"

namespace kt {
  Input Input::singleton{};

  void Input::endFrame() {
    for (auto& [key, state] : keyStates) {
      if (state == KeyState::Release) {
        state = KeyState::Up;
      } else if (state == KeyState::Press) {
        state = KeyState::Down;
      }
    }

    for (u32 i = 0; i < 32; i += 2) {
      u32 shiftedMask = (mouseButtonMask >> i) & 0b11;
      if (shiftedMask == (u32)KeyState::Release) {
        mouseButtonMask &= ~(0b11 << i);
      } else if (shiftedMask == (u32)KeyState::Press) {
        mouseButtonMask &= ~(0b11 << i);
        mouseButtonMask |= (u32)KeyState::Down << i;
      }
    }
  }

  [[nodiscard]] KeyState Input::getKeyState(int key) const {
    return keyStates.contains(key) ? keyStates.at(key).as_enum() : KeyState::Up;
  }
  [[nodiscard]] bool Input::isKeyDown(int key) const {
    auto state =
        keyStates.contains(key) ? keyStates.at(key) : Bitflag(KeyState::Up);
    return state.has(KeyState::Down);
  }
  [[nodiscard]] bool Input::isKeyUp(int key) const {
    auto state =
        keyStates.contains(key) ? keyStates.at(key) : Bitflag(KeyState::Up);
    return state == KeyState::Up || state == KeyState::Release;
  }
  [[nodiscard]] bool Input::hasKeyChanged(int key) const {
    auto state =
        keyStates.contains(key) ? keyStates.at(key) : Bitflag(KeyState::Up);
    return state == KeyState::Press || state == KeyState::Release;
  }
  void Input::registerKeyPress(uint32_t key) {
    keyStates[key] = KeyState::Press;
    KT_TRACE("Registered key press: key={}, state={:02b}", key,
             keyStates[key].as_underlying());
  }
  void Input::registerKeyRelease(uint32_t key) {
    keyStates[key] = KeyState::Release;
    KT_TRACE("Registered key release: key={}, state={:02b}", key,
             keyStates[key].as_underlying());
  }
  void Input::registerMouseButtonPress(uint32_t button) {
    mouseButtonMask |= (u32)KeyState::Press << (button * 2);
    // Two extra spaces to align with release
    KT_TRACE("Registered mouse button press:   button={}, mask={:020b}", button,
             mouseButtonMask);
  }
  void Input::registerMouseButtonRelease(uint32_t button) {
    u32 wipeMask = ~(0b11 << (button * 2));
    mouseButtonMask &= wipeMask;
    mouseButtonMask |= (u32)KeyState::Release << (button * 2);
    KT_TRACE("Registered mouse button release: button={}, mask={:020b}", button,
             mouseButtonMask);
  }
  [[nodiscard]] bool Input::isMouseButtonDown(uint32_t button) const {
    u32 buttonState = (mouseButtonMask >> (button * 2)) & 0b11;
    return (buttonState & (u32)KeyState::Down) != 0;
  }
} // namespace kt
