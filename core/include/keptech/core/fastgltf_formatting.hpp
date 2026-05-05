#pragma once

#include <fastgltf/core.hpp>
#include <spdlog/fmt/bundled/format.h>
#include <string_view>

template <> struct fmt::formatter<fastgltf::Error> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const fastgltf::Error& error, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<fastgltf::AccessorType> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const fastgltf::AccessorType& type, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<fastgltf::ComponentType> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const fastgltf::ComponentType& type, fmt::format_context& ctx) const;
};
