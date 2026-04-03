#include "keptech/vulkan/renderer.hpp"

#include "keptech/vulkan/structs.hpp"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/components/camera.hpp>
#include <keptech/core/window.hpp>

namespace kt::vkh {
  bool Renderer::canRenderToFormat(VkFormat format) const {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(m.vkcore.device.physical, format, &formatProps);
    return ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT) |
            (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0;
  }

  std::expected<gltf::Scene, std::string> Renderer::loadMesh(std::string_view path) {
    // TODO: Impl
    return gltf::Scene{};
  }

  std::expected<std::vector<Texture>, std::string> Renderer::createImages(const std::vector<ImageUploadInfo>& infos) {
    // TODO: Impl
    return {};
  }
} // namespace kt::vkh
