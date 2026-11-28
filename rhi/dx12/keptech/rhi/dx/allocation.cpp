#include "allocation.hpp"

namespace kt::rhi {
  Allocation Allocation::unique(D3D12MA::Allocation* alloc) { return Allocation(alloc, 1); }
  Allocation Allocation::shared(D3D12MA::Allocation* alloc, size_t count) { return Allocation(alloc, count); }
  size_t Allocation::getRefCount() const { return ref_count ? ref_count->load() : 0ull; }

  void Allocation::release() {
    if (ref_count && --(*ref_count) == 0) {
      allocation->Release();
      delete ref_count;
    }
  }
  void Allocation::reset() {
    release();
    allocation = nullptr;
    ref_count = nullptr;
  }

  D3D12MA::Allocation* Allocation::get() const { return allocation; }
  D3D12MA::Allocation* Allocation::operator->() const { return allocation; }
  bool Allocation::operator==(const Allocation& other) const { return allocation == other.allocation; }
  bool Allocation::operator!=(const Allocation& other) const { return allocation != other.allocation; }

  Allocation::operator bool() const { return allocation != nullptr; }

  Allocation::Allocation(const Allocation& other) : allocation(other.allocation), ref_count(other.ref_count) {
    if (ref_count) {
      ++(*ref_count);
    }
  }

  Allocation& Allocation::operator=(const Allocation& other) {
    if (this != &other) {
      allocation = other.allocation;
      ref_count = other.ref_count;
      if (ref_count) {
        ++(*ref_count);
      }
    }
    return *this;
  }

  Allocation::Allocation(Allocation&& other) noexcept : allocation(other.allocation), ref_count(other.ref_count) {
    other.allocation = nullptr;
    other.ref_count = nullptr;
  }

  Allocation& Allocation::operator=(Allocation&& other) noexcept {
    if (this != &other) {
      allocation = other.allocation;
      ref_count = other.ref_count;
      other.allocation = nullptr;
      other.ref_count = nullptr;
    }
    return *this;
  }

  Allocation::~Allocation() { release(); }
  Allocation::Allocation(D3D12MA::Allocation* alloc, size_t count) : allocation(alloc), ref_count(new std::atomic_size_t(count)) {}

} // namespace kt::rhi