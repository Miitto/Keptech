#include "keptech/core/gui.h"

namespace keptech::gui {
  std::vector<const char*> Frame::framesToVoid{};

  namespace {
    // https://github.com/ocornut/imgui/issues/8360

    bool MakeWindowVoidInputPassthroughTestMouse(ImGuiWindow* window) {
      ImGuiContext& g = *GImGui;
      if (ImGui::IsAnyItemHovered())
        return false;

      // Any active item is assumed to take inputs unless g.ActiveIdAllowOverlap
      // is set (the variable is historically a bit misnamed, but it allows e.g.
      // InputText() to be active while allowing hovering other items)
      if (ImGui::IsAnyItemActive() && !g.ActiveIdAllowOverlap) {
        // Unless we're clicking/dragging in the window's void itself
        // When clicking on window's void we allow it to take the ActiveId.
        if (g.ActiveId != window->MoveId)
          return false;
      } else {
        if (g.HoveredWindow && g.HoveredWindow->RootWindow != window)
          return false;
      }

      // If a popup is open it eats the click on void
      // FIXME: This could be an optional thing?
      // (low-level input handler may still distinguish io.WantCaptureMouse from
      // ioWantCaptureMouseUnlessPopupClose if they need to)
      if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
        return false;

      return true;
    }

    bool MakeWindowVoidInputPassthroughTestKeyboard(ImGuiWindow* window) {
      ImGuiContext& g = *GImGui;

      if (g.NavWindow == nullptr ||
          g.NavWindow->RootWindow != window->RootWindow)
        return false;

      // If any item is active in the window (e.g. an InputText, or a Button) we
      // keep keyboard to imgui
      if (g.ActiveId != 0 && g.ActiveId != window->MoveId)
        if (g.ActiveIdWindow->RootWindow == window->RootWindow)
          return false;

      // Allow navigation to work, tho it is likely you'd want to use
      // ImGuiWindowFlags_NoNav on the window.
      if (g.NavCursorVisible && g.NavId != 0)
        return false;

      // When navigation cursor is cleared, we disable nav on this window for
      // the frame, preventing e.g. arrow keys from resuming navigation while
      // underlying game/app is using them. Next frame's Begin() will clear the
      // flag again. This means navigation on this window may only be activated
      // via:
      // - Ctrl+Tabbing into the window.
      // - Using arrow key while an item is currently active (typically
      // InputText, but it will also
      //   work while holding mouse button over any item even tho that's a low
      //   affordance behavior).
      // - And pressing Escape will deactivate navigation and relinquish
      // keyboard underlying game/app. This seems like the best design as we
      // want e.g. clicking a button to end up stealing keyboard.
      window->Flags |= ImGuiWindowFlags_NoNav;

      return true;
    }

    void MakeWindowVoidInputPassthrough(const char* name) {
      ImGuiIO& io = ImGui::GetIO();
      ImGuiWindow* window = ImGui::FindWindowByName(name);
      if (window == nullptr)
        return;

      if (MakeWindowVoidInputPassthroughTestMouse(window)) {
        // Write to io.WantCaptureMouse directly, so it is available in e.g.
        // low-level input handler _before_ the next NewFrame(). Technically we
        // don't need to call SetNextFrameWantCaptureMouse(): what matter is
        // that io.WantCaptureMouse is cleared at the time of low-level input
        // handlers applying their filter.
        io.WantCaptureMouse = false;
        ImGui::SetNextFrameWantCaptureMouse(false);
      }

      if (MakeWindowVoidInputPassthroughTestKeyboard(window)) {
        io.WantCaptureKeyboard = false;
        ImGui::SetNextFrameWantCaptureKeyboard(false);
      }
    }
  } // namespace

  void Frame::processInputPassthrough() {
    for (const char* frameName : framesToVoid) {
      MakeWindowVoidInputPassthrough(frameName);
    }
    framesToVoid.clear();
  }
} // namespace keptech::gui
