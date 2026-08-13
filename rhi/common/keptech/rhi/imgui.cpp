#include "imgui.hpp"

#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>

namespace kt::imgui {
  void init() { ImGui::CreateContext(); }

  void newFrame() {
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
  }

  void shutdown() {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
} // namespace kt::imgui