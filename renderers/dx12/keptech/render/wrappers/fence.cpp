#include "fence.hpp"

#include "dx-logger.hpp"

namespace kt::rdr {
  Fence::Fence(Microsoft::WRL::ComPtr<ID3D12Fence>&& fence) : m_fence(std::move(fence)) {}

  Fence::~Fence() {
    // Destructor logic if needed
  }

  uint64_t Fence::signal(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue) {
    auto hr = commandQueue->Signal(m_fence.Get(), ++m_fenceValue);
    DX_ASSERT(SUCCEEDED(hr), "Failed to signal command queue");
    return m_fenceValue;
  }

  void Fence::wait(uint64_t value, HANDLE fenceEvent) {
    // Wait until the fence has been signaled
    if (m_fence->GetCompletedValue() < value) {
      auto hr = m_fence->SetEventOnCompletion(value, fenceEvent);
      DX_ASSERT(SUCCEEDED(hr), "Failed to set event on completion");
      uint8_t waitCount = 0;
      while (::WaitForSingleObject(fenceEvent, 1000) == WAIT_TIMEOUT) {
        ++waitCount;
        if (waitCount > 30) {
          DX_ABORT("Fence wait timed out after 30 seconds.");
        }

        std::this_thread::yield(); // Yield to other threads while waiting
      }
    }
  }

  void Fence::flush(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue, HANDLE fenceEvent) {
    uint64_t fenceValue = signal(commandQueue);
    wait(fenceValue, fenceEvent);
  }
} // namespace kt::rdr