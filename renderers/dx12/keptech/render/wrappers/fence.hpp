#pragma once

#include <cstdint>
#include <wrl.h>

namespace kt::rdr {
  class Fence {
  public:
    Fence() = default;
    Fence(Microsoft::WRL::ComPtr<ID3D12Fence>&& fence);
    ~Fence();

    uint64_t signal(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue);
    void wait(uint64_t value, HANDLE fenceEvent);

    void flush(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue, HANDLE fenceEvent);

    Microsoft::WRL::ComPtr<ID3D12Fence>& operator*() { return m_fence; }

  private:
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue = 0;
  };
} // namespace kt::rdr