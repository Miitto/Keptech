#pragma once

#include <Volk/volk.h>
#include <spdlog/fmt/bundled/format.h>
#include <string_view>

template <> struct fmt::formatter<VkResult> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(VkResult format, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<VkFormat> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(VkFormat format, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<VkPresentModeKHR> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(VkPresentModeKHR mode, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<VkImageLayout> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(VkImageLayout layout, fmt::format_context& ctx) const;
};

namespace kt::rdr {
  struct VkPipelineStageFlags2Formatter {
    VkPipelineStageFlags2 flags;
  };

  struct VkAccessFlags2Formatter {
    VkAccessFlags2 flags;
  };
} // namespace kt::rdr

template <> struct fmt::formatter<kt::rdr::VkPipelineStageFlags2Formatter> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(kt::rdr::VkPipelineStageFlags2Formatter flags, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<kt::rdr::VkAccessFlags2Formatter> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(kt::rdr::VkAccessFlags2Formatter flags, fmt::format_context& ctx) const;
};