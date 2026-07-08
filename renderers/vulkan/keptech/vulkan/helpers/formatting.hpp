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
