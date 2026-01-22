#include "keptech/core/renderer.hpp"
#include "keptech/vulkan/helpers/descriptors.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "setup/core.hpp"

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

namespace keptech::vkh::setup {
  using namespace keptech::vkh;

  std::expected<Renderer::CameraObjects, std::string>
  createCameraObjects(const vk::raii::Device& device,
                      vma::Allocator& allocator) {
    vk::DescriptorPoolSize poolSize{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
    };
    VK_MAKE(descPool,
            device.createDescriptorPool(
                {.maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &poolSize}),
            "Faild to create camera "
            "descriptor pool.");

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, vk::DescriptorType::eUniformBuffer,
                             vk::ShaderStageFlagBits::eAll);
    VKH_MAKE(descLayout, layoutBuilder.build(device, nullptr),
             "Failed to create camera descriptor layout.");

    VK_MAKE(descSet,
            device.allocateDescriptorSets({.descriptorPool = *descPool,
                                           .descriptorSetCount = 1,
                                           .pSetLayouts = &*descLayout}),
            "Failed to allocate camera descriptor set.");

    VKH_MAKE(uniformBuffer,
             AllocatedBuffer::create(
                 allocator,
                 {
                     .size = sizeof(components::Camera::Uniforms),
                     .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                     .sharingMode = vk::SharingMode::eExclusive,
                 },
                 {
                     .flags = vma::AllocationCreateFlagBits::eMapped,
                     .usage = vma::MemoryUsage::eCpuToGpu,
                 }),
             "Failed to create camera uniform buffer.");

    DescriptorWriter descWriter{};

    descWriter.writeBuffer(0,
                           vk::DescriptorBufferInfo{
                               .buffer = uniformBuffer.buffer,
                               .offset = 0,
                               .range = sizeof(components::Camera::Uniforms),
                           },
                           DescriptorWriter::BufferType::Uniform);

    descWriter.update(device, *descSet.front());

    Renderer::CameraObjects cameraObjects{
        .layout = std::move(descLayout),
        .pool = std::move(descPool),
        .descriptorSet = std::move(descSet.front()),
        .uniformBuffer = uniformBuffer,
    };

    return std::move(cameraObjects);
  }
} // namespace keptech::vkh::setup

namespace keptech::vkh {
  using namespace keptech::vkh::setup;

  std::expected<Renderer, std::string>
  Renderer::create(const core::renderer::CreateInfo& createInfo,
                   const core::window::Window& window) {
    VKH_MAKE(vkcore, createVulkanCore(createInfo, window),
             "Failed to create Vulkan core.");

    VKH_MAKE(cameraObjects,
             createCameraObjects(vkcore.device.logical, vkcore.allocator),
             "Failed to create camera objects.");

    VKH_MAKE(imguiObjects, keptech::vkh::setup::setupImGui(window, vkcore),
             "Failed to create ImGui Vulkan objects.");

    VK_DEBUG("Vulkan renderer created successfully.");

    Renderer r{window, std::move(vkcore), std::move(imguiObjects),
               std::move(cameraObjects)};

    return std::move(r);
  }
} // namespace keptech::vkh
