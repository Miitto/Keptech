#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "d3dx12.h"
#include "dx/dx-logger.hpp"
#include "helpers/imGuiDescriptorAlloc.hpp"
#include "keptech/core/result.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/dx/constants.hpp"
#include "keptech/rhi/dx/fence.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/rendererCreateInfo.hpp"
#include <D3D12MemAlloc.h>
#include <array>
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
  class Buffer;

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

    ComPtr<ID3D12GraphicsCommandList4> copyCmdList;
    Fence copyFence;

    DxImGui imGui;

    DescriptorHeap rtvHeap;
    DescriptorHeap dsvHeap;
    DescriptorHeap cbvSrvUavHeap;
    DescriptorHeap samplerHeap;

    ComPtr<ID3D12CommandSignature> drawIndirectSignature;
    ComPtr<ID3D12CommandSignature> drawIndexedIndirectSignature;

    std::vector<rhi::Image> loadedTextures;

    struct OngoingCopy {
      ComPtr<ID3D12CommandAllocator> cmdAlloc;
      uint64_t fenceValue = 0;
      std::vector<D3D12MA::Allocation*> allocsToDrop;
    };
    std::vector<OngoingCopy> ongoingCopies;

    uint8_t frameIndex = 0;
    uint8_t imageIndex = 0;

    std::array<std::vector<ComPtr<ID3D12CommandAllocator>>, MAX_FRAMES_IN_FLIGHT> runningAllocs;
  };

  using VertexBufferView = D3D12_VERTEX_BUFFER_VIEW;
  class ImageCreateInfo;

  class RHI {
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

#include "keptech/rhi/interface/rhi.hpp"

  public:
    CD3DX12_CPU_DESCRIPTOR_HANDLE dxGetRtvHandle(uint16_t index) const;
    CD3DX12_CPU_DESCRIPTOR_HANDLE dxGetDsvHandle(uint16_t index) const;

    const Members& dxGetMembers() const { return m; }
    ComPtr<ID3D12Device2> dxGetDevice() const { return m.device; }
    D3D12MA::Allocator* dxGetAllocator() const { return m.allocator; }
    Members& dxGetMembers() { return m; }
    void dxRegisterRenderTargetImage(rhi::Image& image);
    void dxRegisterDepthStencilImage(rhi::Image& image);
    void dxUpdateRenderTargetImage(rhi::Image& image);
    void dxUpdateDepthStencilImage(rhi::Image& image);

  private:
    std::expected<void, std::string> initInternal(const RendererCreateInfo& createInfo, const Window& window);
    std::expected<ComPtr<IDXGIAdapter4>, std::string> getAdapter(const RendererCreateInfo& createInfo,
                                                                 const ComPtr<IDXGIFactory4>& dxgiFactory);
    std::expected<void, std::string> initDevice(const RendererCreateInfo& createInfo, const ComPtr<IDXGIAdapter4>& dxgiAdapter4);
    std::expected<void, std::string> initSwapchain(const RendererCreateInfo& createInfo, const Window& window,
                                                   const ComPtr<IDXGIFactory4>& dxgiFactory);
    std::expected<void, std::string> initCommandLists();
    std::expected<void, std::string> updateBackbufferDescriptors();
    std::expected<void, std::string> initImGui();

    Members m;
  };
} // namespace kt::rhi