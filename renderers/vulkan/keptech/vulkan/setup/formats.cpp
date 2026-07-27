#include "setup.hpp"

#include "keptech/vulkan/structs.hpp"
#include "renderer.hpp"
#include "vk-logger.hpp"
#include <Volk/volk.h>
#include <keptech/shaders/shader.h>

namespace kt::vkh {
  // Formats
  namespace {
    constexpr std::array GBUFFER_ALBEDO_FORMATS = {VK_FORMAT_B8G8R8A8_SRGB};
    constexpr std::array GBUFFER_POSITION_FORMATS = {VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                     VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};
    constexpr std::array GBUFFER_NORMAL_FORMATS = {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_R16G16B16_SFLOAT,
                                                   VK_FORMAT_R16G16B16A16_SFLOAT};
    constexpr std::array GBUFFER_EMISSIVE_FORMATS = {VK_FORMAT_B10G11R11_UFLOAT_PACK32};
    constexpr std::array GBUFFER_METROUGH_FORMATS = {VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
    constexpr std::array GBUFFER_DEPTH_FORMATS = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
    constexpr std::array HDR_FORMATS = {VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};

    constexpr std::array TEXTYRE_ALBEDO_FORMATS = {VK_FORMAT_BC7_SRGB_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, VK_FORMAT_B8G8R8A8_SRGB};
    constexpr std::array TEXTURE_NORMAL_FORMATS = {VK_FORMAT_BC5_UNORM_BLOCK, VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                                   VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT};
    constexpr std::array TEXTURE_METROUGH_FORMATS = {VK_FORMAT_BC5_UNORM_BLOCK, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM,
                                                     VK_FORMAT_R8G8B8A8_UNORM};
    constexpr std::array TEXTURE_EMISSIVE_FORMATS = {VK_FORMAT_B10G11R11_UFLOAT_PACK32};
  } // namespace

  std::expected<void, std::string> Renderer::initFormats() {
    auto findFormat = [&](std::span<const VkFormat> candidates, VkFormatFeatureFlags features) -> VkFormat {
      for (auto& format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m.vkcore.device, format, &props);
        if ((props.optimalTilingFeatures & features) == features) {
          return format;
        }
      }
      return VK_FORMAT_UNDEFINED;
    };
    auto findColorAttachmentFormat = [&](std::span<const VkFormat> candidates) {
      return findFormat(candidates, VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT);
    };
    auto findDepthAttachmentFormat = [&](std::span<const VkFormat> candidates) {
      return findFormat(candidates, VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT);
    };
    auto findTextureFormat = [&](std::span<const VkFormat> candidates) {
      return findFormat(candidates, VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT);
    };

    m.formats.render = {
        .albedo = findColorAttachmentFormat(GBUFFER_ALBEDO_FORMATS),
        .position = findColorAttachmentFormat(GBUFFER_POSITION_FORMATS),
        .normal = findColorAttachmentFormat(GBUFFER_NORMAL_FORMATS),
        .emissive = findColorAttachmentFormat(GBUFFER_EMISSIVE_FORMATS),
        .metRought = findColorAttachmentFormat(GBUFFER_METROUGH_FORMATS),
        .depth = findDepthAttachmentFormat(GBUFFER_DEPTH_FORMATS),
        .hdr = findColorAttachmentFormat(HDR_FORMATS),
    };
    m.formats.texture = {
        .albedo = findTextureFormat(TEXTYRE_ALBEDO_FORMATS),
        .normal = findTextureFormat(TEXTURE_NORMAL_FORMATS),
        .metRough = findTextureFormat(TEXTURE_METROUGH_FORMATS),
        .emissive = findTextureFormat(TEXTURE_EMISSIVE_FORMATS),
    };
    m.formats.swapchain = m.vkcore.swapchain.config().format.format;

#define CHECK_FORMAT(format)                                                                                                               \
  if (!(format)) {                                                                                                                         \
    return std::unexpected("Failed to find suitable " #format " format.");                                                                 \
  }
    CHECK_FORMAT(m.formats.render.albedo);
    CHECK_FORMAT(m.formats.render.normal);
    CHECK_FORMAT(m.formats.render.emissive);
    CHECK_FORMAT(m.formats.render.metRought);
    CHECK_FORMAT(m.formats.render.depth);
    CHECK_FORMAT(m.formats.render.hdr);
    CHECK_FORMAT(m.formats.texture.albedo);
    CHECK_FORMAT(m.formats.texture.normal);
    CHECK_FORMAT(m.formats.texture.metRough);
    CHECK_FORMAT(m.formats.texture.emissive);
    CHECK_FORMAT(m.formats.swapchain);
    CHECK_FORMAT(m.formats.render.albedo);
    CHECK_FORMAT(m.formats.render.normal);
    CHECK_FORMAT(m.formats.render.emissive);
    CHECK_FORMAT(m.formats.render.metRought);

    VK_DEBUG("Selected formats:");
    VK_DEBUG("  Albedo: {}", m.formats.render.albedo);
    VK_DEBUG("  Normal: {}", m.formats.render.normal);
    VK_DEBUG("  Emissive: {}", m.formats.render.emissive);
    VK_DEBUG("  Metallic-Roughness: {}", m.formats.render.metRought);
    VK_DEBUG("  Depth: {}", m.formats.render.depth);
    VK_DEBUG("  HDR: {}", m.formats.render.hdr);
    VK_DEBUG("  Texture Albedo: {}", m.formats.texture.albedo);
    VK_DEBUG("  Texture Normal: {}", m.formats.texture.normal);
    VK_DEBUG("  Texture Metallic-Roughness: {}", m.formats.texture.metRough);
    VK_DEBUG("  Texture Emissive: {}", m.formats.texture.emissive);
    VK_DEBUG("  Swapchain: {}", m.formats.swapchain);

    return {};
  }
} // namespace kt::vkh
