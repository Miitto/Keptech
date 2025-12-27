#pragma once

#include <atomic>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace keptech::core {
  using SlotMapHandle = size_t;
  template <typename T> class SlotMap {
  public:
    using Handle = SlotMapHandle;

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

    [[nodiscard]] std::vector<SlotMapHandle> handles() const {
      std::vector<SlotMapHandle> handles;
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
    std::atomic<size_t> strongRefs = 0;
    std::atomic<size_t> weakRefs = 0;

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

    bool hasStrongRefs() const {
      return strongRefs.load(std::memory_order_relaxed) > 0;
    }

    bool hasWeakRefs() const {
      return weakRefs.load(std::memory_order_relaxed) > 0;
    }

    bool hasAnyRefs() const { return hasStrongRefs() || hasWeakRefs(); }
  };

  class SlotMapSmartHandle;

  class SlotMapWeakHandle {
  public:
    friend class SlotMapSmartHandle;
    SlotMapWeakHandle() = delete;

    SlotMapWeakHandle(const SlotMapWeakHandle& o)
        : handle(o.handle), refCount(o.refCount) {
      if (!refCount)
        return;
      refCount->newWeakRef();
    }
    SlotMapWeakHandle& operator=(const SlotMapWeakHandle& o) {
      if (this != &o) {
        handle = o.handle;
        refCount = o.refCount;
        if (!refCount)
          return *this;
        refCount->newWeakRef();
      }
      return *this;
    }
    SlotMapWeakHandle(SlotMapWeakHandle&& o) noexcept
        : handle(o.handle), refCount(o.refCount) {
      o.refCount = nullptr;
    }

    SlotMapWeakHandle& operator=(SlotMapWeakHandle&& o) noexcept {
      if (this != &o) {
        handle = o.handle;
        refCount = o.refCount;
        o.refCount = nullptr;
      }
      return *this;
    }

    SlotMapWeakHandle(SlotMapHandle handle, SlotMapRefs& refCount)
        : handle(handle), refCount(&refCount) {
      this->refCount->newWeakRef();
    }

    ~SlotMapWeakHandle() {
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

    operator SlotMapHandle() const { return handle; }
    [[nodiscard]] SlotMapHandle get() const { return handle; }

  private:
    SlotMapHandle handle;
    SlotMapRefs* refCount;
  };

  class SlotMapSmartHandle {
  public:
    SlotMapSmartHandle() = delete;
    SlotMapSmartHandle(const SlotMapSmartHandle& o)
        : handle(o.handle), refCount(o.refCount), deleter(o.deleter) {
      if (!refCount)
        return;
      refCount->newStrongRef();
    }
    SlotMapSmartHandle& operator=(const SlotMapSmartHandle& o) {
      if (this != &o) {
        handle = o.handle;
        deleter = o.deleter;
        refCount = o.refCount;
        if (!refCount)
          return *this;
        refCount->newStrongRef();
      }
      return *this;
    }
    SlotMapSmartHandle(SlotMapSmartHandle&& o) noexcept
        : handle(o.handle), refCount(o.refCount),
          deleter(std::move(o.deleter)) {
      o.refCount = nullptr;
    }
    SlotMapSmartHandle& operator=(SlotMapSmartHandle&& o) noexcept {
      if (this != &o) {
        handle = o.handle;
        deleter = std::move(o.deleter);
        refCount = o.refCount;
        o.refCount = nullptr;
      }
      return *this;
    }
    ~SlotMapSmartHandle() {
      if (refCount == nullptr)
        return;

      refCount->delStrongRef();

      if (!refCount->hasStrongRefs()) {
        deleter();
      }

      if (!refCount->hasAnyRefs()) {
        delete refCount;
      }

      refCount = nullptr;
    }

    template <typename T>
    SlotMapSmartHandle(SlotMapHandle handle, SlotMap<T>& map)
        : handle(handle), refCount(new SlotMapRefs()),
          deleter([this, &map]() { map.erase(this->handle); }) {
      refCount->newStrongRef();
    }

    SlotMapSmartHandle(SlotMapHandle handle, std::function<void()> deleter)
        : handle(handle), refCount(new SlotMapRefs()),
          deleter(std::move(deleter)) {}

    SlotMapSmartHandle(SlotMapHandle handle, SlotMapRefs& refCount,
                       std::function<void()> deleter)
        : handle(handle), refCount(&refCount), deleter(std::move(deleter)) {
      this->refCount->newStrongRef();
    }

    SlotMapSmartHandle(const SlotMapWeakHandle& weakHandle,
                       std::function<void()> deleter)
        : handle(weakHandle.get()), refCount(weakHandle.refCount),
          deleter(std::move(deleter)) {
      if (refCount == nullptr || !refCount->hasStrongRefs()) {
        throw std::runtime_error(
            "Cannot promote weak handle to strong handle: no strong refs");
      }
      refCount->newStrongRef();
    }

    operator SlotMapHandle() const { return handle; }

    [[nodiscard]] SlotMapHandle get() const { return handle; }

    [[nodiscard]] bool valid() const {
      return refCount != nullptr && refCount->hasStrongRefs();
    }

    [[nodiscard]] SlotMapWeakHandle toWeak() const {
      return {handle, *refCount};
    }

  private:
    SlotMapHandle handle;
    SlotMapRefs* refCount;
    std::function<void()> deleter;
  };

} // namespace keptech::core
