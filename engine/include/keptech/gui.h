#pragma once

#include <imgui/imgui.h>
#include <keptech/core/moveGuard.hpp>

namespace keptech::gui {
  class Frame {
  public:
    Frame(const char* const name, bool* p_open = nullptr,
          ImGuiWindowFlags flags = 0) {
      ImGui::Begin(name, p_open, flags);
    }

    template <typename... Args> const Frame& text(Args... args) const {
      ImGui::Text(std::forward<Args>(args)...);
      return *this;
    }

    bool button(const char* const label,
                const ImVec2& size = ImVec2(0, 0)) const {
      return ImGui::Button(label, size);
    }

    template <typename T>
      requires std::is_integral_v<T>
    consteval static inline ImGuiDataType getImGuiDataType() {
      if constexpr (std::is_same_v<T, uint8_t>) {
        return ImGuiDataType_U8;
      } else if constexpr (std::is_same_v<T, int8_t>) {
        return ImGuiDataType_S8;
      } else if constexpr (std::is_same_v<T, uint16_t>) {
        return ImGuiDataType_U16;
      } else if constexpr (std::is_same_v<T, int16_t>) {
        return ImGuiDataType_S16;
      } else if constexpr (std::is_same_v<T, uint32_t>) {
        return ImGuiDataType_U32;
      } else if constexpr (std::is_same_v<T, int32_t>) {
        return ImGuiDataType_S32;
      } else if constexpr (std::is_same_v<T, uint64_t>) {
        return ImGuiDataType_U64;
      } else if constexpr (std::is_same_v<T, int64_t>) {
        return ImGuiDataType_S64;
      } else {
        static_assert(sizeof(T) == 0,
                      "Unsupported integral type for getImGuiDataType");
      }
    }

    template <typename T, typename S = T, typename SF = T>
      requires std::is_integral_v<T>
    bool input(const char* const label, T& value, const S step = 1,
               const SF step_fast = 10, ImGuiInputTextFlags flags = 0) const {
      T step_converted = static_cast<T>(step);
      T step_fast_converted = static_cast<T>(step_fast);
      return ImGui::InputScalar(label, getImGuiDataType<T>(), &value,
                                step_converted == 0 ? nullptr : &step_converted,
                                step_fast_converted == 0 ? nullptr
                                                         : &step_fast_converted,
                                nullptr, flags);
    }

    bool inputInt(const char* const label, int& value, int step = 1,
                  int step_fast = 100, ImGuiInputTextFlags flags = 0) const {
      return ImGui::InputInt(label, &value, step, step_fast, flags);
    }

    bool inputFloat(const char* const label, float& value,
                    const float step = 0.0f, const float step_fast = 0.0f,
                    const char* const format = "%.3f",
                    ImGuiInputTextFlags flags = 0) const {
      return ImGui::InputFloat(label, &value, step, step_fast, format, flags);
    }

    bool inputText(const char* const label, char* buf, size_t buf_size,
                   ImGuiInputTextFlags flags = 0) const {
      return ImGui::InputText(label, buf, buf_size, flags);
    }

    bool inputScalar(const char* const label, ImGuiDataType data_type,
                     void* data, const void* step = nullptr,
                     const void* step_fast = nullptr,
                     const char* const format = nullptr,
                     ImGuiInputTextFlags flags = 0) const {
      return ImGui::InputScalar(label, data_type, data, step, step_fast, format,
                                flags);
    }

    const Frame& sameLine(float offset_from_start_x = 0.0f,
                          float spacing = -1.0f) const {
      ImGui::SameLine(offset_from_start_x, spacing);
      return *this;
    }

    const Frame& width(float w) const {
      ImGui::SetNextItemWidth(w);
      return *this;
    }

    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    Frame(Frame&&) = default;
    Frame& operator=(Frame&&) = default;
    ~Frame() {
      if (!moveGuard.moved())
        ImGui::End();
    }

  private:
    core::MoveGuard moveGuard;
  };
} // namespace keptech::gui
