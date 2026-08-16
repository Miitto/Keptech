#include "fence.hpp"

#include "dx-logger.hpp"

namespace kt::rhi {
  Fence::Fence(Microsoft::WRL::ComPtr<ID3D12Fence>&& fence)
      : m_fence(std::move(fence)), event(::CreateEvent(nullptr, FALSE, FALSE, nullptr)) {}

  void Fence::makeEvent() {
    if (!event) {
      event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }
  }

  Fence::~Fence() {
    if (event) {
      ::CloseHandle(event);
    }
  }

  uint64_t Fence::currentValue() const { return m_fence->GetCompletedValue(); }

  uint64_t Fence::signal(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue) {
    auto hr = commandQueue->Signal(m_fence.Get(), ++m_fenceValue);
    DX_ASSERT(SUCCEEDED(hr), "Failed to signal command queue");
    return m_fenceValue;
  }

  void Fence::signal(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue, uint64_t value) {
    auto hr = commandQueue->Signal(m_fence.Get(), value);
    DX_ASSERT(SUCCEEDED(hr), "Failed to signal command queue");
    m_fenceValue = value;
  }

  void Fence::wait(uint64_t value) {
    // Wait until the fence has been signaled
    if (m_fence->GetCompletedValue() < value) {
      auto hr = m_fence->SetEventOnCompletion(value, event);
      DX_ASSERT(SUCCEEDED(hr), "Failed to set event on completion");
      uint8_t waitCount = 0;
      while (::WaitForSingleObject(event, 1000) == WAIT_TIMEOUT) {
        ++waitCount;
        if (waitCount > 30) {
          DX_ABORT("Fence wait timed out after 30 seconds.");
        }

        std::this_thread::yield(); // Yield to other threads while waiting
      }
    }
  }

  void Fence::flush(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue) {
    uint64_t fenceValue = signal(commandQueue);
    wait(fenceValue);
  }
  Microsoft::WRL::ComPtr<ID3D12Fence>& Fence::operator*() { return m_fence; }
  [[nodiscard]] HANDLE Fence::getEvent() const { return event; }

  Fence::Fence(Fence&& o) noexcept : m_fence(std::move(o.m_fence)), m_fenceValue(o.m_fenceValue), event(o.event) { o.event = nullptr; }
  Fence& Fence::operator=(Fence&& o) noexcept {
    if (this != &o) {
      m_fence = std::move(o.m_fence);
      m_fenceValue = o.m_fenceValue;
      event = o.event;
      o.event = nullptr;
    }
    return *this;
  };
} // namespace kt::rhi