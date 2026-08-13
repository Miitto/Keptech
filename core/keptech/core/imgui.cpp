#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>

namespace kt::rendering {

  void initImGui() { ImGui::CreateContext(); }
  void shutdownImGui() {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
  void newImGuiFrame() {
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
  }

} // namespace kt::rendering
