#pragma once

#include <spdlog/fmt/bundled/format.h>
#include <string_view>
#include <vulkan/vulkan.h>

template <> struct fmt::formatter<VkFormat> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(VkFormat format, fmt::format_context& ctx) const;
};
