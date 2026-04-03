#pragma once

namespace kt::rendering {

  /// Call before the renderer inits its own ImGui backend.
  void initImGui();
  /// Call after the renderer has shut down its own ImGui backend.
  void shutdownImGui();
  /// Call after the renderer has started its next ImGui frame, but before you
  /// start issuing ImGui commands.
  void newImGuiFrame();

} // namespace kt::rendering
