#include "rhi.hpp"
#include "backends/imgui_impl_dx12.h"
#include "d3dx12.h"
#include "dx-logger.hpp"
#include "imgui.h"
#include "keptech/components/transform.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/core/version.h"
#include "keptech/core/window.hpp"
#include "keptech/rhi/imgui.hpp"
#include "wrappers/cmdBuf.hpp"
#include "wrappers/image.hpp"
#include "wrappers/imageRef.hpp"
#include <d3d12.h>
#include <utility>

namespace kt::rhi {

  RHI RHI::singleton{};
  bool RHI::isInitialized = false;

  RHI& RHI::get() { return singleton; }
  bool RHI::isInit() { return isInitialized; }

  bool RHI::canRenderToFormat(ImageFormat format) const {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{.Format = raw(format)};
    m.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
    return (formatSupport.Support1 & (D3D12_FORMAT_SUPPORT1_RENDER_TARGET | D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)) != 0;
  }

  bool RHI::canSampleFromFormat(ImageFormat format) const {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{.Format = raw(format)};
    m.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
    return (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0;
  }
  ImageFormat RHI::getSwapchainFormat() const { return static_cast<ImageFormat>(ImageFormat::R8G8B8A8_UNORM); }

  void RHI::newFrame() {
    auto& frame = m.frames[m.frameIndex];

    for (auto* alloc : frame.allocsToDrop) {
      alloc->Release();
    }
    frame.allocsToDrop.clear();

    m.runningAllocs[m.frameIndex].clear();

    ImGui_ImplDX12_NewFrame();
    imgui::newFrame();
  }

  void RHI::startFrame() {}

  void RHI::debugUi() const {

    auto camera = Scene::active().getActiveCamera();
    if (camera.isValid()) {
      ImGui::Begin("Debug View");
      auto& camT = camera.getComponents<components::Transform>();
      auto camPos = camT.getGlobal()[3];

      ImGui::Text("Camera Position: %.2f, %.2f, %.2f", static_cast<double>(camPos.x), static_cast<double>(camPos.y),
                  static_cast<double>(camPos.z));

      ImGui::End();
    }

    std::string ktInfo = fmt::format("KepTech v{} (DX12) {}x{}", KT_VERSION_STRING, m.swapchain.size.x, m.swapchain.size.y);

    auto viewportSize = ImGui::GetMainViewport()->Size;
    // Needs to be always because of window resize
    ImGui::SetNextWindowPos({viewportSize.x, .0f}, ImGuiCond_None, {1.0f, 0.f});
    ImGui::SetNextWindowBgAlpha(0.f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2.f, 2.f});

    ImGui::Begin("KepTech Info", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav);
    ImGui::TextUnformatted(ktInfo.c_str());
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  void RHI::endFrame(CommandBuffer& cmd) {
#ifndef NDEBUG
    debugUi();
#endif

    ImGui::Render();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart(), m.imageIndex,
                                            static_cast<UINT>(RTV_DESCRIPTOR_SIZE));

    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    cmd->SetDescriptorHeaps(1, m.imGui.srvHeap.GetAddressOf());

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m.swapchain.backbuffers[m.imageIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    cmd->ResourceBarrier(1, &barrier);

    cmd->Close();

    m.queues.graphics->Wait(m.fence, m.fence.getValue());

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    m.queues.graphics->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    m.runningAllocs[m.frameIndex].push_back(std::move(cmd.dxGetAlloc()));

    present();
  }

  void RHI::present() {
    UINT syncInterval = m.swapchain.vSync ? 1 : 0;
    UINT presentFlags = m.swapchain.vSync ? 0 : DXGI_PRESENT_ALLOW_TEARING;

    auto presentRes = m.swapchain.swapchain->Present(syncInterval, presentFlags);

    switch (presentRes) {
    case S_OK:
      break;
    case DXGI_ERROR_DEVICE_RESET:
      DX_ABORT("Device reset during present of swapchain image {} on frame index {}", m.imageIndex, m.frameIndex);
      break;
    case DXGI_ERROR_DEVICE_REMOVED:
      DX_ABORT("Device lost during present of swapchain image {} on frame index {}", m.imageIndex, m.frameIndex);
      break;
    case DXGI_STATUS_OCCLUDED:
      DX_DEBUG("Window occluded during present of swapchain image {} on frame index {}", m.imageIndex, m.frameIndex);
      break;
    default:
      DX_WARN("Unknown error during present of swapchain image {} on frame index {}: {}", m.imageIndex, m.frameIndex, presentRes);
      break;
    }

    m.frames[m.frameIndex].fenceValue = m.fence.signal(m.queues.graphics);

    m.frameIndex = (m.frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    m.fence.wait(m.frames[m.frameIndex].fenceValue);

    m.imageIndex = static_cast<uint8_t>(m.swapchain.swapchain->GetCurrentBackBufferIndex());
  }

  void RHI::onResize() {
    glm::uvec2 renderSize = m.window->getRenderSize();
    DX_DEBUG("Window resized to {}x{}", renderSize.x, renderSize.y);
    m.fence.flush(m.queues.graphics);

    for (uint32_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; ++i) {
      m.swapchain.backbuffers[i].Reset();
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m.frames[i].fenceValue = m.frames[m.frameIndex].fenceValue;
    }

    DXGI_SWAP_CHAIN_DESC swapchainDesc{};
    DX_REQUIRE(SUCCEEDED(m.swapchain.swapchain->GetDesc(&swapchainDesc)), "Failed to get swapchain description");
    DX_REQUIRE(SUCCEEDED(m.swapchain.swapchain->ResizeBuffers(SWAPCHAIN_IMAGE_COUNT, renderSize.x, renderSize.y,
                                                              swapchainDesc.BufferDesc.Format, swapchainDesc.Flags)),
               "Failed to resize swapchain buffers");

    m.swapchain.size = renderSize;

    m.imageIndex = static_cast<uint8_t>(m.swapchain.swapchain->GetCurrentBackBufferIndex());
    auto res = updateBackbufferDescriptors();
    if (!res) {
      DX_ABORT("Failed to update backbuffer descriptors: {}", res.error());
    }
  }

  RHI::~RHI() {
    m.fence.flush(m.queues.graphics);

    for (auto& frame : m.frames) {
      for (auto* alloc : frame.allocsToDrop) {
        alloc->Release();
      }
    }

    if (m.allocator) {
      m.allocator->Release();
      m.allocator = nullptr;
    }

    ImGui_ImplDX12_Shutdown();
    imgui::shutdown();
  }

  ImageRef RHI::getSwapchainImage() const {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart(), m.imageIndex,
                                            static_cast<UINT>(RTV_DESCRIPTOR_SIZE));
    return {"Swapchain Image", m.swapchain.backbuffers[m.imageIndex].Get(), rtvHandle};
  }

  CD3DX12_CPU_DESCRIPTOR_HANDLE RHI::dxGetRtvHandle(uint16_t index) const {
    if (index >= m.rtvHeap.count) {
      DX_ABORT("Invalid RTV index {}. Max count is {}", index, m.rtvHeap.count);
    }
    return {m.rtvHeap.heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index), static_cast<UINT>(RTV_DESCRIPTOR_SIZE)};
  }

  CD3DX12_CPU_DESCRIPTOR_HANDLE RHI::dxGetDsvHandle(uint16_t index) const {
    if (index >= m.dsvHeap.count) {
      DX_ABORT("Invalid DSV index {}. Max count is {}", index, m.dsvHeap.count);
    }
    return {m.dsvHeap.heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index), static_cast<UINT>(DSV_DESCRIPTOR_SIZE)};
  }

  void RHI::dxRegisterRenderTargetImage(rhi::Image& image) {
    if (image.dxGetRtvDsvIndex() != 65535) {
      DX_WARN("Image {} is already registered as a render target", image.getName());
      return;
    }

    image.dxSetRtvDsvIndex(m.rtvHeap.count++);

    dxUpdateRenderTargetImage(image);
  }

  void RHI::dxRegisterDepthStencilImage(rhi::Image& image) {
    if (image.dxGetRtvDsvIndex() != 65535) {
      DX_WARN("Image {} is already registered as a depth stencil", image.getName());
      return;
    }

    image.dxSetRtvDsvIndex(m.dsvHeap.count++);
    dxUpdateDepthStencilImage(image);
  }

  void RHI::dxUpdateRenderTargetImage(rhi::Image& image) {
    if (image.dxGetRtvDsvIndex() == 65535) {
      DX_WARN("Image {} is not registered as a render target", image.getName());
      return;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m.rtvHeap.heap->GetCPUDescriptorHandleForHeapStart(),
                                            static_cast<INT>(image.dxGetRtvDsvIndex()), static_cast<UINT>(RTV_DESCRIPTOR_SIZE));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = raw(image.format());
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    m.device->CreateRenderTargetView(image.dxresource().Get(), &rtvDesc, rtvHandle);
  }

  void RHI::dxUpdateDepthStencilImage(rhi::Image& image) {
    if (image.dxGetRtvDsvIndex() == 65535) {
      DX_WARN("Image {} is not registered as a depth stencil", image.getName());
      return;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m.dsvHeap.heap->GetCPUDescriptorHandleForHeapStart(),
                                            static_cast<INT>(image.dxGetRtvDsvIndex()), static_cast<UINT>(DSV_DESCRIPTOR_SIZE));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = raw(image.format());
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.Texture2D.MipSlice = 0;

    m.device->CreateDepthStencilView(image.dxresource().Get(), &dsvDesc, dsvHandle);
  }

  void RHI::submitGraphicsCmd(CommandBuffer& cmd, uint64_t waitFor, uint64_t signalTo) {
    m.queues.graphics->Wait(m.fence, waitFor);

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    m.queues.graphics->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    if (signalTo > 0) {
      m.fence.signal(m.queues.graphics, signalTo);
    }

    m.runningAllocs[m.frameIndex].push_back(std::move(cmd.dxGetAlloc()));
  }

  void RHI::submitComputeCmd(CommandBuffer& cmd, uint64_t waitFor, uint64_t signalTo) {
    m.queues.compute->Wait(m.fence, waitFor);

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    m.queues.compute->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    if (signalTo > 0) {
      m.fence.signal(m.queues.compute, signalTo);
    }

    m.runningAllocs[m.frameIndex].push_back(std::move(cmd.dxGetAlloc()));
  }

  std::vector<CommandBuffer> RHI::allocateGraphicsCommandBuffers(uint32_t count) {
    std::vector<CommandBuffer> cmdBuffers;
    cmdBuffers.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
      ComPtr<ID3D12CommandAllocator> cmdAlloc;
      DX_REQUIRE(SUCCEEDED(m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc))),
                 "Failed to create graphics command allocator");

      ComPtr<ID3D12GraphicsCommandList> cmdList;
      DX_REQUIRE(SUCCEEDED(m.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList))),
                 "Failed to create graphics command list");
      cmdBuffers.emplace_back(std::move(cmdAlloc), std::move(cmdList));
    }
    return cmdBuffers;
  }

  std::vector<CommandBuffer> RHI::allocateComputeCommandBuffers(uint32_t count) {
    std::vector<CommandBuffer> cmdBuffers;
    cmdBuffers.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
      ComPtr<ID3D12CommandAllocator> cmdAlloc;
      DX_REQUIRE(SUCCEEDED(m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&cmdAlloc))),
                 "Failed to create compute command allocator");

      ComPtr<ID3D12GraphicsCommandList> cmdList;
      DX_REQUIRE(
          SUCCEEDED(m.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList))),
          "Failed to create compute command list");
      cmdBuffers.emplace_back(std::move(cmdAlloc), std::move(cmdList));
    }
    return cmdBuffers;
  }
} // namespace kt::rhi