#pragma once

#include <string>

namespace keptech::components {
  struct Name {
    std::string name;

    operator std::string() { return name; }

    std::string* operator->() { return &name; }

    std::string& operator*() { return name; }
  };
} // namespace keptech::components
