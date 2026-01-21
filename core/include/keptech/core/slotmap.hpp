#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace keptech::core {
  using SlotMapRawHandle = size_t;
  template <typename T> class SlotMap {
  public:
    using Handle = SlotMapRawHandle;

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

    /// DO NOT USE: Resets the entire SlotMap, invalidating all handles.
    /// This may allow for accidental reuse of handles and should be used with
    /// caution.
    void reset() {
      nextFree = 0;
      nextHandle = 0;
      data.clear();
      indexMap.clear();
    }

    [[nodiscard]] std::vector<SlotMapRawHandle> handles() const {
      std::vector<SlotMapRawHandle> handles;
      handles.reserve(indexMap.size());
      for (const auto& [handle, index] : indexMap) {
        handles.push_back(handle);
      }
      return handles;
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

    std::vector<std::optional<T>>& rawData() { return data; }

    /// Packs the SlotMap to remove gaps from erased elements.
    void pack() {
      std::vector<std::optional<size_t>> handleIndices;

      size_t indexMapSize = indexMap.size();
      size_t dataSize = data.size();

      size_t maxSize = std::max(indexMapSize, dataSize);

      handleIndices.resize(maxSize);

      for (const auto& [handle, index] : indexMap) {
        handleIndices[index] = handle;
      }

      for (size_t i = 0; i < data.size(); ++i) {
        std::optional<T>& dataOpt = data[i];
        if (!dataOpt.has_value()) {
          // Find next valid entry
          size_t j = i + 1;
          while (j < data.size() && !data[j].has_value()) {
            ++j;
          }
          if (j >= data.size()) {
            break; // No more valid entries
          }
          // Move entry from j to i
          data[i] = std::move(data[j]);
          data[j] = std::nullopt;

          // Update indexMap
          std::optional<size_t>& handleOpt = handleIndices[j];
          if (handleOpt.has_value()) {
            Handle handle = handleOpt.value();
            indexMap[handle] = i;
            handleOpt = std::nullopt;
            handleIndices[i] = handle;
          }
        }
      }

      for (size_t i = data.size(); i-- > 0;) {
        if (data[i].has_value()) {
          nextFree = i + 1;
          break;
        }
      }
    }

    [[nodiscard]] size_t size() const { return indexMap.size(); }

  private:
    size_t nextFree = 0;
    Handle nextHandle = 0;
    std::vector<std::optional<T>> data;
    std::unordered_map<Handle, size_t> indexMap;
  };

  struct SlotMapRefs {
    std::atomic<size_t> strongRefs = 1;
    std::atomic<size_t> weakRefs = 0;
    std::function<void()> deleter;

    SlotMapRefs(std::function<void()> deleter)
        : strongRefs(1), weakRefs(0), deleter(std::move(deleter)) {}

    void newStrongRef() { strongRefs.fetch_add(1, std::memory_order_seq_cst); }

    void delStrongRef() { strongRefs.fetch_sub(1, std::memory_order_seq_cst); }

    void newWeakRef() { weakRefs.fetch_add(1, std::memory_order_seq_cst); }

    void delWeakRef() { weakRefs.fetch_sub(1, std::memory_order_seq_cst); }

    void strongToWeak() {
      strongRefs.fetch_sub(1, std::memory_order_seq_cst);
      weakRefs.fetch_add(1, std::memory_order_seq_cst);
    }

    void weakToStrong() {
      weakRefs.fetch_sub(1, std::memory_order_seq_cst);
      strongRefs.fetch_add(1, std::memory_order_seq_cst);
    }

    [[nodiscard]] bool hasStrongRefs() const {
      return strongRefs.load(std::memory_order_relaxed) > 0;
    }

    [[nodiscard]] bool hasWeakRefs() const {
      return weakRefs.load(std::memory_order_relaxed) > 0;
    }

    [[nodiscard]] bool hasAnyRefs() const {
      return hasStrongRefs() || hasWeakRefs();
    }
  };

  class SlotMapRawSmartHandle;

  class SlotMapRawWeakHandle {
  public:
    friend class SlotMapRawSmartHandle;
    SlotMapRawWeakHandle() = delete;

    SlotMapRawWeakHandle(const SlotMapRawWeakHandle& o)
        : handle(o.handle), refCount(o.refCount) {
      if (!refCount)
        return;
      refCount->newWeakRef();
    }
    SlotMapRawWeakHandle& operator=(const SlotMapRawWeakHandle& o) {
      if (this != &o) {
        handle = o.handle;
        refCount = o.refCount;
        if (!refCount)
          return *this;
        refCount->newWeakRef();
      }
      return *this;
    }
    SlotMapRawWeakHandle(SlotMapRawWeakHandle&& o) noexcept
        : handle(o.handle), refCount(o.refCount) {
      o.refCount = nullptr;
    }

    SlotMapRawWeakHandle& operator=(SlotMapRawWeakHandle&& o) noexcept {
      if (this != &o) {
        handle = o.handle;
        refCount = o.refCount;
        o.refCount = nullptr;
      }
      return *this;
    }

    SlotMapRawWeakHandle(SlotMapRawHandle handle, SlotMapRefs& refCount)
        : handle(handle), refCount(&refCount) {
      this->refCount->newWeakRef();
    }

    ~SlotMapRawWeakHandle() {
      if (refCount == nullptr)
        return;
      refCount->delWeakRef();

      if (!refCount->hasAnyRefs()) {
        delete refCount;
      }

      refCount = nullptr;
    }

    [[nodiscard]] bool valid() const {
      return refCount != nullptr && refCount->hasStrongRefs();
    }

    operator SlotMapRawHandle() const { return handle; }
    [[nodiscard]] SlotMapRawHandle get() const { return handle; }

  private:
    SlotMapRawHandle handle;
    SlotMapRefs* refCount;
  };

  class SlotMapRawSmartHandle {
  public:
    SlotMapRawSmartHandle() = delete;
    SlotMapRawSmartHandle(const SlotMapRawSmartHandle& o)
        : handle(o.handle), refCount(o.refCount) {
      if (!refCount)
        return;
      refCount->newStrongRef();
    }
    SlotMapRawSmartHandle& operator=(const SlotMapRawSmartHandle& o) {
      if (this != &o) {
        handle = o.handle;
        refCount = o.refCount;
        if (!refCount)
          return *this;
        refCount->newStrongRef();
      }
      return *this;
    }
    SlotMapRawSmartHandle(SlotMapRawSmartHandle&& o) noexcept
        : handle(o.handle), refCount(o.refCount) {
      o.refCount = nullptr;
    }
    SlotMapRawSmartHandle& operator=(SlotMapRawSmartHandle&& o) noexcept {
      if (this != &o) {
        handle = o.handle;
        refCount = o.refCount;
        o.refCount = nullptr;
      }
      return *this;
    }
    ~SlotMapRawSmartHandle() {
      if (refCount == nullptr)
        return;

      refCount->delStrongRef();

      if (!refCount->hasStrongRefs()) {
        refCount->deleter();
      }

      if (!refCount->hasAnyRefs()) {
        delete refCount;
      }

      refCount = nullptr;
    }

    template <typename T>
    SlotMapRawSmartHandle(SlotMapRawHandle handle, SlotMap<T>& map)
        : handle(handle), refCount(new SlotMapRefs(
                              [this, &map]() { map.erase(this->handle); })) {}

    SlotMapRawSmartHandle(SlotMapRawHandle handle,
                          std::function<void()> deleter)
        : handle(handle), refCount(new SlotMapRefs(std::move(deleter))) {}

    SlotMapRawSmartHandle(SlotMapRawHandle handle, SlotMapRefs& refCount)
        : handle(handle), refCount(&refCount) {}

    SlotMapRawSmartHandle(const SlotMapRawWeakHandle& weakHandle)
        : handle(weakHandle.get()), refCount(weakHandle.refCount) {
      if (refCount == nullptr || !refCount->hasStrongRefs()) {
        throw std::runtime_error(
            "Cannot promote weak handle to strong handle: no strong refs");
      }
      refCount->weakToStrong();
    }

    operator SlotMapRawHandle() const { return handle; }

    [[nodiscard]] SlotMapRawHandle get() const { return handle; }

    [[nodiscard]] bool valid() const {
      return refCount != nullptr && refCount->hasStrongRefs();
    }

    [[nodiscard]] SlotMapRawWeakHandle toWeak() const {
      return {handle, *refCount};
    }

  private:
    SlotMapRawHandle handle;
    SlotMapRefs* refCount;
  };

  template <typename T> class SlotMapSmartHandle;

  template <typename T> class SlotMapHandle {
  public:
    SlotMapHandle() : handle(0) {}
    SlotMapHandle(SlotMapRawHandle handle) : handle(handle) {}

    operator SlotMapRawHandle() const { return handle; }

    bool operator==(const SlotMapHandle& other) const {
      return handle == other.handle;
    }
    bool operator==(const SlotMapRawHandle other) const {
      return handle == other;
    }
    bool operator==(const SlotMapSmartHandle<T>& other) const {
      return handle == other;
    }

  private:
    SlotMapRawHandle handle;
  };

  template <typename T> class SlotMapSmartHandle {
  public:
    SlotMapSmartHandle(SlotMapRawSmartHandle handle)
        : handle(std::move(handle)) {}

    operator SlotMapRawSmartHandle() const { return handle; }

    bool operator==(const SlotMapSmartHandle& other) const {
      return handle == other.handle;
    }
    bool operator==(const SlotMapRawHandle other) const {
      return handle == other;
    }
    bool operator==(const SlotMapHandle<T>& other) const {
      return handle == other;
    }

  private:
    SlotMapRawSmartHandle handle;
  };
} // namespace keptech::core
