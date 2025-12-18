#define VMA_IMPLEMENTATION

#include "vk-logger.hpp"
#include <spdlog/fmt/bundled/printf.h>
#include <string_view>

#define VMA_DEBUG_LOG_FORMAT(format, ...)                                      \
  vmaLog(fmt::sprintf(format, ##__VA_ARGS__));

#define VMA_LEAK_LOG_FORMAT(format, ...)                                       \
  vmaMemLeak(fmt::sprintf(format, ##__VA_ARGS__));

namespace {
  void vmaLog(std::string_view str) {
    VK_DEBUG("VMA: {}", str);
    // For break
  }
  void vmaMemLeak(std::string_view str) {
    VK_ERROR("VMA LEAK: {}", str);
    // For break
  }
} // namespace

#include "vk_mem_alloc.hpp"
