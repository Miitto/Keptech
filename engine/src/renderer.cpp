#include "keptech/renderer.hpp"

#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>

namespace keptech {
  void Renderer::newFrame() {
    backend->newFrame();

    ImGui_ImplSDL3_NewFrame();

    ImGui::NewFrame();
  }

  void Renderer::initImGui() {
    ImGui::CreateContext();
    backend->initImGui();
  }

  Renderer::~Renderer() {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
} // namespace keptech
