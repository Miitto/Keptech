#pragma once

#include <map>
#include <optional>
#include <vector>

namespace keptech::core {
  template <typename T> class SlotMap {
  public:
    using Handle = size_t;

    SlotMap() = default;
    SlotMap(const SlotMap&) = default;
    SlotMap& operator=(const SlotMap&) = default;
    SlotMap(SlotMap&& o) noexcept
        : nextFree(o.nextFree), data(std::move(o.data)),
          indexMap(std::move(o.indexMap)) {
      o.nextFree = 0;
    }
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

    [[nodiscard]] bool has(Handle handle) const {
      return indexMap.find(handle) != indexMap.end() &&
             data[indexMap.at(handle)].has_value();
    }

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

    T& operator[](Handle handle) {
      return data.at(indexMap.at(handle)).value();
    }

    const T& operator[](Handle handle) const {
      return data.at(indexMap.at(handle)).value();
    }

    std::optional<T> erase(Handle handle) {
      auto it = indexMap.find(handle);
      if (it == indexMap.end()) {
        return std::nullopt;
      }
      size_t index = it->second;
      std::optional<T> value = std::move(data[index]);
      data[index] = std::nullopt;
      indexMap.erase(it);
      if (index < nextFree) {
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

    /// DO NOT USE: Resets the entire SlotMap, invalidating all handles.
    /// This may allow for accidental reuse of handles and should be used with
    /// caution.
    void reset() {
      nextFree = 0;
      nextHandle = 0;
      data.clear();
      indexMap.clear();
    }

    std::vector<T*> values() {
      std::vector<T*> vals;
      for (auto& opt : data) {
        if (opt.has_value()) {
          vals.push_back(&opt.value());
        }
      }
      return vals;
    }

  private:
    size_t nextFree = 0;
    Handle nextHandle = 0;
    std::vector<std::optional<T>> data;
    std::map<Handle, size_t> indexMap;
  };
} // namespace keptech::core
