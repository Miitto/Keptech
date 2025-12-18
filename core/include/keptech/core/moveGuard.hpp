#pragma once

namespace keptech::core {
  class MoveGuard {
  public:
    explicit MoveGuard() = default;
    MoveGuard(const MoveGuard&) = delete;
    MoveGuard& operator=(const MoveGuard&) = delete;
    MoveGuard(MoveGuard&& o) noexcept : _moved(o._moved) { o._moved = true; }
    MoveGuard& operator=(MoveGuard&& o) noexcept {
      if (this != &o) {
        _moved = o._moved;
        o._moved = true;
      }
      return *this;
    }
    ~MoveGuard() = default;

    [[nodiscard]] bool moved() const noexcept { return _moved; }

  private:
    bool _moved = false;
  };
} // namespace keptech::core
