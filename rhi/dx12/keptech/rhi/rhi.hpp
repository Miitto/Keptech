#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "d3dx12.h"
#include "helpers/imGuiDescriptorAlloc.hpp"
#include "keptech/rhi/constants.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/rendererCreateInfo.hpp"
#include "keptech/rhi/wrappers/cmdBuf.hpp"
#include "keptech/rhi/wrappers/fence.hpp"
#include <D3D12MemAlloc.h>
#include <expected>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace kt {
  class Window;
}

namespace kt::rhi {
  template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  class Image;

  struct DxImGui {
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    ImGuiDescriptorHeapAllocator descriptorAlloc;
  };

  struct Swapchain {
    ComPtr<IDXGISwapChain4> swapchain;
    std::array<ComPtr<ID3D12Resource>, SWAPCHAIN_IMAGE_COUNT> backbuffers;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    bool tearingSupport = false;
    bool vSync = true;
    glm::uvec2 size{0, 0};
  };

  struct Frame {
    uint64_t fenceValue = 0;
    std::vector<D3D12MA::Allocation*> allocsToDrop;
  };

  struct Queues {
    ComPtr<ID3D12CommandQueue> graphics;
    ComPtr<ID3D12CommandQueue> compute;
    ComPtr<ID3D12CommandQueue> copy;
  };

  struct DescriptorHeap {
    ComPtr<ID3D12DescriptorHeap> heap;
    uint16_t count = 0;
  };

  struct Members {
    const Window* window = nullptr;

    ComPtr<ID3D12Device2> device;
    D3D12MA::Allocator* allocator = nullptr;

    Queues queues;

    Swapchain swapchain;

    std::array<Frame, MAX_FRAMES_IN_FLIGHT> frames;

    Fence fence;

    ComPtr<ID3D12CommandAllocator> copyCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> copyCmdList;
    Fence copyFence;

    DxImGui imGui;

    DescriptorHeap rtvHeap;
    DescriptorHeap dsvHeap;

    uint8_t frameIndex = 0;
    uint8_t imageIndex = 0;

    std::array<std::vector<ComPtr<ID3D12CommandAllocator>>, MAX_FRAMES_IN_FLIGHT> runningAllocs;
  };

  class RHI {
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  public:
    static RHI& get();
    const Members& getMembers() const { return m; }
    ComPtr<ID3D12Device2> getDevice() const { return m.device; }
    D3D12MA::Allocator* getAllocator() const { return m.allocator; }
    Members& getMembers() { return m; }
    uint8_t getFrameIndex() const { return m.frameIndex; }
    uint8_t getLastFrameIndex() const { return (m.frameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT; }

    bool canRenderToFormat(ImageFormat format) const;
    bool canSampleFromFormat(ImageFormat format) const;

    ImageFormat getSwapchainFormat() const;
    ImageRef getSwapchainImage() const;

    uint64_t getTimelineValue() const { return m.fence.getValue(); }

    void submitGraphicsCmd(CommandBuffer& cmd, uint64_t waitFor = 0, uint64_t signalTo = 0);
    void submitComputeCmd(CommandBuffer& cmd, uint64_t waitFor = 0, uint64_t signalTo = 0);

    std::vector<CommandBuffer> allocateGraphicsCommandBuffers(uint32_t count);
    std::vector<CommandBuffer> allocateComputeCommandBuffers(uint32_t count);

    void waitGraphicsIdle() { m.fence.flush(m.queues.graphics); }
    void waitComputeIdle() { m.fence.flush(m.queues.compute); }
    void waitCopyIdle() { m.copyFence.flush(m.queues.copy); }
    void waitIdle() {
      waitGraphicsIdle();
      waitComputeIdle();
      waitCopyIdle();
    }

    // Called internally, don't use
    void newFrame();
    void startFrame();
    void endFrame(CommandBuffer& cmdBuf);

    static bool isInit();
    std::expected<void, std::string> static init(const RendererCreateInfo& createInfo, const Window& window);

    void onResize();

    RHI(const RHI&) = delete;
    RHI& operator=(const RHI&) = delete;
    RHI(RHI&&) = delete;
    RHI& operator=(RHI&&) = delete;

    ~RHI();

    CD3DX12_CPU_DESCRIPTOR_HANDLE dxGetRtvHandle(uint16_t index) const;
    CD3DX12_CPU_DESCRIPTOR_HANDLE dxGetDsvHandle(uint16_t index) const;

    void dxRegisterRenderTargetImage(rhi::Image& image);
    void dxRegisterDepthStencilImage(rhi::Image& image);
    void dxUpdateRenderTargetImage(rhi::Image& image);
    void dxUpdateDepthStencilImage(rhi::Image& image);

  private:
    void present();

    void debugUi() const;

    RHI() = default;

    std::expected<void, std::string> initInternal(const RendererCreateInfo& createInfo, const Window& window);
    std::expected<ComPtr<IDXGIAdapter4>, std::string> getAdapter(const RendererCreateInfo& createInfo,
                                                                 const ComPtr<IDXGIFactory4>& dxgiFactory);
    std::expected<void, std::string> initDevice(const RendererCreateInfo& createInfo, const ComPtr<IDXGIAdapter4>& dxgiAdapter4);
    std::expected<void, std::string> initSwapchain(const RendererCreateInfo& createInfo, const Window& window,
                                                   const ComPtr<IDXGIFactory4>& dxgiFactory);
    std::expected<void, std::string> initCommandLists();
    std::expected<void, std::string> updateBackbufferDescriptors();
    std::expected<void, std::string> initImGui();

    static RHI singleton;
    static bool isInitialized;
    Members m;
  };
} // namespace kt::rhi