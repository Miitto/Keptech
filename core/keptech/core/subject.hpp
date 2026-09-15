#pragma once

#include <functional>

namespace kt {
  template <typename T> class Subject {
  public:
    using Fn = std::function<void(const T&)>;

    void registerObserver(Fn observer) { observers.push_back(observer); }

    void unregisterObserver(Fn observer) { observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end()); }

    void notify(const T& data) {
      for (const auto& observer : observers) {
        observer(data);
      }
    }

  private:
    std::vector<Fn> observers;
  };
} // namespace kt