#include "renderer.hpp"
#include "backends/imgui_impl_dx12.h"
#include "d3dx12.h"
#include "dx-logger.hpp"
#include "imgui.h"
#include "keptech/components/transform.hpp"
#include "keptech/core/version.h"
#include "keptech/maths/frustum.hpp"
#include "keptech/render/graph/builder.hpp"
#include "keptech/render/imgui.hpp"
#include "keptech/render/interface/renderer.hpp"
#include "wrappers/cmdBuf.hpp"
#include <d3d12.h>

namespace kt::rdr {
  static_assert(interface::IsRenderer<Renderer>, "Renderer does not satisfy the IsRenderer concept");

  Renderer Renderer::singleton{};
  bool Renderer::isInitialized = false;

  Renderer& Renderer::get() { return singleton; }
  bool Renderer::isInit() { return isInitialized; }

  bool Renderer::canRenderToFormat(ImageFormat format) const {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{.Format = format};
    m.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
    return (formatSupport.Support1 & (D3D12_FORMAT_SUPPORT1_RENDER_TARGET | D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)) != 0;
  }

  bool Renderer::canSampleFromFormat(ImageFormat format) const {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{.Format = format};
    m.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
    return (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0;
  }

  void Renderer::newFrame() {
    auto& frame = m.frames[m.frameIndex];
    auto& allocs = frame.commandAllocators;
    auto& gAlloc = allocs.graphics;
    auto& cAlloc = allocs.compute;

    gAlloc->Reset();
    cAlloc->Reset();

    m.commandLists.graphics->Reset(gAlloc.Get(), nullptr);
    m.commandLists.compute->Reset(cAlloc.Get(), nullptr);

    for (auto* alloc : frame.allocsToDrop) {
      alloc->Release();
    }
    frame.allocsToDrop.clear();

    ImGui_ImplDX12_NewFrame();
    rendering::newImGuiFrame();
  }

  maths::Frustum Renderer::startFrame() { return maths::Frustum{}; }

  void Renderer::debugUi() const {
    ImGui::Begin("Debug View");

    auto camera = Scene::active().getActiveCamera();
    auto& camT = camera.getComponents<components::Transform>();
    auto camPos = camT.getGlobal()[3];

    ImGui::Text("Camera Position: %.2f, %.2f, %.2f", static_cast<double>(camPos.x), static_cast<double>(camPos.y),
                static_cast<double>(camPos.z));

    ImGui::End();

    std::string ktInfo = fmt::format("KepTech v{} (DX12) {}x{}", KT_VERSION_STRING, m.swapchain.size.x, m.swapchain.size.y);

    auto viewportSize = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos({viewportSize.x, .0f}, ImGuiCond_Appearing, {1.0f, 1.0f});
    ImGui::SetNextWindowBgAlpha(0.f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    ImGui::Begin("KepTech Info", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav);
    ImGui::TextUnformatted(ktInfo.c_str());
    ImGui::End();
    ImGui::PopStyleVar();
  }

  void Renderer::endFrame(const CommandBuffer& cmd) {

    {
      CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
          m.swapchain.backbuffers[m.imageIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
      cmd->ResourceBarrier(1, &barrier);
    }

#ifndef NDEBUG
    debugUi();
#endif

    ImGui::Render();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart(), m.imageIndex,
                                            static_cast<UINT>(RTV_DESCRIPTOR_SIZE));

    std::array<FLOAT, 4> clearColor = {.0f, .2f, .4f, 1.0f};
    cmd->ClearRenderTargetView(rtvHandle, clearColor.data(), 0, nullptr);
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    cmd->SetDescriptorHeaps(1, m.imGui.srvHeap.GetAddressOf());

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m.swapchain.backbuffers[m.imageIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    cmd->ResourceBarrier(1, &barrier);

    cmd->Close();

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    m.queues.graphics->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    present();
  }

  void Renderer::present() {
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

  void Renderer::onResize() {
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

    m.imageIndex = static_cast<uint8_t>(m.swapchain.swapchain->GetCurrentBackBufferIndex());
    auto res = updateBackbufferDescriptors();
    if (!res) {
      DX_ABORT("Failed to update backbuffer descriptors: {}", res.error());
    }
  }

  Renderer::~Renderer() {
    m.fence.flush(m.queues.graphics);

    m.buffers.~Buffers();

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
    rendering::shutdownImGui();
  }

  void Renderer::setRenderGraphProps(RenderGraphBuilder& builder) const {
    DXGI_SWAP_CHAIN_DESC swapchainDesc{};
    DX_REQUIRE(SUCCEEDED(m.swapchain.swapchain->GetDesc(&swapchainDesc)), "Failed to get swapchain description");
    builder.setSwapchainFormat(m.formats.swapchain);
    builder.setSwapchainSize({swapchainDesc.BufferDesc.Width, swapchainDesc.BufferDesc.Height});

    auto d = SDL_GetDisplayForWindow(m.window->getHandle());
    auto* dm = SDL_GetCurrentDisplayMode(d);
    builder.setRenderResolution({dm->w, dm->h});
  }

  void Renderer::addGeometryPass(RenderGraphBuilder& builder, bool clearColor) {
    (void)clearColor;
    auto& p = builder.addPass("kt::geometry");
    p.addColorOutput("kt::albedo", {
                                       .sizeType = AttachmentSize::SwapchainRelative,
                                   });
  }

  void Renderer::resetCopyAllocator() {
    m.copyFence.flush(m.queues.copy);
    m.copyCommandAllocator->Reset();
    m.commandLists.copy->Reset(m.copyCommandAllocator.Get(), nullptr);
  }

  void Renderer::submitForDrop(D3D12MA::Allocation* allocation) { m.frames[m.frameIndex].allocsToDrop.push_back(allocation); }
} // namespace kt::rdr