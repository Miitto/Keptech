#include "d3dx12.h"
#include "helpers/formatting.hpp"
#include "keptech/core/window.hpp"
#include "macros.hpp"
#include "rhi.hpp"
#include <expected>
#include <synchapi.h>

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace kt::rhi {
  namespace {
    void dx12DebugCallback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description,
                           void* context) {
      (void)context;
      switch (severity) {
      case D3D12_MESSAGE_SEVERITY_CORRUPTION:
        DX_CRITICAL("{} CORRUPTION: {} {}", category, id, description);
        break;
      case D3D12_MESSAGE_SEVERITY_ERROR:
        DX_ERROR("{}: {} {}", category, id, description);
        break;
      case D3D12_MESSAGE_SEVERITY_WARNING:
        DX_WARN("{}: {} {}", category, id, description);
        break;
      case D3D12_MESSAGE_SEVERITY_INFO:
        DX_INFO("{}: {} {}", category, id, description);
        break;
      case D3D12_MESSAGE_SEVERITY_MESSAGE:
        DX_DEBUG("{}: {} {}", category, id, description);
        break;
      }
    }
  } // namespace

  std::expected<void, std::string> RHI::init(const RendererCreateInfo& createInfo, const Window& window) {
    if (isInitialized) {
      return std::unexpected("Renderer is already initialized");
    }

    auto result = singleton.initInternal(createInfo, window);
    if (!result) {
      return std::unexpected(result.error());
    }

    isInitialized = true;
    return {};
  }

  std::expected<void, std::string> RHI::initInternal(const RendererCreateInfo& createInfo, const Window& window) {
    m.window = &window;

#ifndef NDEBUG
    ComPtr<ID3D12Debug> debugInterface;
    DX_MAKE(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)), "Failed to get D3D12 debug interface");
    debugInterface->EnableDebugLayer();
    DX_DEBUG("D3D12 debug layer enabled");
#endif

    ComPtr<IDXGIFactory4> dxgiFactory;
    UINT createFactoryFlags = 0;
#ifndef NDEBUG
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    DX_MAKE(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)), "Failed to create DXGI factory");

    DX_CREATE(adapter, getAdapter(createInfo, dxgiFactory), "Failed to get suitable adapter");

    {
      auto res = initDevice(createInfo, adapter);
      if (!res) {
        return std::unexpected(res.error());
      }
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };

    DX_MAKE(m.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m.queues.graphics)), "Failed to create graphics command queue");
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    DX_MAKE(m.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m.queues.compute)), "Failed to create compute command queue");
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    DX_MAKE(m.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m.queues.copy)), "Failed to create copy command queue");

    {
      auto res = initSwapchain(createInfo, window, dxgiFactory);
      if (!res) {
        return std::unexpected(res.error());
      }
    }

    {
      auto res = initCommandLists();
      if (!res) {
        return std::unexpected(res.error());
      }
    }

    {
      auto res = initImGui();
      if (!res) {
        return std::unexpected(res.error());
      }
    }

    DX_MAKE(m.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&*m.fence)), "Failed to create fence");
    m.fence.makeEvent();
    DX_MAKE(m.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&*m.copyFence)), "Failed to create copy fence");
    m.copyFence.makeEvent();

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = 300, // TODO: Make this configurable
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    DX_MAKE(m.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m.rtvHeap.heap)), "Failed to create RTV descriptor heap");

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = 100, // TODO: Make this configurable
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    DX_MAKE(m.device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m.dsvHeap.heap)), "Failed to create DSV descriptor heap");

    D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 1000, // TODO: Make this configurable
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0,
    };
    DX_MAKE(m.device->CreateDescriptorHeap(&cbvSrvUavHeapDesc, IID_PPV_ARGS(&m.cbvSrvUavHeap.heap)),
            "Failed to create CBV_SRV_UAV descriptor heap");

    D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        .NumDescriptors = 100, // TODO: Make this configurable
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0,
    };
    DX_MAKE(m.device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m.samplerHeap.heap)),
            "Failed to create sampler descriptor heap");

    D3D12_INDIRECT_ARGUMENT_DESC indirectArgumentDesc = {
        .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW,
    };
    D3D12_COMMAND_SIGNATURE_DESC drawIndirectSignatureDesc = {
        .ByteStride = sizeof(D3D12_DRAW_ARGUMENTS),
        .NumArgumentDescs = 1,
        .pArgumentDescs = &indirectArgumentDesc,
    };
    DX_MAKE(m.device->CreateCommandSignature(&drawIndirectSignatureDesc, nullptr, IID_PPV_ARGS(&m.drawIndirectSignature)),
            "Failed to create draw indirect command signature");
    D3D12_INDIRECT_ARGUMENT_DESC indexedIndirectArgumentDesc = {
        .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED,
    };
    D3D12_COMMAND_SIGNATURE_DESC drawIndexedIndirectSignatureDesc = {
        .ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
        .NumArgumentDescs = 1,
        .pArgumentDescs = &indexedIndirectArgumentDesc,
    };
    DX_MAKE(m.device->CreateCommandSignature(&drawIndexedIndirectSignatureDesc, nullptr, IID_PPV_ARGS(&m.drawIndexedIndirectSignature)),
            "Failed to create draw indexed indirect command signature");
    return {};
  }

  std::expected<ComPtr<IDXGIAdapter4>, std::string> RHI::getAdapter(const RendererCreateInfo&, const ComPtr<IDXGIFactory4>& dxgiFactory) {
    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    SIZE_T maxVideoMemory = 0;
    for (UINT adapterIndex = 0; dxgiFactory->EnumAdapters1(adapterIndex, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++adapterIndex) {
      DXGI_ADAPTER_DESC1 adapterDesc;
      dxgiAdapter1->GetDesc1(&adapterDesc);

      if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue; // Skip software adapters
      }

      if (FAILED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_12_2, _uuidof(ID3D12Device), nullptr))) {
        continue;
      }

      if (adapterDesc.DedicatedVideoMemory > maxVideoMemory) {
        maxVideoMemory = adapterDesc.DedicatedVideoMemory;
        DX_MAKE(dxgiAdapter1.As(&dxgiAdapter4), "Failed to query IDXGIAdapter4 interface");
      }
    }

    return dxgiAdapter4;
  }

  std::expected<void, std::string> RHI::initDevice(const RendererCreateInfo&, const ComPtr<IDXGIAdapter4>& dxgiAdapter4) {
    DX_MAKE(D3D12CreateDevice(dxgiAdapter4.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m.device)), "Failed to create D3D12 device");

#ifndef NDEBUG
    ComPtr<ID3D12InfoQueue1> infoQueue;
    if (SUCCEEDED(m.device.As(&infoQueue))) {
      std::array severities = {D3D12_MESSAGE_SEVERITY_INFO};

      // Suppress individual messages by their ID
      std::array denyIds = {
          D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, // Allow arbitrary clear values.
          // These 2 get triggered by a graphcics debugger
          D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
          D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
      };

      D3D12_INFO_QUEUE_FILTER newFilter = {};
      newFilter.DenyList.NumSeverities = severities.size();
      newFilter.DenyList.pSeverityList = severities.data();
      newFilter.DenyList.NumIDs = denyIds.size();
      newFilter.DenyList.pIDList = denyIds.data();

      DX_MAKE(infoQueue->PushStorageFilter(&newFilter), "Failed to push storage filter to info queue");

      DWORD cookie = 0;
      DX_MAKE(infoQueue->RegisterMessageCallback(&dx12DebugCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &cookie),
              "Failed to register message callback");
    }
#endif

    D3D12MA::ALLOCATOR_DESC allocatorDesc = {
        .pDevice = m.device.Get(),
        .pAdapter = dxgiAdapter4.Get(),
        .Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS,
    };
    DX_MAKE(D3D12MA::CreateAllocator(&allocatorDesc, &m.allocator), "Failed to create D3D12 memory allocator");

    RTV_DESCRIPTOR_SIZE = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    DSV_DESCRIPTOR_SIZE = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    CBV_SRV_UAV_DESCRIPTOR_SIZE = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(m.device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
      featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    ROOT_SIGNATURE_VERSION = featureData.HighestVersion;

    return {};
  }

  std::expected<void, std::string> RHI::initSwapchain(const RendererCreateInfo&, const Window& window,
                                                      const ComPtr<IDXGIFactory4>& dxgiFactory) {

    ComPtr<IDXGIFactory5> dxgiFactory5;
    BOOL tearingSupportB = FALSE;
    if (SUCCEEDED(dxgiFactory.As(&dxgiFactory5))) {
      if (FAILED(dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearingSupportB, sizeof(tearingSupportB)))) {
        tearingSupportB = FALSE;
      }
    }
    m.swapchain.tearingSupport = tearingSupportB == TRUE;

    m.swapchain.size = window.getRenderSize();

    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {.Width = m.swapchain.size.x,
                                           .Height = m.swapchain.size.y,
                                           .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                                           .Stereo = FALSE,
                                           .SampleDesc = {.Count = 1, .Quality = 0},
                                           .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                                           .BufferCount = SWAPCHAIN_IMAGE_COUNT,
                                           .Scaling = DXGI_SCALING_STRETCH,
                                           .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
                                           .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
                                           .Flags = m.swapchain.tearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u};

    SDL_PropertiesID props = SDL_GetWindowProperties(window.getHandle());
    HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL));

    if (!hwnd) {
      return std::unexpected("Failed to get HWND from SDL window");
    }

    ComPtr<IDXGISwapChain1> swapchain1;
    DX_MAKE(dxgiFactory->CreateSwapChainForHwnd(m.queues.graphics.Get(), hwnd, &swapchainDesc, nullptr, nullptr, &swapchain1),
            "Failed to create swapchain");

    DX_DEBUG("Swapchain created with size {}x{} and format {}", m.swapchain.size.x, m.swapchain.size.y, swapchainDesc.Format);

    DX_MAKE(dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER), "Failed to make window association");

    DX_MAKE(swapchain1.As(&m.swapchain.swapchain), "Failed to query IDXGISwapChain4 interface");

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = SWAPCHAIN_IMAGE_COUNT,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };

    DX_MAKE(m.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m.swapchain.rtvHeap)), "Failed to create RTV descriptor heap");

    auto res = updateBackbufferDescriptors();
    if (!res) {
      return std::unexpected(res.error());
    }

    return {};
  }

  std::expected<void, std::string> RHI::updateBackbufferDescriptors() {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (uint32_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; ++i) {
      DX_MAKE(m.swapchain.swapchain->GetBuffer(i, IID_PPV_ARGS(&m.swapchain.backbuffers[i])), "Failed to get swapchain buffer");
      m.device->CreateRenderTargetView(m.swapchain.backbuffers[i].Get(), nullptr, rtvHandle);
      rtvHandle.Offset(static_cast<INT>(RTV_DESCRIPTOR_SIZE));
    }

    return {};
  }

  std::expected<void, std::string> RHI::initCommandLists() {
    ComPtr<ID3D12CommandAllocator> cmdAlloc;
    DX_MAKE(m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&cmdAlloc)), "Failed to create command allocator");

    DX_MAKE(m.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&m.copyCmdList)),
            "Failed to create command list");

    m.copyCmdList->Close();

    return {};
  }
} // namespace kt::rhi