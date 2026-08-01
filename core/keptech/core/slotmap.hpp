#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kt {
  template <typename T> class SlotMap;

  class SlotMapHandle {
  public:
    template <typename T> friend class SlotMap;
    SlotMapHandle() = default;

    operator bool() const { return value != ~0u; }

  private:
    SlotMapHandle(size_t v) : value(v) {}
    SlotMapHandle operator++() {
      ++value;
      return *this;
    }

    size_t value = ~0u;
  };

  template <typename T> class SlotMap {
  public:
    using Handle = SlotMapHandle;

    SlotMap() = default;
    SlotMap(const SlotMap&) = default;
    SlotMap& operator=(const SlotMap&) = default;
    SlotMap(SlotMap&& o) noexcept : nextFree(o.nextFree), data(std::move(o.data)), indexMap(std::move(o.indexMap)) { o.nextFree = 0; }
    SlotMap& operator=(SlotMap&& o) noexcept {
      if (*this != o) {
        nextFree = o.nextFree;
        data = std::move(o.data);
        indexMap = std::move(o.indexMap);
        o.nextFree = 0;
      }
      return *this;
    }
    ~SlotMap() = default;

    [[nodiscard]] bool has(Handle handle) const { return indexMap.find(handle) != indexMap.end() && data[indexMap.at(handle)].has_value(); }

    [[nodiscard]] Handle insert(const T& value) {
      size_t index = nextFree;
      if (nextFree < data.size()) {
        index = nextFree;
        data[index] = value;
        while (nextFree < data.size() && data[nextFree].has_value()) {
          ++nextFree;
        }
      } else {
        index = data.size();
        data.push_back(value);
        nextFree = data.size();
      }
      Handle handle = ++nextHandle;
      indexMap[handle] = index;
      return handle;
    }

    [[nodiscard]] Handle insert(T&& value) {
      size_t index = nextFree;
      if (nextFree < data.size()) {
        index = nextFree;
        data[index] = std::move(value);
        while (nextFree < data.size() && data[nextFree].has_value()) {
          ++nextFree;
        }
      } else {
        index = data.size();
        data.push_back(std::move(value));
        nextFree = data.size();
      }
      Handle handle = ++nextHandle;
      indexMap[handle] = index;
      return handle;
    }

    template <typename... Args> [[nodiscard]] Handle emplace(Args&&... args) {
      size_t index = nextFree;
      if (nextFree < data.size()) {
        index = nextFree;
        data[index] = T(std::forward<Args>(args)...);
        while (nextFree < data.size() && data[nextFree].has_value()) {
          ++nextFree;
        }
      } else {
        index = data.size();
        data.emplace_back(T(std::forward<Args>(args)...));
        nextFree = data.size();
      }
      Handle handle = ++nextHandle;
      indexMap[handle] = index;
      return handle;
    }

    T& operator[](Handle handle) { return data.at(indexMap.at(handle)).value(); }

    const T& operator[](Handle handle) const { return data.at(indexMap.at(handle)).value(); }

    std::optional<T> erase(Handle handle, bool swapEnd = false) {
      auto it = indexMap.find(handle);
      if (it == indexMap.end()) {
        return std::nullopt;
      }
      size_t index = it->second;
      std::optional<T> value = std::move(data[index]);
      data[index] = std::nullopt;
      indexMap.erase(it);
      if (swapEnd) {
        size_t lastIndex = data.size() - 1;
        for (; lastIndex > 0; --lastIndex) {
          if (data[lastIndex].has_value()) {
            break;
          }
        }
        if (!data[lastIndex].has_value()) {
          if (index < nextFree) {
            nextFree = index;
          }
          return value;
        }

        data[index] = std::move(data[lastIndex]);
        data[lastIndex] = std::nullopt;

        if (lastIndex < nextFree) {
          nextFree = lastIndex;
        }
      } else if (index < nextFree) {
        nextFree = index;
      }
      return value;
    }

    const T* get(Handle handle) const {
      auto it = indexMap.find(handle);
      if (it == indexMap.end()) {
        return nullptr;
      }
      size_t index = it->second;
      return &data[index].value();
    }

    T* get(Handle handle) {
      auto it = indexMap.find(handle);
      if (it == indexMap.end()) {
        return nullptr;
      }
      size_t index = it->second;
      return &data[index].value();
    }

    /// Resets the entire SlotMap, invalidating all handles.
    /// This may allow for accidental reuse of handles and should be used with
    /// caution.
    void reset() {
      nextFree = 0;
      nextHandle = 0;
      data.clear();
      indexMap.clear();
    }

    /// @brief Returns a vector of all valid handles in the SlotMap.
    /// @return A vector of all valid handles.
    [[nodiscard]] std::vector<SlotMapHandle> handles() const {
      std::vector<SlotMapHandle> handles;
      handles.reserve(indexMap.size());
      for (const auto& [handle, index] : indexMap) {
        handles.push_back(handle);
      }
      return handles;
    }

    /// @brief Returns a vector of all valid values in the SlotMap.
    /// @return A vector of all valid values.
    std::vector<T*> values() {
      std::vector<T*> vals;
      for (auto& opt : data) {
        if (opt.has_value()) {
          vals.push_back(&opt.value());
        }
      }
      return vals;
    }

    std::vector<std::optional<T>>& rawData() { return data; }

    [[nodiscard]] size_t size() const { return indexMap.size(); }

    /// Returns the internal index of the given handle, or std::nullopt if not
    /// present. Used for when using a SlotMap as a backing store for another
    /// container.
    std::optional<size_t> indexOf(Handle handle) const {
      auto it = indexMap.find(handle);
      if (it == indexMap.end()) {
        return std::nullopt;
      }
      return it->second;
    }

  private:
    size_t nextFree = 0;
    Handle nextHandle = 0;
    std::vector<std::optional<T>> data;
    std::unordered_map<Handle, size_t> indexMap;
  };
} // namespace kt
