#pragma once

#include <vector>

namespace kt {
  template <typename T> class Vec : public std::vector<T> {
  public:
    template <typename F> void retain(const F& f) {
      auto it = std::remove_if(this->begin(), this->end(), [&](const T& v) { return !f(v); });
      this->erase(it, this->end());
    }

    Vec clone() const { return Vec(*this); }

    template <class Self> auto&& std(this Self&& self) { return std::forward<Self>(self); }

    Vec() = default;
    Vec(std::initializer_list<T> init) : std::vector<T>(init) {}
    Vec(const std::vector<T>& vec) : std::vector<T>(vec) {}
    Vec(std::vector<T>&& vec) : std::vector<T>(std::move(vec)) {}

    Vec(size_t count) : std::vector<T>(count) {}

    template <typename... Args> Vec(size_t count, Args&&... args) : std::vector<T>(count, std::forward<Args>(args)...) {}

    explicit Vec(const Vec<T>& vec) : std::vector<T>(vec) {};
    Vec& operator=(const Vec<T>& vec) {
      std::vector<T>::operator=(vec);
      return *this;
    }
    Vec(Vec<T>&& vec) noexcept : std::vector<T>(std::move(vec)) {}
    Vec& operator=(Vec<T>&& vec) noexcept {
      std::vector<T>::operator=(std::move(vec));
      return *this;
    }
    ~Vec() = default;
  };
} // namespace kt