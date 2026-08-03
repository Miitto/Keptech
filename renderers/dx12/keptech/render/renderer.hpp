#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "helpers/imGuiDescriptorAlloc.hpp"
#include "keptech/render/constants.hpp"
#include "keptech/render/formats.hpp"
#include "keptech/render/gltf/scene.hpp"
#include "keptech/render/rendererCreateInfo.hpp"
#include "keptech/render/wrappers/fence.hpp"
#include <D3D12MemAlloc.h>
#include <expected>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace kt {
  namespace maths {
    struct Frustum;
  }
} // namespace kt

namespace kt::rdr {
  class RenderGraphBuilder;
  class CommandBuffer;

  struct Members {
    const Window* window = nullptr;
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<ID3D12Device2> device;
    D3D12MA::Allocator* allocator = nullptr;
    ComPtr<ID3D12CommandQueue> graphicsQueue;
    ComPtr<IDXGISwapChain4> swapchain;
    std::array<ComPtr<ID3D12Resource>, SWAPCHAIN_IMAGE_COUNT> backbuffers;
    ComPtr<ID3D12GraphicsCommandList> graphicsCmdList;
    std::array<ComPtr<ID3D12CommandAllocator>, MAX_FRAMES_IN_FLIGHT> graphicsCmdAlloc;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    Fence fence;

    ComPtr<ID3D12DescriptorHeap> imGuiSrvHeap;
    ImGuiDescriptorHeapAllocator imGuiDescriptorAlloc;

    std::array<uint64_t, MAX_FRAMES_IN_FLIGHT> fenceValues{};
    uint64_t fenceValue = 0;
    HANDLE fenceEvent = nullptr;

    uint8_t frameIndex = 0;
    uint8_t imageIndex = 0;

    Formats formats{};

    bool tearingSupport = false;
    bool vSync = true;
  };

  class Renderer {
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  public:
    static Renderer& get();
    const Members& getMembers() const { return m; }
    Members& getMembers() { return m; }
    uint8_t getFrameIndex() const { return m.frameIndex; }
    uint8_t getLastFrameIndex() const { return (m.frameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT; }

    void addGeometryPass(RenderGraphBuilder& graph, bool clearColor = false);

    const Formats& getFormats() const { return m.formats; }

    /// Loads a glTF mesh from the specified path. Returns a gltf::Scene on success, or an error message on failure.
    std::expected<gltf::Scene, std::string> loadMesh(std::string_view path);

    void waitIdle() { m.fence.flush(m.graphicsQueue, m.fenceEvent); }

    // Called internally, don't use
    void newFrame();
    maths::Frustum startFrame();
    void endFrame(const CommandBuffer& cmd);

    static bool isInit();
    std::expected<void, std::string> static init(const RendererCreateInfo& createInfo, const Window& window);

    void setRenderGraphProps(RenderGraphBuilder& builder) const;
    void onResize();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    ~Renderer();

  private:
    void present();

    void debugUi() const;

    Renderer() = default;

    std::expected<void, std::string> initInternal(const RendererCreateInfo& createInfo, const Window& window);
    std::expected<ComPtr<IDXGIAdapter4>, std::string> getAdapter(const RendererCreateInfo& createInfo,
                                                                 const ComPtr<IDXGIFactory4>& dxgiFactory);
    std::expected<void, std::string> initDevice(const RendererCreateInfo& createInfo, const ComPtr<IDXGIAdapter4>& dxgiAdapter4);
    std::expected<void, std::string> initSwapchain(const RendererCreateInfo& createInfo, const Window& window,
                                                   const ComPtr<IDXGIFactory4>& dxgiFactory);
    std::expected<void, std::string> initCommandLists();
    std::expected<void, std::string> updateBackbufferDescriptors();
    std::expected<void, std::string> initImGui();

    static Renderer singleton;
    static bool isInitialized;
    Members m;
  };
} // namespace kt::rdr