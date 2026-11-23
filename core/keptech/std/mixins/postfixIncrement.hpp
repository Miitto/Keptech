#pragma once

namespace kt {
  class PostfixIncrement {
  public:
    template <typename Self> Self operator++(this Self& self, int) {
      Self copy = self;
      ++self;
      return copy;
    }
  };
} // namespace kt