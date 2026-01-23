#include "keptech/core/renderer.hpp"
#include "keptech/vulkan/helpers/descriptors.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "setup/core.hpp"

#include "descriptors.hpp"
#include "imgui.hpp"
#include "keptech/core/window.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <keptech/core/components/camera.hpp>
#include <keptech/core/maths/maths.hpp>

namespace keptech::vkh::setup {
  using namespace keptech::vkh;

  std::expected<AllocatedBuffer, std::string>
  createCameraObjects(const vk::raii::Device& device, vma::Allocator& allocator,
                      DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>& descriptors) {

    auto sizeSingleCameraUniform = sizeof(components::Camera::Uniforms);
    auto paddedSize =
        keptech::core::maths::roundToAlignment(sizeSingleCameraUniform, 256);
    auto totalSize = paddedSize + sizeSingleCameraUniform;

    VKH_MAKE(uniformBuffer,
             AllocatedBuffer::create(
                 allocator,
                 {
                     .size = totalSize,
                     .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                     .sharingMode = vk::SharingMode::eExclusive,
                 },
                 {
                     .flags = vma::AllocationCreateFlagBits::eMapped,
                     .usage = vma::MemoryUsage::eCpuToGpu,
                 }),
             "Failed to create camera uniform buffer.");

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      auto& descriptor = descriptors.sets[i];
      DescriptorWriter descWriter{};

      descWriter.writeBuffer(0,
                             vk::DescriptorBufferInfo{
                                 .buffer = uniformBuffer.buffer,
                                 .offset = i * paddedSize,
                                 .range = sizeof(components::Camera::Uniforms),
                             },
                             DescriptorWriter::BufferType::Uniform);

      descWriter.update(device, descriptor);
    }

    return uniformBuffer;
  }
} // namespace keptech::vkh::setup

namespace keptech::vkh {
  using namespace keptech::vkh::setup;

  std::expected<Renderer, std::string>
  Renderer::create(const core::renderer::CreateInfo& createInfo,
                   const core::window::Window& window) {
    VKH_MAKE(vkcore, createVulkanCore(createInfo, window),
             "Failed to create Vulkan core.");

    VKH_MAKE(gBuffer,
             createGBuffer(vkcore.allocator, vkcore.device.logical,
                           vkcore.device.physical,
                           vkcore.swapchain.config().extent),
             "Failed to create GBuffer.");

    VKH_MAKE(globalDescriptorSets,
             createGlobalDescriptors(vkcore.device.logical),
             "Failed to create global descriptor sets.");

    VKH_MAKE(cameraObjects,
             createCameraObjects(vkcore.device.logical, vkcore.allocator,
                                 globalDescriptorSets),
             "Failed to create camera objects.");

    VKH_MAKE(imguiObjects,
             keptech::vkh::setup::setupImGui(window, vkcore, gBuffer),
             "Failed to create ImGui Vulkan objects.");

    VK_DEBUG("Vulkan renderer created successfully.");

    Renderer r{
        window,        std::move(vkcore), std::move(imguiObjects),
        cameraObjects, gBuffer,           std::move(globalDescriptorSets)};

    return std::move(r);
  }
} // namespace keptech::vkh
