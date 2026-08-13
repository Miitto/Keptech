#pragma once

#include <cstdint>
#include <wrl.h>

namespace kt::rhi {
  class Fence {
  public:
    Fence() = default;
    Fence(Microsoft::WRL::ComPtr<ID3D12Fence>&& fence);
    ~Fence();

    uint64_t getValue() const { return m_fenceValue; }

    uint64_t signal(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue);
    void signal(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue, uint64_t value);
    void wait(uint64_t value);

    void flush(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue);

    Microsoft::WRL::ComPtr<ID3D12Fence>& operator*();
    [[nodiscard]] HANDLE getEvent() const;

    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;
    Fence(Fence&& o) noexcept;
    Fence& operator=(Fence&& o) noexcept;

    operator ID3D12Fence*() const { return m_fence.Get(); }

    void makeEvent();

  private:
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue = 0;
    HANDLE event = nullptr;
  };
} // namespace kt::rhi