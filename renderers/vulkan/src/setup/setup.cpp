#include "keptech/vulkan/renderer.hpp"
#include "setup/core.hpp"

#include "descriptors.hpp"
#include "keptech/core/window.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "macros.hpp"
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <keptech/core/components/camera.hpp>
#include <keptech/core/maths/maths.hpp>

namespace keptech::vkh {
  using namespace keptech::vkh::setup;

  std::expected<RendererBackend, std::string>
  RendererBackend::create(const RendererCreateInfo& createInfo,
                          const core::window::Window& window) {
    VKH_MAKE(vkcore, createVulkanCore(createInfo, window),
             "Failed to create Vulkan core.");

    VKH_MAKE(globalDescriptorSets,
             createGlobalDescriptors(vkcore.device.logical),
             "Failed to create global descriptor sets.");

    VK_DEBUG("Vulkan renderer created successfully.");

    RendererBackend r{window, std::move(vkcore),
                      std::move(globalDescriptorSets)};

    return std::move(r);
  }
} // namespace keptech::vkh
