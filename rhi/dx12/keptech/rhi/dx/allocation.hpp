#pragma once

#include <D3D12MemAlloc.h>

namespace kt::rhi {
  class Allocation {
  public:
    static Allocation unique(D3D12MA::Allocation* alloc);

    static Allocation shared(D3D12MA::Allocation* alloc, size_t count);

    size_t getRefCount() const;

    /// Releases the underlying allocation if this is the last reference to it. After calling this, the Allocation object will be invalid.
    /// This function does not reset the Allocation object itself, it is safer to call reset() instead which will also mark the Allocation
    /// object as invalid.
    void release();

    /// Releases the underlying allocation if this is the last reference to it and marks this Allocation object as invalid.
    void reset();

    D3D12MA::Allocation* get() const;
    D3D12MA::Allocation* operator->() const;
    bool operator==(const Allocation& other) const;
    bool operator!=(const Allocation& other) const;
    operator bool() const;

    Allocation() = default;
    Allocation(const Allocation& other);
    Allocation& operator=(const Allocation& other);
    Allocation(Allocation&& other) noexcept;
    Allocation& operator=(Allocation&& other) noexcept;
    ~Allocation();

  private:
    Allocation(D3D12MA::Allocation* alloc, size_t count);

    D3D12MA::Allocation* allocation = nullptr;
    std::atomic_size_t* ref_count = nullptr;
  };
} // namespace kt::rhi